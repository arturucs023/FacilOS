#include "ulib.h"
#include "bearssl.h"

void __main(void) {
}

/* t_tls: auto-prueba cripto de BearSSL (Ring3, sin red).
   KAT fijos (SHA-256, HMAC, GCM ida+vuelta, ECDSA P-256) para
   validar el vendoreado en QEMU en cada FULL. */

static int pruebas = 0;
static int fallos = 0;

static void ok(int cond, const char *nom) {
    char b[96];
    int i = 0, t = 0;
    pruebas++;
    if (!cond) {
        fallos++;
        while ("FALLO "[i]) b[t++] = "FALLO "[i++];
    } else {
        while ("ok "[i]) b[t++] = "ok "[i++];
    }
    i = 0;
    while (nom[i] && t < 90) b[t++] = nom[i++];
    b[t++] = '\n';
    b[t] = 0;
    u_print(b);
}

static int memeq(const unsigned char *a, const unsigned char *b, int n) {
    int i;
    for (i = 0; i < n; i++) {
        if (a[i] != b[i]) return 0;
    }
    return 1;
}

/* SHA-256("abc"). */
static const unsigned char SALIDA_ABC[32] = {
    0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde,
    0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
    0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
};

/* HMAC-SHA256 RFC 4231 caso 1: clave 20x0x0b, "Hi There".
   b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7 */
static const unsigned char HMAC_ESP[32] = {
    0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53, 0x5c, 0xa8, 0xaf, 0xce,
    0xaf, 0x0b, 0xf1, 0x2b, 0x88, 0x1d, 0xc2, 0x00, 0xc9, 0x83, 0x3d, 0xa7,
    0x26, 0xe9, 0x37, 0x6c, 0x2e, 0x32, 0xcf, 0xf7
};

/* ECDSA P-256 (generado con cryptography, mensaje "FacilOS TLS KAT"). */
static const unsigned char KAT_PRIV[32] = {
    0x1c, 0x71, 0xc7, 0x1f, 0x38, 0xd2, 0xf7, 0xfc, 0xab, 0x66, 0x50, 0x75,
    0x48, 0xee, 0xf4, 0xa0, 0x53, 0x25, 0x7f, 0x68, 0xec, 0x97, 0x40, 0x34,
    0x12, 0x0c, 0xe9, 0x05, 0x59, 0xac, 0x75, 0x63,
};
static const unsigned char KAT_PUB[65] = {
    0x04, 0x96, 0x43, 0xac, 0x4c, 0x0b, 0xdf, 0x06, 0x25, 0xbf, 0x28, 0x44,
    0x3d, 0x69, 0x8c, 0x00, 0x37, 0x65, 0xbd, 0x60, 0x95, 0x01, 0xbd, 0x93,
    0xb3, 0x63, 0x90, 0x37, 0x8d, 0xf9, 0x66, 0xad, 0xb4, 0xaa, 0x5a, 0x07,
    0x94, 0x03, 0x36, 0x32, 0x5c, 0x04, 0xd4, 0xab, 0xd0, 0x3b, 0x29, 0x5c,
    0x59, 0x01, 0x61, 0x31, 0x5e, 0x30, 0xf4, 0xb1, 0x66, 0xa0, 0x69, 0xc9,
    0x43, 0xd5, 0xbd, 0x38, 0x3c,
};
static const unsigned char KAT_HASH[32] = {
    0x6b, 0x10, 0x87, 0xe1, 0x50, 0x41, 0xdd, 0x31, 0x6b, 0x8c, 0x60, 0x7a,
    0x22, 0x79, 0xab, 0x43, 0x6c, 0xb1, 0xb4, 0x14, 0x8e, 0xca, 0x0a, 0x23,
    0xe5, 0x35, 0x88, 0x8e, 0xd3, 0x4b, 0xb6, 0xe9,
};
static const unsigned char KAT_SIG[64] = {
    0xa1, 0x2e, 0x5c, 0xc6, 0x21, 0x20, 0x8c, 0xbb, 0x7c, 0x3c, 0x15, 0x37,
    0x10, 0xac, 0xec, 0xd1, 0x1e, 0x8e, 0x34, 0x80, 0xbd, 0x48, 0x50, 0x23,
    0xac, 0x89, 0xd7, 0x59, 0x50, 0x97, 0x5e, 0xe9, 0x5f, 0x93, 0xa3, 0x02,
    0x4d, 0xd7, 0xc2, 0x59, 0x7c, 0x8c, 0x58, 0xe7, 0xe6, 0x5e, 0xd8, 0x34,
    0x24, 0x74, 0xe4, 0x5f, 0x80, 0x32, 0xbc, 0xff, 0xfb, 0xae, 0xa9, 0x18,
    0xb8, 0x0e, 0xd1, 0x64,
};

int main(void) {
    u_print("t_tls: KAT...\n");
    {
        /* SHA-256("abc"). */
        br_sha256_context sc;
        unsigned char sal[32];
        br_sha256_init(&sc);
        br_sha256_update(&sc, "abc", 3);
        br_sha256_out(&sc, sal);
        ok(memeq(sal, SALIDA_ABC, 32), "tls-sha256-abc");
    }
    {
        /* HMAC-SHA256 RFC 4231 caso 1. */
        br_hmac_key_context kc;
        br_hmac_context hc;
        unsigned char clave[20], sal[32];
        int i;
        for (i = 0; i < 20; i++) clave[i] = 0x0b;
        br_hmac_key_init(&kc, &br_sha256_vtable, clave,
                         sizeof(clave));
        br_hmac_init(&hc, &kc, 32);
        br_hmac_update(&hc, "Hi There", 8);
        br_hmac_out(&hc, sal);
        ok(memeq(sal, HMAC_ESP, 32), "tls-hmac-rfc4231");
    }
    {
        /* AES-128-GCM ida y vuelta + deteccion de retoque. */
        br_aes_ct_ctr_keys actr;
        br_gcm_context gc;
        unsigned char clave[16], iv[12], pt[64], ct[64], tg[16];
        unsigned char aad[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
        int i;
        for (i = 0; i < 16; i++) clave[i] = (unsigned char)(i * 17 + 3);
        for (i = 0; i < 12; i++) iv[i] = (unsigned char)(i * 5 + 1);
        for (i = 0; i < 64; i++) pt[i] = (unsigned char)(i * 7 + i);
        for (i = 0; i < 64; i++) ct[i] = pt[i];
        br_aes_ct_ctr_init(&actr, clave, sizeof(clave));
        br_gcm_init(&gc, &actr.vtable, &br_ghash_ctmul32);
        br_gcm_reset(&gc, iv, sizeof(iv));
        br_gcm_aad_inject(&gc, aad, sizeof(aad));
        br_gcm_flip(&gc);
        br_gcm_run(&gc, 1, ct, sizeof(ct));
        br_gcm_get_tag(&gc, tg);
        ok(!memeq(ct, pt, 64), "tls-gcm-cifra");
        {
            /* Descifra intacto: recupera el claro y valida tag. */
            br_gcm_context gd;
            unsigned char pt2[64];
            int j;
            for (j = 0; j < 64; j++) pt2[j] = ct[j];
            br_gcm_init(&gd, &actr.vtable, &br_ghash_ctmul32);
            br_gcm_reset(&gd, iv, sizeof(iv));
            br_gcm_aad_inject(&gd, aad, sizeof(aad));
            br_gcm_flip(&gd);
            br_gcm_run(&gd, 0, pt2, sizeof(pt2));
            ok(memeq(pt2, pt, 64) &&
               br_gcm_check_tag(&gd, tg) != 0,
               "tls-gcm-ida-vuelta");
        }
        {
            /* Un bit retocado: el tag ya no cuela. */
            br_gcm_context gd;
            unsigned char pt2[64];
            int j;
            for (j = 0; j < 64; j++) pt2[j] = ct[j];
            pt2[7] ^= 1;
            br_gcm_init(&gd, &actr.vtable, &br_ghash_ctmul32);
            br_gcm_reset(&gd, iv, sizeof(iv));
            br_gcm_aad_inject(&gd, aad, sizeof(aad));
            br_gcm_flip(&gd);
            br_gcm_run(&gd, 0, pt2, sizeof(pt2));
            ok(br_gcm_check_tag(&gd, tg) == 0, "tls-gcm-retoca-no");
        }
    }
    {
        /* ECDH P-256: pub = priv*G coincide con la de referencia. */
        br_ec_private_key sk;
        br_ec_public_key pk;
        unsigned char kbuf[65];
        size_t n;
        sk.curve = BR_EC_secp256r1;
        sk.x = (unsigned char *)KAT_PRIV;
        sk.xlen = sizeof(KAT_PRIV);
        n = br_ec_compute_pub(&br_ec_prime_i31, &pk, kbuf, &sk);
        ok(n == 65 && memeq(kbuf, KAT_PUB, 65), "tls-ecdh-pub");
    }
    {
        /* ECDSA P-256: la firma de referencia verifica. */
        br_ec_public_key pk;
        uint32_t v;
        pk.curve = BR_EC_secp256r1;
        pk.q = (unsigned char *)KAT_PUB;
        pk.qlen = sizeof(KAT_PUB);
        v = br_ecdsa_i31_vrfy_raw(&br_ec_prime_i31, KAT_HASH,
                                  sizeof(KAT_HASH), &pk, KAT_SIG,
                                  sizeof(KAT_SIG));
        ok(v != 0, "tls-ecdsa-ok");
        {
            /* Firma corrupta: no verifica. */
            static unsigned char mala[64];
            int j;
            for (j = 0; j < 64; j++) mala[j] = KAT_SIG[j];
            mala[10] ^= 4;
            v = br_ecdsa_i31_vrfy_raw(&br_ec_prime_i31, KAT_HASH,
                                      sizeof(KAT_HASH), &pk, mala,
                                      sizeof(mala));
            ok(v == 0, "tls-ecdsa-mala-no");
        }
    }
    {
        char nb[12];
        int q = 0;
        u_print("t_tls: ");
        u_itoa(pruebas, nb);
        while (nb[q]) {
            char c[2];
            c[0] = nb[q++];
            c[1] = 0;
            u_print(c);
        }
        u_print(" pruebas ");
        q = 0;
        u_itoa(fallos, nb);
        while (nb[q]) {
            char c[2];
            c[0] = nb[q++];
            c[1] = 0;
            u_print(c);
        }
        u_print(" fallos\n");
        return fallos ? 1 : 0;
    }
}
