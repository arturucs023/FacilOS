/* t_cpp: bateria F32 C++ (v2.4.2). Verifica en Ring 3:
 * init global (orden), ctor/dtor, new/delete/new[], herencia +
 * virtual, templates, namespace, operador, estatico local (guardas),
 * interop C y codigo de salida. El dtor GLOBAL se verifica en el
 * harness (linea "dtor-global" en serie, corre tras main).
 * OJO C++: no definir __main (lo pone el runtime f32cxx). */
#include "ulib.h"
#include "sys.h"

static int pruebas = 0;
static int fallos = 0;

static void ok(int cond, const char *nom) {
    pruebas++;
    if (!cond) {
        fallos++;
        u_print("FAIL ");
    } else {
        u_print("ok ");
    }
    u_print(nom);
    u_print("\n");
}

/* ---- 1. init global con orden ---- */
static int orden[4];
static int norden = 0;

struct Reg {
    int id;
    Reg(int i) : id(i) {
        if (norden < 4) orden[norden++] = id;
    }
};

static Reg g1(1);
static Reg g2(2);

/* ---- 2/3. clases, herencia, virtual ---- */
class Base {
  public:
    int base_v;
    Base() : base_v(10) {}
    virtual int quien() const { return 1; }
    virtual ~Base() {}
};

class Hija : public Base {
  public:
    int extra;
    Hija() : extra(32) {}
    int quien() const { return 2; }
};

static int nd_total = 0;

struct Contador {
    Contador() { nd_total++; }
    ~Contador() { nd_total--; }
};

/* ---- dtor global visible en serie ---- */
struct Adios {
    ~Adios() { u_print("dtor-global\n"); }
};
static Adios adios;

/* ---- namespace + operador + template ---- */
namespace tt {
struct Par {
    int a, b;
    Par operator+(const Par &o) const {
        Par r = {a + o.a, b + o.b};
        return r;
    }
};
template <typename T> T triple(T v) { return v + v + v; }
template <typename T> struct Env {
    T v;
    explicit Env(T x) : v(x) {}
    T dos() const { return v + v; }
};
} // namespace tt

static int estatico_local_veces = 0;
static int llama_una_vez(void) {
    static int puesto = 0;
    if (!puesto) {
        puesto = 1;
        estatico_local_veces++;
    }
    return puesto;
}

int main(void) {
    int cs;
    __asm__ volatile("mov %%cs,%0" : "=r"(cs));
    ok((cs & 3) == 3, "anillo3");

    /* init global corrio antes de main, en orden */
    ok(norden == 2 && orden[0] == 1 && orden[1] == 2, "init-global-orden");

    /* ctor/dtor local */
    ok(nd_total == 0, "ctor-cero");
    {
        Contador c1, c2;
        ok(nd_total == 2, "ctor-local");
    }
    ok(nd_total == 0, "dtor-local");

    /* new/delete */
    int *p = new int(1234);
    ok(p && *p == 1234, "new-int");
    *p = 777;
    ok(*p == 777, "new-escritura");
    delete p;
    int *q = new int[8];
    ok(q != 0, "new-array");
    for (int i = 0; i < 8; i++) q[i] = i * 3;
    ok(q[0] == 0 && q[7] == 21, "new-array-datos");
    delete[] q;
    Base *b = new Hija();
    ok(b->quien() == 2 && b->base_v == 10 &&
           ((Hija *)b)->extra == 32,
       "herencia-virtual");
    delete b;

    /* namespace, operador, templates */
    tt::Par x = {1, 2}, y = {10, 20};
    tt::Par z = x + y;
    ok(z.a == 11 && z.b == 22, "operador-mas");
    ok(tt::triple(7) == 21, "template-func");
    tt::Env<int> e(19);
    ok(e.dos() == 38, "template-clase");
    int &ref = z.a;
    ref = 5;
    ok(z.a == 5, "referencia");

    /* estatico local: la guarda corre una vez */
    llama_una_vez();
    llama_una_vez();
    ok(estatico_local_veces == 1, "estatico-local-guarda");

    /* interop C */
    void *m = u_malloc(64);
    ok(m != 0, "interop-malloc");
    u_strcpy((char *)m, "abc");
    ok(u_strcmp((char *)m, "abc") == 0, "interop-cadenas");
    u_free(m);
    ok(sys_ticks() >= 0, "interop-ticks");

    u_print("t_cpp: ");
    u_print_num(pruebas);
    u_print(" pruebas ");
    u_print_num(fallos);
    u_print(" fallos\n");
    return fallos;
}
