#include "ugfx.h"

void __main(void) {
}

/* ugfx32 (etapa 5, E5b): prueba viva de UGFX. Dibuja primitivas y
   controles, cuenta eventos. Arg = pid del servidor. */

static const char *items[] = {
    "cero", "uno", "dos", "tres", "cuatro", "cinco",
};

int main(void) {
    const char *a = u_args();
    ug_win_t v;
    ug_ev_t ev;
    int srv = 0, nkey = 0, nmou = 0, hover = 0, puls = 0, chk = 0;
    int sel = 0, scroll = 0, pos = 0, bot_antes = 0;
    char campo[40];
    while (*a == ' ') a++;
    srv = u_atoi(a);
    if (srv <= 0) return 99;
    campo[0] = 0;
    if (ug_abre(&v, srv, 460, 300, "ugfx")) return 98;
    for (;;) {
        int t = ug_evento(&v, &ev, 1);
        if (t == UG_EV_SALIR) return 0;
        if (t == UG_EV_TECLA) {
            nkey++;
            if (!ug_campo_tecla(campo, 40, &pos, ev.tecla))
                ug_lista_tecla(&sel, &scroll, 6, 3, ev.tecla);
        } else if (t == UG_EV_RATON) {
            int lx = ev.rx - v.x, ly = ev.ry - (v.y + UG_CHROME);
            nmou++;
            hover = ug_dentro(lx, ly, 20, 120, 120, 28);
            if (ug_clic(bot_antes, ev.rbot) && hover) puls = !puls;
            if (ug_clic(bot_antes, ev.rbot) &&
                ug_dentro(lx, ly, 20, 160, 12, 12))
                chk = !chk;
            bot_antes = ev.rbot;
        } else if (t == UG_EV_DRAW) {
            char nb[64];
            ug_comienza(&v);
            ug_limpia(&v, C_NEGRO);
            ug_linea(&v, 0, 0, 200, 60, C_ROJO);
            ug_rect(&v, 210, 8, 100, 40, C_VERDE);
            ug_relleno(&v, 320, 8, 100, 40, C_AZUL);
            ug_pixel(&v, 5, 5, C_BLANCO);
            ug_texto(&v, 8, 64, "UGFX OK", C_AMARILLO);
            u_itoa(nkey, nb);
            ug_texto(&v, 8, 84, nb, C_BLANCO);
            u_itoa(nmou, nb);
            ug_texto(&v, 60, 84, nb, C_BLANCO);
            ug_boton(&v, 20, 120, 120, 28, "pulsar",
                     puls ? UG_BOTON_PULSADO :
                     hover ? UG_BOTON_HOVER : UG_BOTON_NORMAL);
            ug_check(&v, 20, 160, "marca", chk);
            ug_campo(&v, 160, 120, 200, campo, pos, v.foco);
            ug_lista(&v, 160, 160, 200, 48, items, 6, sel, scroll);
            ug_fin(&v);
        }
    }
}
