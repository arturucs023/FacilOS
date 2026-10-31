#include "ktimer.h"
#include "cpu.h"
#include "libc.h"

#define KT_MAX 16

typedef struct {
    int activo;
    int periodico;
    uint32_t falta;
    uint32_t periodo;
    kt_fn cb;
    void *arg;
} kt_t;

static kt_t tabla[KT_MAX];

static int alta(uint32_t ticks, int periodico, kt_fn cb, void *arg) {
    uint32_t fl;
    int id = -1;
    if (!ticks || !cb) return -1;
    fl = cpu_eflags();
    cli();
    for (int i = 0; i < KT_MAX; i++) {
        if (!tabla[i].activo) { id = i; break; }
    }
    if (id >= 0) {
        tabla[id].activo = 1;
        tabla[id].periodico = periodico;
        tabla[id].falta = ticks;
        tabla[id].periodo = ticks;
        tabla[id].cb = cb;
        tabla[id].arg = arg;
    }
    cpu_if_restaura(fl);
    return id;
}

int kt_cada(uint32_t ticks, kt_fn cb, void *arg) {
    return alta(ticks, 1, cb, arg);
}

int kt_una(uint32_t ticks, kt_fn cb, void *arg) {
    return alta(ticks, 0, cb, arg);
}

void kt_cancela(int id) {
    uint32_t fl;
    if (id < 0 || id >= KT_MAX) return;
    fl = cpu_eflags();
    cli();
    tabla[id].activo = 0;
    tabla[id].cb = 0;
    cpu_if_restaura(fl);
}

void kt_tick(void) {
    /* Corre en IRQ: solo descuenta y dispara callbacks triviales. */
    for (int i = 0; i < KT_MAX; i++) {
        kt_fn cb;
        void *arg;
        if (!tabla[i].activo) continue;
        if (--tabla[i].falta > 0) continue;
        cb = tabla[i].cb;
        arg = tabla[i].arg;
        if (tabla[i].periodico) tabla[i].falta = tabla[i].periodo;
        else tabla[i].activo = 0;
        if (cb) cb(arg);
    }
}

static volatile uint32_t marca_una = 0;
static volatile uint32_t cuenta_per = 0;

static void cb_una(void *a) {
    (void)a;
    marca_una = 1;
}

static void cb_per(void *a) {
    (void)a;
    cuenta_per++;
}

int kt_autoprueba(void) {
    /* Un disparo a 5 ticks + periodico cada 10: espera activa con hlt
       (los IRQ siguen entrando) hasta ver ambos, con tope de 200 ticks.
       Llamar con IF=1; lo conserva. */
    uint32_t t0;
    int id_p;
    marca_una = 0;
    cuenta_per = 0;
    if (kt_una(5, cb_una, 0) < 0) return -1;
    id_p = kt_cada(10, cb_per, 0);
    if (id_p < 0) return -2;
    t0 = 0;
    for (;;) {
        uint32_t m, c;
        uint32_t fl = cpu_eflags();
        sti();
        hlt();
        cli();
        m = marca_una;
        c = cuenta_per;
        cpu_if_restaura(fl);
        if (m && c >= 3) break;
        if (++t0 > 300) {
            kt_cancela(id_p);
            return -3;
        }
    }
    kt_cancela(id_p);
    return 0;
}
