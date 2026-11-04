#include "ulib.h"
#include "utext.h"

void __main(void) {
}

static int fallos = 0;
static int pruebas = 0;

static void ok(int cond, const char *nom) {
    pruebas++;
    if (!cond) {
        fallos++;
        u_printf("FALLO %s\n", nom);
    }
}

int main(void) {
    int av = 0;
    char d[64];
    u_print("tutf8 v2.2\n");
    /* ASCII */
    ok(ut_dec("A", 1, &av) == 0x41 && av == 1, "ascii");
    /* 2 bytes: ñ U+00F1 = C3 B1 */
    ok(ut_dec("n\xc3\xb1o", 4, &av) == 'n' && av == 1, "pre");
    ok(ut_dec("\xc3\xb1", 2, &av) == 0xF1 && av == 2, "enie");
    /* 3 bytes: € U+20AC = E2 82 AC */
    ok(ut_dec("\xe2\x82\xac", 3, &av) == 0x20AC && av == 3, "euro");
    /* Invalida: continuacion suelta */
    ok(ut_dec("\x81", 1, &av) == UT_REEMPLAZO && av == 1, "suelta");
    /* Incompleta */
    ok(ut_dec("\xc3", 1, &av) == UT_REEMPLAZO, "corta");
    /* Sobre-larga */
    ok(ut_dec("\xc0\xaf", 2, &av) == UT_REEMPLAZO, "sobrelarga");
    /* Sustituto */
    ok(ut_dec("\xed\xa0\x80", 3, &av) == UT_REEMPLAZO, "sustituto");
    /* Codifica */
    ok(ut_enc(d, 64, 0xF1) == 2 && !(unsigned char)d[0] - 0xC3 &&
       !(unsigned char)d[1] - 0xB1, "enc2");
    ok(ut_enc(d, 64, 0x20AC) == 3, "enc3");
    ok(ut_enc(d, 1, 0xF1) == 0, "enc-capo");
    ok(ut_enc(d, 64, 0xD800) == 0, "enc-sust");
    /* Validez y largo */
    ok(ut_valido("a\xc3\xb1", 3), "val-ok");
    ok(!ut_valido("a\xc3", 2), "val-mal");
    ok(ut_largo("a\xc3\xb1\xe2\x82\xac") == 3, "largo");
    /* Navegacion */
    ok(ut_anterior("a\xc3\xb1o", 3) == 1, "ant");
    ok(ut_siguiente("a\xc3\xb1o", 1) == 3, "sig");
    ok(ut_anterior("a\xc3\xb1o", 4) == 3, "ant2");
    /* Compone (NFD -> NFC) */
    ok(ut_compone('a', 0x301) == 0xE1, "c-aguda");
    ok(ut_compone('A', 0x301) == 0xC1, "c-Aguda");
    ok(ut_compone('n', 0x303) == 0xF1, "c-virg");
    ok(ut_compone('N', 0x303) == 0xD1, "c-Virg");
    ok(ut_compone('u', 0x308) == 0xFC, "c-dier");
    ok(ut_compone('c', 0x327) == 0xE7, "c-cedi");
    ok(ut_compone('x', 0x301) == 0, "c-no");
    {
        char n[32];
        /* a + U+0301 (CC 81) -> á (C3 A1) */
        int w = ut_nfc("a\xcc\x81n", n, 32);
        ok(w == 3 && !(unsigned char)n[0] - 0xC3 &&
           !(unsigned char)n[1] - 0xA1 && n[2] == 'n', "nfc");
    }
    u_printf("tutf8: %d pruebas %d fallos\n", pruebas, fallos);
    return fallos ? 1 : 0;
}
