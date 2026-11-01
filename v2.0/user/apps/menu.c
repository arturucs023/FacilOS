#include "ulib.h"

void __main(void) {
}

/* menu grafico v1.7: flechas + Enter. Lanza programas (sys 25),
   abre la terminal (sys 26) o sale a la shell. 100% codigo propio. */

#define NENT 9

static const char *nombres[NENT] = {
    "Calculadora", "Reloj", "Adivina un numero", "Editor de texto",
    "Visor de archivos", "Paint", "Demo del sistema", "Terminal", "Salir"
};

static const char *ayudas[NENT] = {
    "cuentas con teclado", "hora del sistema", "juego de adivinar",
    "edita archivos (pide nombre)", "muestra archivos (pide nombre)",
    "dibuja con flechas", "pruebas de la plataforma", "shell de texto",
    "vuelve a la terminal"
};

static const char *progs[NENT] = {
    "calculadora", "reloj", "adivina", "editor16", "visor32", "paint32",
    "demo16", "", ""
};

static int pide_archivo[NENT] = { 0, 0, 0, 1, 1, 0, 0, 0, 0 };

static int sel = 0;
static int ultimo_rc = -999;

static void marco(int x, int y, int w, int h, int color) {
    u_rectangulo(x, y, w, 1, color);
    u_rectangulo(x, y + h - 1, w, 1, color);
    u_rectangulo(x, y, 1, h, color);
    u_rectangulo(x + w - 1, y, 1, h, color);
}

/* Lee linea con eco grafico (dorso). Enter=1, ESC=0. */
static int lee_gfx(char *d, int max, int x, int y, int wchars) {
    int n = 0;
    d[0] = 0;
    for (;;) {
        int t = u_tecla();
        int i;
        if (t == 13) return 1;
        if (t == 27) return 0;
        if (t == 8) {
            if (n > 0) n--;
        } else if (t >= 32 && t < 127) {
            if (n < max - 1) d[n++] = (char)t;
        } else continue;
        d[n] = 0;
        u_rectangulo(x, y, wchars * 8, 16, C_NEGRO);
        for (i = 0; i < n; i++) {
            char s[2] = { d[i], 0 };
            u_texto(x + i * 8, y, s, C_BLANCO);
        }
        u_rectangulo(x + n * 8, y, 8, 16, C_GRIS);
        u_presentar(x, y, wchars * 8, 16);
    }
}

static void dibuja(void) {
    int i;
    u_texto(0, 0, "", C_NEGRO);   /* toma propiedad (si no, destino falla) */
    u_destino(1);
    u_limpiar(C_NEGRO);
    u_rectangulo(0, 0, 800, 56, C_AZUL);
    u_texto(280, 20, "FacilOS - menu", C_BLANCO);
    for (i = 0; i < NENT; i++) {
        int y = 100 + i * 44;
        if (i == sel) {
            u_rectangulo(180, y - 6, 440, 32, C_GRIS_OSCURO);
            u_texto(200, y, nombres[i], C_AMARILLO);
        } else {
            u_texto(200, y, nombres[i], C_GRIS);
        }
        u_texto(430, y, ayudas[i], C_GRIS_OSCURO);
    }
    marco(170, 86, 460, NENT * 44 + 12, C_AZUL_CLARO);
    u_texto(150, 520, "Flechas: mover  Enter: elegir  ESC: salir", C_CYAN);
    if (ultimo_rc != -999) {
        char b[32];
        u_strcpy(b, "Ultimo: rc ");
        u_itoa(ultimo_rc, b + 11);
        u_texto(150, 544, b, C_GRIS_OSCURO);
    }
    u_presentar(0, 0, 800, 600);
}

/* Los programas hijos dibujan sobre lo que haya: deja la pantalla limpia
   (la consola de texto escribe en el frente). */
static void limpia_frente(void) {
    u_destino(0);
    u_limpiar(C_NEGRO);
}

/* Al salir del menu, limpia el frente: la consola de texto dibuja ahi. */
static int salir_limpio(void) {
    limpia_frente();
    return 0;
}

int main(void) {
    dibuja();
    for (;;) {
        int t = u_tecla();
        if (t == TEC_ARRIBA_U) {
            sel = (sel + NENT - 1) % NENT;
            dibuja();
        } else if (t == TEC_ABAJO_U) {
            sel = (sel + 1) % NENT;
            dibuja();
        } else if (t == TEC_IZQ_U || t == TEC_DER_U) {
            continue;
        } else if (t == 13) {
            if (sel == 7) {
                /* limpia solo el frente (la consola escribe ahi) */
                limpia_frente();
                u_terminal();
                dibuja();
            } else if (sel == 8) {
                return salir_limpio();
            } else if (pide_archivo[sel]) {
                char nom[32];
                limpia_frente();
                u_rectangulo(0, 0, 800, 40, C_AZUL);
                u_texto(200, 12, "Escribe el nombre del archivo:", C_BLANCO);
                u_presentar(0, 0, 800, 40);
                if (lee_gfx(nom, 32, 200, 60, 30)) {
                    if (nom[0]) {
                        limpia_frente();
                        u_ejecutar(progs[sel], nom);
                    }
                }
                dibuja();
            } else {
                limpia_frente();
                u_ejecutar(progs[sel], "");
                dibuja();
            }
        } else if (t == 27) {
            return salir_limpio();
        }
    }
}
