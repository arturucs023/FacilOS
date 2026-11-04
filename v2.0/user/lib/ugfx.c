#include "ugfx.h"
#include "utext.h"

/* UGFX v1: implementacion. Solo syscalls + IPC; sin estado global salvo
   el lease en curso (un hilo por proceso en FacilOS32). */

static int lee_num(const char *b, int r, int *k) {
    while (*k < r && (b[*k] < '0' || b[*k] > '9') && b[*k] != '-') (*k)++;
    {
        int v = u_atoi(b + *k);
        while (*k < r && ((b[*k] >= '0' && b[*k] <= '9') || b[*k] == '-'))
            (*k)++;
        return v;
    }
}

int ug_abre(ug_win_t *v, int srv, int w, int h, const char *titulo) {
    char m[64], b[64];
    int de = 0, t = 0, q = 0;
    if (!v || srv <= 0) return -1;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    v->srv = srv;
    v->id = -1;
    v->w = w;
    v->h = h;
    v->x = 0;
    v->y = 0;
    v->foco = 0;
    v->vivo = 1;
    m[t++] = 'R';
    m[t++] = 'E';
    m[t++] = 'G';
    m[t++] = ' ';
    {
        char nb[12];
        u_itoa(w, nb);
        q = 0;
        while (nb[q] && t < 60) m[t++] = nb[q++];
        m[t++] = ' ';
        u_itoa(h, nb);
        q = 0;
        while (nb[q] && t < 60) m[t++] = nb[q++];
        if (titulo && titulo[0]) {
            m[t++] = ' ';
            q = 0;
            while (titulo[q] && t < 62) m[t++] = titulo[q++];
        }
        m[t] = 0;
    }
    if (u_ipc_enviar(srv, m, t + 1)) return -2;
    for (;;) {
        int r = u_ipc_recibir(b, 64, &de);
        int k;
        if (r < 0) return -3;
        if (de != srv) continue;
        if (r >= 2 && b[0] == 'I' && b[1] == 'D') {
            k = 2;
            v->id = lee_num(b, r, &k);
            return v->id >= 0 ? 0 : -4;
        }
        if (r >= 4 && b[0] == 'Q' && b[1] == 'U') {
            v->vivo = 0;
            return -5;
        }
        /* otros mensajes pre-ID se ignoran (DRAW temprano etc.) */
    }
}

void ug_cierra(ug_win_t *v) {
    if (!v || !v->vivo) return;
    u_ipc_enviar(v->srv, "BYE", 4);
    v->vivo = 0;
}

int ug_evento(ug_win_t *v, ug_ev_t *ev, int bloquear) {
    char b[64];
    int de = 0;
    ev->tipo = UG_EV_NADA;
    if (!v || !v->vivo) {
        ev->tipo = UG_EV_SALIR;
        return UG_EV_SALIR;
    }
    if (!bloquear && !u_ipc_hay()) return UG_EV_NADA;
    for (;;) {
        int r = u_ipc_recibir(b, 64, &de);
        int k;
        if (r < 0) {
            if (!bloquear) return UG_EV_NADA;
            continue;
        }
        if (de != v->srv) continue;
        if (r >= 4 && b[0] == 'D' && b[1] == 'R' && b[2] == 'A') {
            k = 4;
            v->x = lee_num(b, r, &k);
            v->y = lee_num(b, r, &k);
            if (k < r) {
                int w = lee_num(b, r, &k);
                int h = w >= 0 && k < r ? lee_num(b, r, &k) : -1;
                if (w > 0) v->w = w;
                if (h > 0) v->h = h;
            }
            ev->tipo = UG_EV_DRAW;
            ev->x = v->x;
            ev->y = v->y;
            ev->w = v->w;
            ev->h = v->h;
            return UG_EV_DRAW;
        }
        if (r >= 4 && b[0] == 'Q' && b[1] == 'U') {
            v->vivo = 0;
            ev->tipo = UG_EV_SALIR;
            u_ipc_enviar(v->srv, "BYE", 4);
            return UG_EV_SALIR;
        }
        if (r >= 3 && b[0] == 'K' && b[1] == 'E' && b[2] == 'Y') {
            k = 3;
            ev->tipo = UG_EV_TECLA;
            ev->tecla = lee_num(b, r, &k);
            return UG_EV_TECLA;
        }
        if (r >= 3 && b[0] == 'M' && b[1] == 'O' && b[2] == 'U') {
            k = 3;
            ev->rx = lee_num(b, r, &k);
            ev->ry = lee_num(b, r, &k);
            ev->rbot = lee_num(b, r, &k);
            ev->rueda = 0;
            if (k < r) ev->rueda = lee_num(b, r, &k);
            ev->tipo = UG_EV_RATON;
            return UG_EV_RATON;
        }
        if (r >= 5 && b[0] == 'F' && b[1] == 'O' && b[2] == 'C') {
            k = 5;
            v->foco = lee_num(b, r, &k) ? 1 : 0;
            ev->tipo = UG_EV_FOCO;
            ev->foco = v->foco;
            return UG_EV_FOCO;
        }
        /* ID tardio, INF u otros: se consumen aqui; INF lo lee ug_pide_info
           solo si llega justo tras pedirlo (ver ug_pide_info). */
        if (!bloquear) return UG_EV_NADA;
    }
}

int ug_pide_info(ug_win_t *v, int *w, int *h) {
    char b[64];
    int de = 0;
    if (!v || !v->vivo) return -1;
    if (u_ipc_enviar(v->srv, "INF", 4)) return -1;
    for (;;) {
        int r = u_ipc_recibir(b, 64, &de);
        int k;
        if (r < 0) return -1;
        if (de != v->srv) continue;
        if (r >= 3 && b[0] == 'I' && b[1] == 'N' && b[2] == 'F') {
            k = 3;
            {
                int ww = lee_num(b, r, &k);
                int hh = lee_num(b, r, &k);
                /* INF devuelve la PANTALLA, no la ventana: no tocar
                   v->w/v->h (antes los corrompia a 800x570 y el cliente
                   limpiaba/presentaba fuera de su cromo). */
                if (w) *w = ww;
                if (h) *h = hh;
            }
            return 0;
        }
        if (r >= 4 && b[0] == 'D' && b[1] == 'R' && b[2] == 'A') {
            /* DRAW interleado: actualiza la posicion (el dibujo inicial
               depende de ella) y confirma sin pintar; el contenido
               llega con el DRAW del bucle principal. Antes se
               presentaba en (0,0) y dejaba un fantasma. */
            k = 4;
            v->x = lee_num(b, r, &k);
            v->y = lee_num(b, r, &k);
            if (k < r) {
                int ww = lee_num(b, r, &k);
                int hh = ww >= 0 && k < r ? lee_num(b, r, &k) : -1;
                if (ww > 0) v->w = ww;
                if (hh > 0) v->h = hh;
            }
            u_ipc_enviar(v->srv, "ACK", 4);
            continue;
        }
        if (r >= 4 && b[0] == 'Q' && b[1] == 'U') {
            v->vivo = 0;
            u_ipc_enviar(v->srv, "BYE", 4);
            return -2;
        }
    }
}

void ug_sync(ug_win_t *v) {
    if (!v || !v->vivo) return;
    u_ipc_enviar(v->srv, "SYNC", 5);
}

void ug_modal(ug_win_t *v, int on) {
    if (!v || !v->vivo) return;
    u_ipc_enviar(v->srv, on ? "MODAL 1" : "MODAL 0", 8);
}

void ug_tick(ug_win_t *v, int s) {
    char m[16];
    int t = 0, q = 0;
    char nb[12];
    if (!v || !v->vivo) return;
    if (s < 0) s = 0;
    if (s > 60) s = 60;
    m[t++] = 'T';
    m[t++] = 'I';
    m[t++] = 'C';
    m[t++] = 'K';
    m[t++] = ' ';
    u_itoa(s, nb);
    while (nb[q] && t < 14) m[t++] = nb[q++];
    m[t] = 0;
    u_ipc_enviar(v->srv, m, t + 1);
}

void ug_edit(ug_win_t *v, int on) {
    if (!v || !v->vivo) return;
    u_ipc_enviar(v->srv, on ? "EDIT 1" : "EDIT 0", 7);
}

void ug_comienza(ug_win_t *v) {
    /* Toma el lease (precedente: asegura_propiedad del servidor). */
    u_texto(0, 0, "", C_NEGRO);
    u_destino(1);   /* v2.2-post: los clientes tambien componen en el
                       dorso (el present de ug_fin ya lo vuelca) */
    v->ox = v->x;
    v->oy = v->y + UG_CHROME;
}

void ug_fin(ug_win_t *v) {
    u_presentar(v->x, v->y, v->w, v->h);
    u_ipc_enviar(v->srv, "ACK", 4);
}

/* Recorte al contenido (ox,oy,w,h-UG_CHROME). */
static int recorta(ug_win_t *v, int *x, int *y, int *w, int *h) {
    int ch = v->h - UG_CHROME;
    int x0 = v->ox, y0 = v->oy, x1 = v->ox + v->w, y1 = v->oy + (ch < 1 ? 1 : ch);
    int ax = *x + x0, ay = *y + y0;
    int bx = ax + *w, by = ay + *h;
    if (ax < x0) ax = x0;
    if (ay < y0) ay = y0;
    if (bx > x1) bx = x1;
    if (by > y1) by = y1;
    if (bx <= ax || by <= ay) return 0;
    *x = ax;
    *y = ay;
    *w = bx - ax;
    *h = by - ay;
    return 1;
}

void ug_pixel(ug_win_t *v, int x, int y, int color) {
    int w = 1, h = 1;
    if (!recorta(v, &x, &y, &w, &h)) return;
    u_pixel(x, y, color);
}

void ug_linea(ug_win_t *v, int x0, int y0, int x1, int y1, int color) {
    /* Bresenham con recorte por punto (simple y seguro). */
    int dx = x1 >= x0 ? x1 - x0 : x0 - x1;
    int dy = y1 >= y0 ? y1 - y0 : y0 - y1;
    int sx = x1 >= x0 ? 1 : -1, sy = y1 >= y0 ? 1 : -1;
    int err = dx - dy, x = x0, y = y0;
    for (;;) {
        int w = 1, h = 1, px = x, py = y;
        if (recorta(v, &px, &py, &w, &h)) u_pixel(px, py, color);
        if (x == x1 && y == y1) break;
        {
            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x += sx; }
            if (e2 < dx) { err += dx; y += sy; }
        }
    }
}

void ug_rect(ug_win_t *v, int x, int y, int w, int h, int color) {
    if (w <= 0 || h <= 0) return;
    ug_linea(v, x, y, x + w - 1, y, color);
    ug_linea(v, x, y + h - 1, x + w - 1, y + h - 1, color);
    ug_linea(v, x, y, x, y + h - 1, color);
    ug_linea(v, x + w - 1, y, x + w - 1, y + h - 1, color);
}

void ug_relleno(ug_win_t *v, int x, int y, int w, int h, int color) {
    if (!recorta(v, &x, &y, &w, &h)) return;
    u_rectangulo(x, y, w, h, color);
}

void ug_texto(ug_win_t *v, int x, int y, const char *s, int color) {
    char fila[64];
    int i = 0, yy = y;
    /* Multilinea con '\n'; recorte por filas de 16 px. */
    while (*s) {
        int n = 0;
        while (s[n] && s[n] != '\n' && n < 60) {
            fila[n] = s[n];
            n++;
        }
        fila[n] = 0;
        {
            int w = n * UG_FW, h = UG_FH, xx = x, fy = yy;
            if (recorta(v, &xx, &fy, &w, &h) && w > 0)
                u_texto(xx, fy, fila, color);
        }
        if (!s[n]) break;
        s += s[n] == '\n' ? n + 1 : n;
        yy += UG_FH;
        i++;
        if (i > 64) break;
    }
    (void)i;
}

void ug_limpia(ug_win_t *v, int color) {
    u_rectangulo(v->ox, v->oy, v->w, v->h - UG_CHROME < 1 ? 1 : v->h - UG_CHROME,
                 color);
}

/* ---- controles ---- */

int ug_dentro(int x, int y, int x0, int y0, int w, int h) {
    return x >= x0 && y >= y0 && x < x0 + w && y < y0 + h;
}

void ug_etiqueta(ug_win_t *v, int x, int y, const char *s, int color) {
    ug_texto(v, x, y, s, color);
}

void ug_boton(ug_win_t *v, int x, int y, int w, int h, const char *t,
              int estado) {
    int cara = C_GRIS, borde = C_BLANCO, txt = C_NEGRO;
    if (estado == UG_BOTON_OFF) {
        cara = C_GRIS_OSCURO;
        borde = C_GRIS_OSCURO;
        txt = C_GRIS;
    } else if (estado == UG_BOTON_PULSADO) {
        cara = C_GRIS_OSCURO;
        borde = C_NEGRO;
    } else if (estado == UG_BOTON_HOVER) {
        cara = C_BLANCO;
    }
    ug_relleno(v, x, y, w, h, cara);
    ug_rect(v, x, y, w, h, borde);
    if (t) {
        int n = u_strlen(t), tw = n * UG_FW;
        int tx = x + (w - tw) / 2, ty = y + (h - UG_FH) / 2;
        if (tx < x + 2) tx = x + 2;
        ug_texto(v, tx, ty < y ? y : ty, t, txt);
    }
}

int ug_clic(int antes, int ahora) {
    return !(antes & 1) && (ahora & 1);
}

void ug_panel(ug_win_t *v, int x, int y, int w, int h, int borde,
              int relleno) {
    ug_relleno(v, x, y, w, h, relleno);
    ug_rect(v, x, y, w, h, borde);
}

void ug_check(ug_win_t *v, int x, int y, const char *t, int marcado) {
    ug_rect(v, x, y + 2, 12, 12, C_BLANCO);
    if (marcado) {
        ug_relleno(v, x + 2, y + 4, 8, 8, C_VERDE_CLARO);
    }
    if (t) ug_texto(v, x + 16, y, t, C_BLANCO);
}

void ug_campo(ug_win_t *v, int x, int y, int w, const char *buf, int pos,
              int foco) {
    int n;
    ug_relleno(v, x, y, w, UG_FH + 4, C_NEGRO);
    ug_rect(v, x, y, w, UG_FH + 4, foco ? C_AMARILLO : C_GRIS);
    n = u_strlen(buf);
    {
        /* Ventana deslizante para textos largos. */
        int maxc = (w - 4) / UG_FW, ini = 0, i;
        char vis[64];
        if (maxc > 60) maxc = 60;
        if (maxc < 1) maxc = 1;
        if (pos > ini + maxc - 1) ini = pos - maxc + 1;
        while (ini > 0 && ((unsigned char)buf[ini] & 0xC0) == 0x80) ini--;
        for (i = 0; i < maxc && buf[ini + i]; i++) vis[i] = buf[ini + i];
        vis[i] = 0;
        ug_texto(v, x + 2, y + 2, vis, C_BLANCO);
        if (foco) {
            /* columna en caracteres (no bytes): multibyte no desplaza */
            int col = 0, p = ini;
            while (p < pos && buf[p]) {
                p = ut_siguiente(buf, p);
                col++;
            }
            {
                int cx = x + 2 + col * UG_FW;
                ug_relleno(v, cx, y + 2, 2, UG_FH, C_BLANCO);
            }
        }
    }
    (void)n;
}

int ug_campo_tecla(char *buf, int max, int *pos, int tecla) {
    int n = u_strlen(buf);
    if (tecla == 8) {
        if (*pos > 0 && *pos <= n) {
            int a = ut_anterior(buf, *pos), i;
            for (i = a; i < n; i++) buf[i] = buf[i + (*pos - a)];
            *pos = a;
            return 1;
        }
        return 0;
    }
    if (tecla == 0x103) {
        if (*pos > 0) {
            *pos = ut_anterior(buf, *pos);
            return 1;
        }
        return 0;
    }
    if (tecla == 0x104) {
        if (*pos < n) {
            *pos = ut_siguiente(buf, *pos);
            if (*pos > n) *pos = n;
            return 1;
        }
        return 0;
    }
    if (tecla >= 32 && !(tecla >= 0x101 && tecla <= 0x10A) &&
        !(tecla >= 0xE000 && tecla <= 0xE004) && n + 1 < max) {
        char tmp[5];
        int w = ut_enc(tmp, 5, (unsigned)tecla), i;
        if (!w || n + w >= max) return 0;
        for (i = n; i >= *pos; i--) buf[i + w] = buf[i];
        for (i = 0; i < w; i++) buf[*pos + i] = tmp[i];
        *pos += w;
        return 1;
    }
    return 0;
}

void ug_lista(ug_win_t *v, int x, int y, int w, int h, const char **items,
              int n, int sel, int scroll) {
    int i, vis = h / UG_FH;
    ug_relleno(v, x, y, w, h, C_NEGRO);
    ug_rect(v, x, y, w, h, C_GRIS);
    if (vis < 1) vis = 1;
    for (i = 0; i < vis && scroll + i < n; i++) {
        int yy = y + 1 + i * UG_FH;
        char fila[48];
        int k = 0;
        const char *s = items[scroll + i];
        if (scroll + i == sel)
            ug_relleno(v, x + 1, yy, w - 2, UG_FH, C_AZUL);
        while (s[k] && k < 40) {
            fila[k] = s[k];
            k++;
        }
        fila[k] = 0;
        ug_texto(v, x + 3, yy, fila,
                 scroll + i == sel ? C_BLANCO : C_GRIS);
    }
}

int ug_lista_tecla(int *sel, int *scroll, int n, int vis, int tecla) {
    if (tecla == 0x101) {
        if (*sel > 0) {
            (*sel)--;
            if (*sel < *scroll) *scroll = *sel;
            return 1;
        }
        return 0;
    }
    if (tecla == 0x102) {
        if (*sel + 1 < n) {
            (*sel)++;
            if (*sel >= *scroll + vis) *scroll = *sel - vis + 1;
            return 1;
        }
        return 0;
    }
    return 0;
}

int ug_lista_clic(int my, int y0, int scroll, int n) {
    int idx = scroll + (my - y0 - 1) / UG_FH;
    if (my < y0 + 1) return -1;
    if (idx < 0 || idx >= n) return -1;
    return idx;
}
