#include "ulib.h"

void __main(void) {
}

/* Calculadora entera: digitos, + - * /, Enter(=), C, Retroceso, q/ESC. */

static char ent[18];
static int nent = 0;
static int acc = 0, pendiente = 0, nuevo = 1, error = 0;

static void aplica(void) {
    if (error || pendiente == 0) return;
    if (pendiente == '+') acc = acc + u_atoi(ent);
    else if (pendiente == '-') acc = acc - u_atoi(ent);
    else if (pendiente == '*') acc = acc * u_atoi(ent);
    else if (pendiente == '/') {
        int d = u_atoi(ent);
        if (!d) { error = 1; return; }
        acc = acc / d;
    }
    pendiente = 0;
}

static void dibuja(void) {
    char vis[32];
    u_destino(1);
    u_limpiar(C_NEGRO);
    u_rectangulo(190, 60, 420, 200, C_AZUL);
    u_rectangulo(200, 70, 400, 60, C_NEGRO);
    if (error) u_strcpy(vis, "Error");
    else if (nent) {
        int i;
        for (i = 0; i < nent && i < 30; i++) vis[i] = ent[i];
        vis[i] = 0;
    } else u_itoa(acc, vis);
    u_texto(490 - u_strlen(vis) * 8, 88, vis, C_VERDE_CLARO);
    u_texto(210, 150,
            pendiente == '+' ? "+  (operando...)" :
            pendiente == '-' ? "-  (operando...)" :
            pendiente == '*' ? "*  (operando...)" :
            pendiente == '/' ? "/  (operando...)" : "lista", C_GRIS);
    u_texto(210, 190, "0-9  + - * /  Enter(=)  C  <-  q", C_CYAN);
    u_texto(210, 300, "Entera: la division trunca. ESC sale.", C_GRIS_OSCURO);
    u_presentar(190, 60, 420, 200);
    u_presentar(210, 300, 400, 16);
}

int main(void) {
    ent[0] = 0;
    dibuja();
    for (;;) {
        int t = u_tecla();
        if (t == 'q' || t == 27) return 0;
        if (error) {
            if (t == 'c' || t == 'C') {
                error = 0;
                acc = 0;
                pendiente = 0;
                nent = 0;
                ent[0] = 0;
                nuevo = 1;
                dibuja();
            }
            continue;
        }
        if (t >= '0' && t <= '9') {
            if (nuevo) { nent = 0; nuevo = 0; }
            if (nent < 16) ent[nent++] = (char)t;
            ent[nent] = 0;
            dibuja();
        } else if (t == '+' || t == '-' || t == '*' || t == '/') {
            if (!nuevo && nent) {
                if (pendiente) aplica();
                else acc = u_atoi(ent);
            }
            pendiente = t;
            nuevo = 1;
            dibuja();
        } else if (t == 13 || t == '=') {
            if (pendiente && nent && !nuevo) {
                aplica();
                nent = 0;
                ent[0] = 0;
                nuevo = 1;
            }
            dibuja();
        } else if (t == 8) {
            if (nent) {
                nent--;
                ent[nent] = 0;
                dibuja();
            }
        } else if (t == 'c' || t == 'C') {
            acc = 0;
            pendiente = 0;
            nent = 0;
            ent[0] = 0;
            nuevo = 1;
            dibuja();
        }
    }
}
