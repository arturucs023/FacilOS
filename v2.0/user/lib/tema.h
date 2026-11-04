#ifndef F32_TEMA_H
#define F32_TEMA_H

/* TEMA FacilOS32 v2.1-P0 ("marino", identidad propia).
 * Color: RGB24(r,g,b) = 0x01000000|rgb (bit24 = marca RGB cruda para el
 * kernel; la paleta 0-15 sigue valida). Todo Ring3; el kernel no conoce
 * el tema. Coste: ~0 RAM dinamica (todo const estatico). */

#define RGB24(r, g, b) \
    ((int)(0x01000000u | ((unsigned)(r) << 16) | ((unsigned)(g) << 8) | \
           (unsigned)(b)))

/* Fondo procedural: degradado vertical + formas. */
#define T_FONDO_ARRIBA RGB24(27, 42, 74)
#define T_FONDO_ABAJO RGB24(12, 20, 40)
#define T_FONDO_BANDA RGB24(36, 54, 94)
#define T_FONDO_LINEA RGB24(61, 125, 216)

/* Panel inferior (30 px). */
#define T_PANEL RGB24(44, 58, 74)
#define T_PANEL_BORDE RGB24(26, 35, 48)
#define T_PANEL_TEXTO RGB24(242, 245, 249)
#define T_PANEL_SEP RGB24(26, 35, 48)

/* Acento / seleccion. */
#define T_ACENTO RGB24(61, 125, 216)
#define T_SELECCION RGB24(46, 95, 163)

/* Ventanas. */
#define T_VENT_FONDO RGB24(20, 28, 40)
#define T_TIT_ACT RGB24(46, 95, 163)
#define T_TIT_INACT RGB24(58, 69, 86)
#define T_TIT_TEXTO RGB24(242, 245, 249)
#define T_BORDE RGB24(16, 22, 31)
#define T_SOMBRA_1 RGB24(10, 15, 25)
#define T_SOMBRA_2 RGB24(14, 21, 34)
#define T_SOMBRA_3 RGB24(18, 27, 43)

/* Botones de ventana. */
#define T_BTN_CARA RGB24(58, 74, 94)
#define T_BTN_BORDE RGB24(20, 28, 40)
#define T_BTN_CERRAR RGB24(192, 57, 43)
#define T_BTN_CERRAR_TXT RGB24(255, 255, 255)
#define T_BTN_MIN RGB24(58, 74, 94)

/* Menu. */
#define T_MENU_FONDO RGB24(44, 58, 74)
#define T_MENU_CAB RGB24(46, 95, 163)
#define T_MENU_TEXTO RGB24(242, 245, 249)
#define T_MENU_SEL RGB24(46, 95, 163)

/* Geometria P0 (CROMO_H=20 intacto: contrato UGFX con clientes). */
#define T_BARRA_H 30
#define T_SOMBRA_PX 3
#define T_ICON 16

/* ---- iconos 16x16 propios (letras; '.' = transparente) ----
 * Paleta de iconos: K contorno, W blanco, G gris, g gris oscuro,
 * B azul, b azul oscuro, N verde, R rojo, Y amarillo, C cyan,
 * O naranja, P papel. 24 iconos. */
#define T_ICON_PAL "KWRGBNYCOPg.b"

static int t_icon_rgb(char c) {
    switch (c) {
    case 'K': return RGB24(16, 22, 31);
    case 'W': return RGB24(242, 245, 249);
    case 'R': return RGB24(192, 57, 43);
    case 'G': return RGB24(138, 151, 168);
    case 'B': return RGB24(61, 125, 216);
    case 'N': return RGB24(63, 163, 77);
    case 'Y': return RGB24(232, 195, 58);
    case 'C': return RGB24(57, 192, 216);
    case 'O': return RGB24(216, 122, 61);
    case 'P': return RGB24(232, 236, 241);
    case 'g': return RGB24(58, 69, 86);
    case 'b': return RGB24(27, 42, 74);
    default: return 0;
    }
}

/* Nombres de fondos (compartido servidor/configura). */
static const char *t_fondo_nombre(int i) {
    if (i == 1) return "grafito";
    if (i == 2) return "bosque";
    if (i == 3) return "burdeos";
    if (i == 4) return "medianoche";
    return "marino";
}
/* Orden iconos: 0 F32, 1 terminal, 2 explorador, 3 editor, 4 sysinfo,
 * 5 configura, 6 carpeta, 7 texto, 8 ejecutable, 9 cerrar,
 * 10 minimizar, 11 activa, 12 atras, 13 delante, 14 subir,
 * 15 actualizar, 16 nuevo, 17 carpeta nueva, 18 renombrar, 19 copiar,
 * 20 cortar, 21 pegar, 22 borrar, 23 propiedades. */
static const char *const T_ICONOS[24][16] = {
    { /* 0 F32: roseta azul con F blanca */
      ".....KKKKKK.....",
      "...KKBBBBBBKK...",
      "..KBBBBBBBBBBK..",
      "..KBWWWWWWWBBK..",
      ".KBWWKKKKKWBBK..",
      ".KBWWK...KWBBK..",
      ".KBWWKKKKKWBBK..",
      ".KBWWK...KWBBK..",
      ".KBWWK...KWBBK..",
      ".KBWWK.....BBK..",
      "..KBBBBBBBBBBK..",
      "..KBBBBBBBBBBK..",
      "...KKBBBBBBKK...",
      ".....KKKKKK.....",
      "................",
      "................" },
    { /* 1 terminal: pantalla oscura con prompt */
      ".KKKKKKKKKKKKKK.",
      ".KbbbbbbbbbbbbK.",
      ".KbWb........bK.",
      ".KbWWb.......bK.",
      ".KbWb........bK.",
      ".Kbggggggggg.bK.",
      ".KbWbNN....b.bK.",
      ".KbWWb.NN..b.bK.",
      ".KbWb..NNNNb.bK.",
      ".Kb...b...Nb.bK.",
      ".KbbbbbbbbbbbbK.",
      ".KKKKKKKKKKKKKK.",
      "...KggggggggK...",
      "...KggggggggK...",
      "...KKKKKKKKKK...",
      "................" },
    { /* 2 explorador: carpeta amarilla */
      "................",
      "..KKKKK.........",
      "..KYYYKKKKKKK...",
      "..KYYYYYYYYYYYK.",
      ".KYYYYYYYYYYYYK.",
      ".KYKKKKKKKKKYYK.",
      ".KYKPPPPPPPKYYK.",
      ".KYKPPPPPPPKYYK.",
      ".KYKPPPPPPPKYYK.",
      ".KYKPPPPPPPKYYK.",
      ".KYYYYYYYYYYYYK.",
      ".KKKKKKKKKKKKKK.",
      "................",
      "................",
      "................",
      "................" },
    { /* 3 editor: hoja con lineas + lapiz */
      "....KKKKKKK.....",
      "...KPPPPPPPK....",
      "...KPBBBBBPK....",
      "...KPbPPPPPKO...",
      "...KPbPPPPPKOO..",
      "...KPBBBBBPKOO..",
      "...KPPPPPPPKOO..",
      "...KPBBBBBPKO...",
      "...KPPPPPPPK....",
      "...KPBBBBBPK....",
      "...KPPPPPPPK....",
      "...KKKKKKKKK....",
      "................",
      "................",
      "................",
      "................" },
    { /* 4 sysinfo: monitor con barras */
      ".KKKKKKKKKKKKKK.",
      ".KbBBBBBBBBBBbK.",
      ".KbBWWbbbbBBbBK.",
      ".KbBWbbbNNbBbBK.",
      ".KbBbbbbNNbBbBK.",
      ".KbBbbYYNNbBbBK.",
      ".KbBbYYYYNbBbBK.",
      ".KbBbbYYNbBBbBK.",
      ".KbBbbbbbBBBbbK.",
      ".KbbbbbbbbbbbbK.",
      ".KKKKKKKKKKKKKK.",
      ".....KggggK.....",
      "...KKggggggKK...",
      "...KggggggggK...",
      "...KKKKKKKKKK...",
      "................" },
    { /* 5 configura: rueda dentada */
      "......KKKK......",
      ".....KGGGGK.....",
      "...KKKGKKGKKK...",
      "....KGKKKKGK....",
      "....GKKWWKKG....",
      ".KKKGKWWWWKGKKK.",
      ".KGGKWWWWWWKGGK.",
      ".KGKWWWWWWWWKGK.",
      ".KGGKWWWWWWKGGK.",
      ".KKKGKWWWWKGKKK.",
      "....GKKWWKKG....",
      "....KGKKKKGK....",
      "...KKKGKKGKKK...",
      ".....KGGGGK.....",
      "......KKKK......",
      "................" },
    { /* 6 carpeta: carpeta abierta */
      "................",
      "..KKKKKK........",
      "..KOOOOOKKKKK...",
      "..KOOOOOOOOOOK..",
      ".KKKKKKKKKKKOO..",
      ".KOYYYYYYYYYOOK.",
      ".KOYKKKKKKKYOOK.",
      ".KOYKPPPPPKYOOK.",
      ".KOYKPPPPPKYOOK.",
      ".KOYYYYYYYYYOOK.",
      ".KKKKKKKKKKKKK..",
      "................",
      "................",
      "................",
      "................",
      "................" },
    { /* 7 texto: hoja pequena */
      ".....KKKKKK.....",
      "....KPPPPPK.....",
      "....KPBBBK......",
      "....KPPPPK......",
      "....KPBBBK......",
      "....KPPPPK......",
      "....KPBBBK......",
      "....KPPPPK......",
      "....KPBBBK......",
      "....KPPPPK......",
      "....KKKKKK......",
      "................",
      "................",
      "................",
      "................",
      "................" },
    { /* 8 ejecutable: flecha verde */
      ".....KKKKKK.....",
      "...KKNNNNNNKK...",
      "..KNNNNNNNNNNK..",
      "..KNWWNNNNNNNK..",
      "..KNWWWNNNNNNK..",
      "..KNWWWWNNNNNK..",
      "..KNWWWWWWNNNK..",
      "..KNWWWWNNNNNK..",
      "..KNWWWNNNNNNK..",
      "..KNWWNNNNNNNK..",
      "..KNNNNNNNNNNK..",
      "...KKNNNNNNKK...",
      ".....KKKKKK.....",
      "................",
      "................",
      "................" },
    { /* 9 cerrar: X blanca sobre rojo */
      "..KKKKKKKKKKKK..",
      "..KRRRRRRRRRRK..",
      "..KRWWKRRKWWRK..",
      "..KRRWWKWWRRK...",
      "..KRRRWWWWRRK...",
      "..KRRRRWWRRRK...",
      "..KRRRRWWRRRK...",
      "..KRRRWWWWRRK...",
      "..KRRWWKWWRRK...",
      "..KRWWKRRKWWRK..",
      "..KRRRRRRRRRRK..",
      "..KKKKKKKKKKKK..",
      "................",
      "................",
      "................",
      "................" },
    { /* 10 minimizar: guion sobre gris */
      "..KKKKKKKKKKKK..",
      "..KggggggggggK..",
      "..KgGGGGGGGGGgK.",
      "..KgGGGGGGGGGgK.",
      "..KgGGGGGGGGGgK.",
      "..KgGWWWWWWWGgK.",
      "..KgGWWWWWWWGgK.",
      "..KgGGGGGGGGGgK.",
      "..KgGGGGGGGGGgK.",
      "..KggggggggggK..",
      "..KKKKKKKKKKKK..",
      "................",
      "................",
      "................",
      "................",
      "................" },
    { /* 11 activa: punto azul con halo */
      "................",
      "................",
      ".....KKKKK......",
      "....KBBBBBK.....",
      "...KBBWWBBBK....",
      "...KBWWWBBBK....",
      "...KBWWWWBBK....",
      "...KBBWWBBBK....",
      "....KBBBBBK.....",
      ".....KKKKK......",
      "................",
      "................",
      "................",
      "................",
      "................",
      "................" },
    { /* 12 atras: flecha a la izquierda */
      "................",
      ".....KK.........",
      "....KWWK........",
      "...KWWWWK.......",
      "..KWWWWWWK......",
      ".KWWWWWWWWK.....",
      ".KKKKKKKKKKK....",
      "...KWWWWK.......",
      "...KWWWWK.......",
      "...KWWWWK.......",
      "...KWWWWK.......",
      "...KKKKKK.......",
      "................",
      "................",
      "................",
      "................" },
    { /* 13 delante: flecha a la derecha */
      "................",
      ".........KK.....",
      "........KWWK....",
      ".......KWWWWK...",
      "......KWWWWWWK..",
      ".....KWWWWWWWWK.",
      ".....KKKKKKKKKK.",
      ".......KWWWWK...",
      ".......KWWWWK...",
      ".......KWWWWK...",
      ".......KWWWWK...",
      ".......KKKKKK...",
      "................",
      "................",
      "................",
      "................" },
    { /* 14 subir: flecha hacia arriba */
      "................",
      ".......KK.......",
      "......KWWK......",
      "......KWWK......",
      ".....KWWWWK.....",
      ".....KWWWWK.....",
      "....KWWWWWWK....",
      "...KWWWWWWWWK...",
      "..KWWWWWWWWWWK..",
      "..KKKKKKKKKKKK..",
      "................",
      "................",
      "................",
      "................",
      "................",
      "................" },
    { /* 15 actualizar: circulo con punta */
      "................",
      ".....KKKKK......",
      "....K.....K.....",
      "...K.......K....",
      "..K.........K...",
      "..K.........K...",
      "..K.........K...",
      "..K.........K...",
      "..K.........K...",
      "...K.......K....",
      "....K.....KKK...",
      ".....KKKKK.K....",
      "...........K....",
      "................",
      "................",
      "................" },
    { /* 16 nuevo: hoja en blanco */
      "....KKKKKKK.....",
      "....KPPPPPPK....",
      "....KPPPPPPK....",
      "....KPPPPPPK....",
      "....KPPPPPPK....",
      "....KPPPPPPK....",
      "....KPPPPPPK....",
      "....KPPPPPPK....",
      "....KPPPPPPK....",
      "....KPPPPPPK....",
      "....KPPPPPPK....",
      "....KKKKKKK.....",
      "................",
      "................",
      "................",
      "................" },
    { /* 17 carpeta nueva: carpeta con cruz */
      "................",
      "..KKKKK.........",
      "..KYYYKKKKKKK...",
      "..KYYYYYYYYYYK..",
      ".KYYYYYYYYYYYYK.",
      ".KYKYYYYYYKKKK..",
      ".KYKYYYYYYK.....",
      ".KYKYYYYYYK.....",
      ".KYYYYYYYYK.NN..",
      ".KYYYYYYYYKNNNN.",
      ".KKKKKKKKKK.NN..",
      ".............NN.",
      "................",
      "................",
      "................",
      "................" },
    { /* 18 renombrar: lapiz diagonal */
      "................",
      "..............K.",
      ".............KOK.",
      "............KOOK.",
      "...........KOOK..",
      "..........KOOK...",
      ".........KOOK....",
      "........KOOK.....",
      ".......KOOK......",
      "......KOOK.......",
      ".....KKK.........",
      "....KK...........",
      "................",
      "................",
      "................",
      "................" },
    { /* 19 copiar: dos hojas */
      "....KKKKKKK.....",
      "...KPPPPPPK.....",
      "...KPBBBKPK.....",
      "...KPPPPKPK.....",
      "...KPBBBKPK.....",
      "...KPPPPKPK.....",
      "...KPBBBKPK.....",
      "...KKKKKKPK.....",
      ".....KPPPPK.....",
      ".....KPPPPK.....",
      ".....KPPPPK.....",
      ".....KKKKKK.....",
      "................",
      "................",
      "................",
      "................" },
    { /* 20 cortar: tijeras */
      "................",
      ".KK..........KK.",
      ".KWWK......KWWK.",
      "..KWWK....KWWK..",
      "...KWWK..KWWK...",
      "....KWWKKWWK....",
      ".....KWWWWK.....",
      "......KWWK......",
      ".....KWWWWK.....",
      "....KWWKKWWK....",
      "...KWWK..KWWK...",
      "..KWWK....KWWK..",
      ".KWWK......KWWK.",
      ".KK..........KK.",
      "................",
      "................" },
    { /* 21 pegar: portapapeles */
      ".....KKKKK......",
      "....KBBBBBK.....",
      "....KBBBBBK.....",
      "..KKKKKKKKKK....",
      "..KPPPPPPPPK....",
      "..KPPPPPPPPK....",
      "..KPPPPPPPPK....",
      "..KPPPPPPPPK....",
      "..KPPPPPPPPK....",
      "..KPPPPPPPPK....",
      "..KPPPPPPPPK....",
      "..KKKKKKKKKK....",
      "................",
      "................",
      "................",
      "................" },
    { /* 22 borrar: equis roja */
      "................",
      ".KK..........KK.",
      ".KRRK......KRRK.",
      "..KRRK....KRRK..",
      "...KRRK..KRRK...",
      "....KRRKKRRK....",
      ".....KRRRRK.....",
      "......KRRK......",
      ".....KRRRRK.....",
      "....KRRKKRRK....",
      "...KRRK..KRRK...",
      "..KRRK....KRRK..",
      ".KRRK......KRRK.",
      ".KK..........KK.",
      "................",
      "................" },
    { /* 23 propiedades: i sobre azul */
      "................",
      ".....KKKKK......",
      "....KBBBBBK.....",
      "...KBBBWBBBK....",
      "...KBBBWBBBK....",
      "...KBBBBBBBK....",
      "...KBBBWBBBK....",
      "...KBBBWBBBK....",
      "...KBBBWBBBK....",
      "...KBBBWBBBK....",
      "...KBBBWBBBK....",
      "...KBBBWBBBK....",
      "....KBBBBBK.....",
      ".....KKKKK......",
      "................",
      "................" },
};

#endif
