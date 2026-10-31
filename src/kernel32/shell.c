#include "shell.h"
#include "libc.h"
#include "cpu.h"
#include "irq.h"
#include "teclado.h"
#include "fs.h"
#include "seg.h"
#include "exec.h"
extern int exec_base_dir;

#define LIN_MAX 128
#define HIST_N 20

static char hist[HIST_N][LIN_MAX];
static int hist_n = 0;
extern int teclado_es;

static void pon_numero(uint32_t t) {
    char nb[12];
    int ni = 0;
    if (!t) nb[ni++] = '0';
    else {
        char rv[12];
        int ri = 0;
        while (t && ri < 11) { rv[ri++] = (char)('0' + t % 10); t /= 10; }
        while (ri) nb[ni++] = rv[--ri];
    }
    nb[ni] = 0;
    cons_puts(nb);
}

static void ruta_str(int dir, char *out) {
    int pila[32];
    int p = 0, d = dir;
    while (d && p < 32) { pila[p++] = d; d = fs_padre(d); }
    int n = 0;
    out[n++] = '/';
    for (int i = p - 1; i >= 0 && n < LIN_MAX - 34; i--) {
        const char *nm = fs_nombre(pila[i]);
        for (int j = 0; nm[j] && n < LIN_MAX - 2; j++) out[n++] = nm[j];
        out[n++] = '/';
    }
    out[n] = 0;
    if (n > 1) out[n - 1] = 0;
}

static void pon_pront(void) {
    char r[LIN_MAX];
    ruta_str(fs_actual(), r);
    cons_puts("facil32:");
    cons_puts(r);
    cons_puts("> ");
}

static int lee_linea(char *buf) {
    int len = 0, hpos = hist_n;
    for (;;) {
        int t = kbd_tecla();
        if (t == 13) {
            cons_putc('\n');
            buf[len] = 0;
            return len;
        } else if (t == 8) {
            if (len > 0) {
                len--;
                if (cons_gfx()) cons_putc('\b');
                else vga_puts("\b \b");
                uart_puts("\b \b");
            }
        } else if (t == TEC_ARRIBA || t == TEC_ABAJO) {
            if (!hist_n) continue;
            if (t == TEC_ARRIBA && hpos > 0) hpos--;
            if (t == TEC_ABAJO && hpos < hist_n) hpos++;
            while (len > 0) {
                len--;
                if (cons_gfx()) cons_putc('\b');
                else vga_puts("\b \b");
            }
            uart_putc('\r');
            if (hpos < hist_n) {
                const char *h = hist[hpos];
                while (*h && len < LIN_MAX - 1) {
                    buf[len++] = *h;
                    cons_putc(*h);
                    h++;
                }
            }
        } else if (t >= 32 && t < 127) {
            if (len < LIN_MAX - 1) {
                buf[len++] = (char)t;
                cons_putc((char)t);
            }
        }
    }
}

static void hist_guarda(const char *l) {
    if (!l[0]) return;
    if (hist_n < HIST_N) {
        libc_strcpy(hist[hist_n++], l);
    } else {
        for (int i = 0; i < HIST_N - 1; i++)
            libc_strcpy(hist[i], hist[i + 1]);
        libc_strcpy(hist[HIST_N - 1], l);
    }
}

static void parte(const char *ruta, int *dir, char *nom) {
    char tmp[LIN_MAX];
    libc_strcpy(tmp, ruta);
    int l = libc_strlen(tmp);
    while (l > 1 && tmp[l - 1] == '/') tmp[--l] = 0;
    char *sep = 0;
    for (int i = l - 1; i >= 0; i--) {
        if (tmp[i] == '/') { sep = tmp + i; break; }
    }
    char ndir[LIN_MAX];
    if (!sep) {
        libc_strcpy(ndir, ".");
        libc_strcpy(nom, tmp);
    } else if (sep == tmp) {
        libc_strcpy(ndir, "/");
        libc_strcpy(nom, sep + 1);
    } else {
        *sep = 0;
        libc_strcpy(ndir, tmp);
        libc_strcpy(nom, sep + 1);
    }
    *dir = -1;
    if (!nom[0]) return;
    *dir = fs_resuelve(ndir);
}

static int autoguardar(void) {
    if (fs_migrar || fs_disco_ok) {
        if (fs_guardar()) {
            cons_puts("Error al guardar.\n");
            return -1;
        }
    }
    return 0;
}

static uint8_t cmos(uint8_t r) {
    outb(0x70, (uint8_t)(0x80 | r));
    return inb(0x71);
}
static int bcd(int b) { return ((b >> 4) * 10) + (b & 15); }

static void cmd_fecha(void) {
    for (int i = 0; i < 100000; i++) {
        if (!(cmos(0x0A) & 0x80)) break;
    }
    char b[32];
    int vals[6] = { bcd(cmos(7)), bcd(cmos(8)), bcd(cmos(9)),
                    bcd(cmos(4)), bcd(cmos(2)), bcd(cmos(0)) };
    int n = 0;
    const char seps[6] = { '/', '/', ' ', ':', ':', 0 };
    for (int k = 0; k < 6; k++) {
        b[n++] = (char)('0' + vals[k] / 10);
        b[n++] = (char)('0' + vals[k] % 10);
        b[n++] = seps[k];
    }
    if (n) b[n - 1] = 0;
    cons_puts(b);
    cons_putc('\n');
}

static void cmd_ver(const char *args) {
    char a1[LIN_MAX], a2[LIN_MAX];
    a1[0] = 0; a2[0] = 0;
    const char *p = args;
    while (*p == ' ') p++;
    int i = 0;
    while (*p && *p != ' ' && i < LIN_MAX - 1) a1[i++] = *p++;
    a1[i] = 0;
    while (*p == ' ') p++;
    i = 0;
    while (*p && *p != ' ' && i < LIN_MAX - 1) a2[i++] = *p++;
    a2[i] = 0;
    int modo = 0, dir = fs_actual();
    const char *pr = 0;
    if (a1[0] == '-' && !a2[0]) { libc_strcpy(a2, a1); a1[0] = 0; }
    if (a2[0]) {
        if (!libc_strcmp(a2, "--todo")) modo = 3;
        else if (!libc_strcmp(a2, "--carpetas")) modo = 1;
        else if (!libc_strcmp(a2, "--archivos")) modo = 2;
        if (a1[0]) pr = a1;
    } else if (a1[0]) {
        pr = a1;
    }
    if (pr) {
        int d = fs_resuelve(pr);
        if (d < 0) { cons_puts("Ruta no valida.\n"); return; }
        dir = d;
    }
    if (modo == 3) {
        for (int k = 1; k < 64; k++) {
            if (!fs_tipo(k) || fs_padre(k) != dir) continue;
            if (fs_tipo(k) == 1) {
                cons_puts("  <carpeta> ");
                cons_puts(fs_nombre(k));
            } else {
                cons_puts("  <archivo> ");
                cons_puts(fs_nombre(k));
                cons_puts(" (");
                pon_numero(fs_tam(k));
                cons_puts(")");
            }
            cons_putc('\n');
        }
        return;
    }
    if (!modo || modo == 1) {
        cons_puts("CARPETAS:\n");
        for (int k = 1; k < 64; k++) {
            if (fs_tipo(k) == 1 && fs_padre(k) == dir) {
                cons_puts(" ");
                cons_puts(fs_nombre(k));
                cons_putc('\n');
            }
        }
    }
    if (!modo || modo == 2) {
        cons_puts("ARCHIVOS:\n");
        for (int k = 1; k < 64; k++) {
            if (fs_tipo(k) == 2 && fs_padre(k) == dir) {
                cons_puts(" ");
                cons_puts(fs_nombre(k));
                cons_putc('\n');
            }
        }
        /* v1.8: programas del catalogo de la ISO (solo lectura). */
        {
            extern int cdcat_cuantos(void);
            extern const char *cdcat_nombre(int);
            extern int cdcat_tam(int);
            int nc = dir == 0 ? cdcat_cuantos() : 0;
            for (int k = 0; k < nc; k++) {
                cons_puts(" <cd> ");
                cons_puts(cdcat_nombre(k));
                cons_puts(" (");
                pon_numero((uint32_t)cdcat_tam(k));
                cons_puts(")\n");
            }
        }
    }
}

static void cmd_espacio(void) {
    int oc = 0, arch = 0, carp = 0;
    uint32_t bytes = 0;
    for (int k = 0; k < 64; k++) {
        if (!fs_tipo(k)) continue;
        oc++;
        if (fs_tipo(k) == 1) carp++;
        else { arch++; bytes += fs_tam(k); }
    }
    cons_puts("Espacio en disco:\n  Ocupados: ");
    pon_numero((uint32_t)oc);
    cons_puts("\n  Libres: ");
    pon_numero((uint32_t)(64 - oc));
    cons_puts("\n  Archivos: ");
    pon_numero((uint32_t)arch);
    cons_puts("\n  Carpetas: ");
    pon_numero((uint32_t)carp);
    cons_puts("\n  Bytes en archivos: ");
    pon_numero(bytes);
    cons_putc('\n');
}

static void cmd_editar(const char *nom) {
    int dir;
    char base[LIN_MAX];
    parte(nom, &dir, base);
    if (dir < 0) { cons_puts("Ruta no valida.\n"); return; }
    int idx = fs_hijo(dir, base);
    static uint8_t buf[512];
    uint32_t tam = 0;
    if (idx >= 0) {
        if (fs_tipo(idx) != 2) { cons_puts("No es archivo.\n"); return; }
        if (fs_tam(idx) > 511) {
            cons_puts("Muy largo para el editor interno (max 511).\n");
            return;
        }
        int r = fs_leer(idx, buf);
        if (r < 0) { cons_puts("Error al leer.\n"); return; }
        tam = (uint32_t)r;
        cons_puts("Editando (escribe :guardar para guardar, :salir sin guardar).\n");
        for (uint32_t i = 0; i < tam; i++) cons_putc((char)buf[i]);
        if (tam) cons_putc('\n');
    } else {
        cons_puts("Editando nuevo (escribe :guardar para guardar, :salir sin guardar).\n");
    }
    char lin[LIN_MAX];
    for (;;) {
        cons_puts("> ");
        lee_linea(lin);
        if (!libc_strcmp(lin, ":guardar")) {
            if (idx < 0) {
                idx = fs_crear(2, dir, base);
                if (idx < 0) { cons_puts("No se pudo crear.\n"); return; }
            }
            if (fs_escribir(idx, buf, tam)) { cons_puts("Muy largo.\n"); return; }
            autoguardar();
            cons_puts("Archivo guardado.\n");
            return;
        }
        if (!libc_strcmp(lin, ":salir")) {
            cons_puts("Edicion cancelada.\n");
            return;
        }
        for (int i = 0; lin[i] && tam < 511; i++) buf[tam++] = (uint8_t)lin[i];
        if (tam < 511) buf[tam++] = '\n';
        else { cons_puts("Lleno (512).\n"); return; }
    }
}

static void ayuda_cmd(const char *c) {
    if (!c[0] || !libc_strcmp(c, "--todo")) {
        cons_puts("ayuda ver_archivos entrar_carpeta crear_carpeta crear_archivo\n"
                  "escribir_archivo leer_archivo copiar_archivo mover_archivo\n"
                  "renombrar_archivo buscar_texto editar_archivo borrar_archivo\n"
                  "borrar_carpeta donde_estoy limpiar eco fecha quien_soy\n"
                  "espacio_disco guardar historial teclado version ejecutar\n"
                  "menu salir\n");
        return;
    }
    cons_puts("usa: comando [args] (sin ayuda detallada (v1.8))\n");
}

void shell(void) {
    char lin[LIN_MAX], cmd[LIN_MAX], args[LIN_MAX];
    fs_init();
    if (fs_disco_ok == 1) cons_puts("Disco de datos listo. Archivos recuperados.\n");
    else if (fs_disco_ok == 2) cons_puts("Disco v1.1 detectado. Se migrara a v1.2 al usar guardar.\n");
    else cons_puts("Sin disco de datos (v1.8: los programas vienen de la ISO). "
                  "Memoria temporal.\n");
    cons_puts("FacilOS32 shell. Escribe ayuda.\n");
    for (;;) {
        pon_pront();
        lee_linea(lin);
        hist_guarda(lin);
        const char *p = lin;
        while (*p == ' ') p++;
        int i = 0;
        while (*p && *p != ' ' && i < LIN_MAX - 1) cmd[i++] = *p++;
        cmd[i] = 0;
        while (*p == ' ') p++;
        libc_strcpy(args, p);
        if (!cmd[0]) continue;
        if (!libc_strcmp(cmd, "ayuda")) {
            char a1[LIN_MAX];
            int j = 0;
            while (args[j] && args[j] != ' ' && j < LIN_MAX - 1) { a1[j] = args[j]; j++; }
            a1[j] = 0;
            ayuda_cmd(a1);
        } else if (!libc_strcmp(cmd, "version")) {
            cons_puts("FacilOS32 version 2.0 - Shell 32-bit en C.\n");
        } else if (!libc_strcmp(cmd, "quien_soy")) {
            cons_puts("Eres invitado, usuario de FacilOS32.\n");
        } else if (!libc_strcmp(cmd, "eco")) {
            cons_puts(args);
            cons_putc('\n');
        } else if (!libc_strcmp(cmd, "limpiar")) {
            cons_clear();
        } else if (!libc_strcmp(cmd, "fecha")) {
            cmd_fecha();
        } else if (!libc_strcmp(cmd, "donde_estoy")) {
            char r[LIN_MAX];
            ruta_str(fs_actual(), r);
            cons_puts(r);
            cons_putc('\n');
        } else if (!libc_strcmp(cmd, "historial")) {
            for (int h = 0; h < hist_n; h++) {
                cons_puts(hist[h]);
                cons_putc('\n');
            }
        } else if (!libc_strcmp(cmd, "teclado")) {
            if (!args[0]) {
                cons_puts(teclado_es ? "Teclado actual: espanol.\n" : "Teclado actual: americano.\n");
            } else if (!libc_strcmp(args, "es")) {
                teclado_pon(1);
                autoguardar();
                cons_puts("Teclado espanol (ES) activado.\n");
            } else if (!libc_strcmp(args, "us")) {
                teclado_pon(0);
                autoguardar();
                cons_puts("Teclado americano (US) activado.\n");
            } else {
                cons_puts("Usa: teclado es|us\n");
            }
        } else if (!libc_strcmp(cmd, "ver_archivos")) {
            cmd_ver(args);
        } else if (!libc_strcmp(cmd, "entrar_carpeta")) {
            if (!args[0]) { cons_puts("Falta nombre.\n"); continue; }
            int d = fs_resuelve(args);
            if (d < 0) { cons_puts("No existe con ese nombre.\n"); continue; }
            fs_pon_actual(d);
        } else if (!libc_strcmp(cmd, "crear_carpeta")) {
            if (!args[0]) { cons_puts("Falta nombre.\n"); continue; }
            int dir;
            char base[LIN_MAX];
            parte(args, &dir, base);
            if (dir < 0) { cons_puts("Ruta no valida.\n"); continue; }
            int r = fs_crear(1, dir, base);
            if (r == -2) cons_puts("Ya existe con ese nombre.\n");
            else if (r == -3) cons_puts("No hay espacio (max 64 cosas).\n");
            else if (r < 0) cons_puts("Nombre no valido.\n");
            else {
                autoguardar();
                cons_puts("Carpeta creada.\n");
            }
        } else if (!libc_strcmp(cmd, "crear_archivo")) {
            if (!args[0]) { cons_puts("Falta nombre.\n"); continue; }
            int dir;
            char base[LIN_MAX];
            parte(args, &dir, base);
            if (dir < 0) { cons_puts("Ruta no valida.\n"); continue; }
            int r = fs_crear(2, dir, base);
            if (r == -2) cons_puts("Ya existe con ese nombre.\n");
            else if (r == -3) cons_puts("No hay espacio (max 64 cosas).\n");
            else if (r < 0) cons_puts("Nombre no valido.\n");
            else {
                autoguardar();
                cons_puts("Archivo creado.\n");
            }
        } else if (!libc_strcmp(cmd, "escribir_archivo")) {
            char nm[LIN_MAX];
            int j = 0;
            const char *q = args;
            while (*q && *q != ' ' && j < LIN_MAX - 1) nm[j++] = *q++;
            nm[j] = 0;
            while (*q == ' ') q++;
            int agregar = 0;
            if (!libc_strncmp(q, "--agregar ", 10)) { agregar = 1; q += 10; }
            else if (!libc_strcmp(q, "--agregar")) { agregar = 1; q += 9; }
            if (!nm[0] || !q[0]) { cons_puts("Falta nombre o texto.\n"); continue; }
            int dir;
            char base[LIN_MAX];
            parte(nm, &dir, base);
            if (dir < 0) { cons_puts("Ruta no valida.\n"); continue; }
            int idx = fs_hijo(dir, base);
            if (idx < 0 || fs_tipo(idx) != 2) { cons_puts("No existe con ese nombre.\n"); continue; }
            static uint8_t buf[512];
            uint32_t tam = 0;
            if (agregar) {
                int r = fs_leer(idx, buf);
                if (r < 0) { cons_puts("Error al leer.\n"); continue; }
                tam = (uint32_t)r;
            }
            uint32_t tl = libc_strlen(q);
            if (tam + tl > 512) { cons_puts("Muy largo (max 512).\n"); continue; }
            libc_memcpy(buf + tam, q, tl);
            tam += tl;
            if (fs_escribir(idx, buf, tam)) { cons_puts("Error al escribir.\n"); continue; }
            autoguardar();
            cons_puts("Guardado.\n");
        } else if (!libc_strcmp(cmd, "leer_archivo")) {
            if (!args[0]) { cons_puts("Falta nombre.\n"); continue; }
            int dir;
            char base[LIN_MAX];
            parte(args, &dir, base);
            if (dir < 0) { cons_puts("Ruta no valida.\n"); continue; }
            int idx = fs_hijo(dir, base);
            if (idx < 0 || fs_tipo(idx) != 2) {
                /* v1.8: puede estar en el catalogo de la ISO. */
                extern int cdcat_buscar(const char *);
                extern int cdcat_leer(int, uint32_t, void *, uint32_t);
                extern int cdcat_tam(int);
                int ci = cdcat_buscar(base);
                if (ci < 0) {
                    cons_puts("No existe con ese nombre.\n");
                    continue;
                }
                {
                    static uint8_t cb[512];
                    int t = cdcat_tam(ci);
                    uint32_t off = 0;
                    while (off < (uint32_t)t) {
                        uint32_t tro = (uint32_t)t - off > 512 ? 512 :
                                       (uint32_t)t - off;
                        int le = cdcat_leer(ci, off, cb, tro);
                        if (le <= 0) break;
                        for (int k = 0; k < le; k++) cons_putc((char)cb[k]);
                        off += (uint32_t)le;
                    }
                    cons_putc('\n');
                }
                continue;
            }
            static uint8_t buf[512];
            uint32_t t = fs_tam(idx), off = 0;
            int mal = 0;
            while (off < t) {
                uint32_t trozo = t - off > 512 ? 512 : t - off;
                int r = fs_leer_parcial(idx, off, buf, trozo);
                if (r <= 0) { mal = 1; break; }
                for (int k = 0; k < r; k++) cons_putc((char)buf[k]);
                off += (uint32_t)r;
            }
            if (mal) { cons_puts("\nError al leer.\n"); continue; }
            cons_putc('\n');
        } else if (!libc_strcmp(cmd, "borrar_archivo")) {
            if (!args[0]) { cons_puts("Falta nombre.\n"); continue; }
            int dir;
            char base[LIN_MAX];
            parte(args, &dir, base);
            if (dir < 0) { cons_puts("Ruta no valida.\n"); continue; }
            int idx = fs_hijo(dir, base);
            if (idx < 0 || fs_tipo(idx) != 2) { cons_puts("No existe con ese nombre.\n"); continue; }
            fs_borrar(idx);
            autoguardar();
            cons_puts("Borrado.\n");
        } else if (!libc_strcmp(cmd, "borrar_carpeta")) {
            char nm[LIN_MAX];
            int forzar = 0;
            int j = 0;
            const char *q = args;
            while (*q && *q != ' ' && j < LIN_MAX - 1) nm[j++] = *q++;
            nm[j] = 0;
            while (*q == ' ') q++;
            if (!libc_strcmp(q, "--forzar")) forzar = 1;
            if (!nm[0]) { cons_puts("Falta nombre.\n"); continue; }
            int dir;
            char base[LIN_MAX];
            parte(nm, &dir, base);
            if (dir < 0) { cons_puts("Ruta no valida.\n"); continue; }
            int idx = fs_hijo(dir, base);
            if (idx < 0 || fs_tipo(idx) != 1) { cons_puts("No existe con ese nombre.\n"); continue; }
            int r = fs_borrar(idx);
            if (r == -2 && !forzar) {
                cons_puts("Carpeta no vacia. Usa --forzar para borrar todo.\n");
                continue;
            }
            if (r == -2 && forzar) {
                for (int k = 63; k >= 0; k--) {
                    if (fs_tipo(k) && fs_padre(k) == idx) fs_borrar(k);
                }
                fs_borrar(idx);
            }
            autoguardar();
            cons_puts("Borrado.\n");
        } else if (!libc_strcmp(cmd, "copiar_archivo")) {
            char o[LIN_MAX], d[LIN_MAX];
            int j = 0;
            const char *q = args;
            while (*q && *q != ' ' && j < LIN_MAX - 1) o[j++] = *q++;
            o[j] = 0;
            while (*q == ' ') q++;
            j = 0;
            while (*q && *q != ' ' && j < LIN_MAX - 1) d[j++] = *q++;
            d[j] = 0;
            if (!o[0] || !d[0]) { cons_puts("Faltan nombres.\n"); continue; }
            int do_;
            char bo[LIN_MAX];
            parte(o, &do_, bo);
            if (do_ < 0) { cons_puts("Ruta no valida.\n"); continue; }
            int io = fs_hijo(do_, bo);
            if (io < 0 || fs_tipo(io) != 2) { cons_puts("No existe con ese nombre.\n"); continue; }
            int dd = fs_resuelve(d);
            int id = -1;
            if (dd >= 0 && fs_tipo(dd) == 1) {
                id = fs_crear(2, dd, bo);
            } else {
                int ddx;
                char bd[LIN_MAX];
                parte(d, &ddx, bd);
                if (ddx < 0) { cons_puts("Ruta no valida.\n"); continue; }
                id = fs_crear(2, ddx, bd);
            }
            if (id == -2) { cons_puts("Ya existe con ese nombre.\n"); continue; }
            if (id < 0) { cons_puts("No hay espacio (max 64 cosas).\n"); continue; }
            static uint8_t buf[512];
            uint32_t t = fs_tam(io);
            int mal = fs_truncar(id, t) ? 1 : 0;
            for (uint32_t off = 0; off < t && !mal; ) {
                uint32_t trozo = t - off > 512 ? 512 : t - off;
                int r = fs_leer_parcial(io, off, buf, trozo);
                if (r <= 0) { mal = 1; break; }
                if (fs_escribir_parcial(id, off, buf, (uint32_t)r)) mal = 1;
                off += (uint32_t)r;
            }
            if (mal) { fs_borrar(id); autoguardar(); cons_puts("Error al copiar.\n"); continue; }
            autoguardar();
            cons_puts("Archivo copiado.\n");
        } else if (!libc_strcmp(cmd, "mover_archivo")) {
            char o[LIN_MAX], d[LIN_MAX];
            int j = 0;
            const char *q = args;
            while (*q && *q != ' ' && j < LIN_MAX - 1) o[j++] = *q++;
            o[j] = 0;
            while (*q == ' ') q++;
            j = 0;
            while (*q && *q != ' ' && j < LIN_MAX - 1) d[j++] = *q++;
            d[j] = 0;
            if (!o[0] || !d[0]) { cons_puts("Faltan nombres.\n"); continue; }
            int do_;
            char bo[LIN_MAX];
            parte(o, &do_, bo);
            if (do_ < 0) { cons_puts("Ruta no valida.\n"); continue; }
            int io = fs_hijo(do_, bo);
            if (io < 0 || !fs_tipo(io)) { cons_puts("No existe con ese nombre.\n"); continue; }
            int r;
            int dd = fs_resuelve(d);
            if (dd >= 0 && fs_tipo(dd) == 1) {
                r = fs_mover(io, dd, 0);
            } else {
                int ddx;
                char bd[LIN_MAX];
                parte(d, &ddx, bd);
                if (ddx < 0) { cons_puts("Ruta no valida.\n"); continue; }
                r = fs_mover(io, ddx, bd);
            }
            if (r == -2) cons_puts("Ya existe con ese nombre.\n");
            else if (r) cons_puts("No se pudo mover.\n");
            else {
                autoguardar();
                cons_puts("Archivo movido.\n");
            }
        } else if (!libc_strcmp(cmd, "renombrar_archivo")) {
            char a[LIN_MAX], b[LIN_MAX];
            int j = 0;
            const char *q = args;
            while (*q && *q != ' ' && j < LIN_MAX - 1) a[j++] = *q++;
            a[j] = 0;
            while (*q == ' ') q++;
            j = 0;
            while (*q && *q != ' ' && j < LIN_MAX - 1) b[j++] = *q++;
            b[j] = 0;
            if (!a[0] || !b[0]) { cons_puts("Faltan nombres.\n"); continue; }
            int da;
            char ba[LIN_MAX];
            parte(a, &da, ba);
            if (da < 0) { cons_puts("Ruta no valida.\n"); continue; }
            int ia = fs_hijo(da, ba);
            if (ia < 0) { cons_puts("No existe con ese nombre.\n"); continue; }
            const char *nuevo = b;
            char bb[LIN_MAX];
            int has_slash = 0;
            for (int k = 0; b[k]; k++) if (b[k] == '/') has_slash = 1;
            if (has_slash) {
                int db;
                parte(b, &db, bb);
                if (db != da) { cons_puts("Solo dentro de la misma carpeta.\n"); continue; }
                nuevo = bb;
            }
            int r = fs_renombrar(ia, nuevo);
            if (r == -2) cons_puts("Ya existe con ese nombre.\n");
            else if (r) cons_puts("Nombre no valido.\n");
            else {
                autoguardar();
                cons_puts("Archivo renombrado.\n");
            }
        } else if (!libc_strcmp(cmd, "buscar_texto")) {
            char a[LIN_MAX];
            int j = 0;
            const char *q = args;
            while (*q && *q != ' ' && j < LIN_MAX - 1) a[j++] = *q++;
            a[j] = 0;
            while (*q == ' ') q++;
            if (!a[0] || !q[0]) { cons_puts("Faltan datos.\n"); continue; }
            int dir;
            char base[LIN_MAX];
            parte(a, &dir, base);
            if (dir < 0) { cons_puts("Ruta no valida.\n"); continue; }
            int idx = fs_hijo(dir, base);
            if (idx < 0 || fs_tipo(idx) != 2) { cons_puts("No existe con ese nombre.\n"); continue; }
            static uint8_t win[640];
            uint32_t lp = libc_strlen(q);
            uint32_t t = fs_tam(idx);
            if (lp > 127) { cons_puts("Texto muy largo.\n"); continue; }
            int pos = -1, mal = 0;
            uint32_t off = 0, carry = 0;
            while (off < t && pos < 0 && !mal) {
                uint32_t trozo = t - off > 512 ? 512 : t - off;
                int r = fs_leer_parcial(idx, off, win + carry, trozo);
                if (r <= 0) { mal = 1; break; }
                uint32_t total = carry + (uint32_t)r;
                for (uint32_t k = 0; k + lp <= total; k++) {
                    if (!libc_memcmp(win + k, q, lp)) {
                        pos = (int)(off - carry + k);
                        break;
                    }
                }
                carry = total < lp ? total : lp - 1;
                if (carry) libc_memcpy(win, win + total - carry, carry);
                off += (uint32_t)r;
            }
            if (mal) {
                cons_puts("Error al leer.\n");
            } else if (pos < 0) {
                cons_puts("Texto no encontrado.\n");
            } else {
                cons_puts("Texto encontrado en la posicion ");
                pon_numero((uint32_t)pos);
                cons_putc('\n');
            }
        } else if (!libc_strcmp(cmd, "editar_archivo")) {
            if (!args[0]) { cons_puts("Falta nombre.\n"); continue; }
            cmd_editar(args);
        } else if (!libc_strcmp(cmd, "espacio_disco")) {
            cmd_espacio();
        } else if (!libc_strcmp(cmd, "ejecutar")) {
            if (!args[0]) { cons_puts("Falta nombre.\n"); continue; }
            char pg[LIN_MAX], ar[LIN_MAX];
            int j = 0;
            const char *q = args;
            while (*q && *q != ' ' && j < LIN_MAX - 1) pg[j++] = *q++;
            pg[j] = 0;
            while (*q == ' ') q++;
            libc_strcpy(ar, q);
            int dir;
            char base[LIN_MAX];
            parte(pg, &dir, base);
            if (dir < 0) { cons_puts("Ruta no valida.\n"); continue; }
            exec_base_dir = fs_actual();
            int idx = fs_hijo(dir, base);
            int en_cd = 0;
            if (idx < 0 || fs_tipo(idx) != 2) {
                /* v1.8: los programas viven en el catalogo de la ISO. */
                extern int cdcat_buscar(const char *);
                en_cd = cdcat_buscar(base) >= 0;
                if (!en_cd) { cons_puts("No existe con ese nombre.\n"); continue; }
            }
            {
                int rc = kern_ejecutar(dir, base, ar);
                if (rc == -100) {
                    cons_puts("No existe con ese nombre.\n");
                } else if (rc == -101) {
                    if (en_cd) {
                        cons_puts("No es ejecutable F32X.\n");
                    } else {
                        uint32_t fr = fs_tam(fs_hijo(dir, base));
                        if (fr < 16) cons_puts("No es ejecutable.\n");
                        else cons_puts("No es ejecutable F32X.\n");
                    }
                } else if (rc == -102 || rc == -103) {
                    cons_puts(rc == -102 ? "Ejecutable corrupto.\n" : "Error al cargar.\n");
                } else if (rc < 0) {
                    cons_puts("programa abortado.\n");
                } else {
                    cons_puts("retorno: ");
                    pon_numero((uint32_t)rc);
                    cons_putc('\n');
                }
            }
        } else if (!libc_strcmp(cmd, "guardar")) {
            if (!fs_disco_ok) { cons_puts("Sin disco de datos. Nada que guardar.\n"); continue; }
            if (fs_guardar()) cons_puts("Error al guardar.\n");
            else cons_puts("Guardado en disco.\n");
        } else if (!libc_strcmp(cmd, "menu")) {
            if (shell_anidada) {
                cons_puts("Volviendo al menu...\n");
                return;
            }
            {
                int rc = kern_ejecutar(0, "menu", "");
                if (rc == -100 || rc == -101) {
                    cons_puts("Sin menu grafico en disco.\n");
                } else if (rc < 0) {
                    cons_puts("El menu fallo.\n");
                }
            }
        } else if (!libc_strcmp(cmd, "salir")) {
            cons_puts("Saliendo de la terminal...\n");
            return;
        } else {
            /* v1.8: si el nombre no es un comando, se prueba como programa
               del catalogo de la ISO (ejecutarlo sin "ejecutar"). */
            extern int cdcat_buscar(const char *);
            if (cdcat_buscar(cmd) >= 0) {
                exec_base_dir = fs_actual();
                int rc = kern_ejecutar(0, cmd, args);
                if (rc == -101) cons_puts("No es ejecutable F32X.\n");
                else if (rc == -102) cons_puts("Ejecutable corrupto.\n");
                else if (rc == -103) cons_puts("Error al cargar.\n");
                else if (rc < 0) cons_puts("programa abortado.\n");
                else {
                    cons_puts("retorno: ");
                    pon_numero((uint32_t)rc);
                    cons_putc('\n');
                }
            } else {
                cons_puts("No entiendo ese comando. Escribe ayuda.\n");
            }
        }
    }
}
