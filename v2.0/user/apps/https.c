#include "ulib.h"
#include "bearssl.h"
#include <time.h>

void __main(void) {
}

/* https (hito TLS): cliente HTTPS/1.0 Ring3 con BearSSL (TLS 1.2,
   ECDHE + AES-GCM/ChaCha20, RSA/ECDSA, x509-minimal).
   HITO 1: solo confia en la CA de test ("F32 Test TLS CA",
   stub local 10.0.2.2); internet real necesita bundle (pendiente).
   Entropia de test: reloj + ticks + tiempos de red (ver NOTA). */

static unsigned char hcuerpo[U_HTTP_MAX_CPO + 1];
static unsigned char iobuf[36864];

static unsigned int ahora(void) {
    int t = u_ticks();
    return t < 0 ? 0u : (unsigned int)t;
}

static void pon_txt(const char *s) {
    u_print(s);
}

static void pon_num(unsigned int v) {
    char nb[12];
    int k = 0;
    u_utoa(v, nb, 10);
    while (nb[k]) {
        char b[2];
        b[0] = nb[k];
        b[1] = 0;
        u_print(b);
        k++;
    }
}

/* ---- confianza de test (CA autofirmada del stub local) ---- */
static const unsigned char TA_DN[28] = {
    0x30, 0x1a, 0x31, 0x18, 0x30, 0x16, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c,
    0x0f, 0x46, 0x33, 0x32, 0x20, 0x54, 0x65, 0x73, 0x74, 0x20, 0x54, 0x4c,
    0x53, 0x20, 0x43, 0x41,
};
static const unsigned char TA_N[256] = {
    0xc6, 0x50, 0xab, 0x3f, 0xc2, 0x4a, 0x52, 0xe1, 0xe0, 0xb2, 0x60, 0xb6,
    0xf5, 0x06, 0x9c, 0x1d, 0xe2, 0x5a, 0xef, 0x7b, 0xec, 0x02, 0xea, 0xef,
    0xca, 0x7d, 0x39, 0x9d, 0xbd, 0x55, 0xf2, 0xf5, 0x41, 0x1f, 0xa2, 0xef,
    0x94, 0xb9, 0x3f, 0xbe, 0x3a, 0xca, 0x52, 0x1e, 0x03, 0x37, 0xea, 0xd4,
    0xcb, 0x10, 0xec, 0xac, 0xe2, 0x3d, 0x9e, 0xca, 0xff, 0x66, 0xf5, 0x22,
    0xe8, 0x89, 0xf6, 0xcb, 0x0f, 0x07, 0x35, 0x73, 0x89, 0xe3, 0x01, 0x43,
    0x33, 0x60, 0xa3, 0xea, 0x86, 0xca, 0xaf, 0xd1, 0x8a, 0x3e, 0x7c, 0xb0,
    0x20, 0xb5, 0xb7, 0x3a, 0x3a, 0xd8, 0x16, 0xb9, 0x91, 0x70, 0x17, 0xb7,
    0xa1, 0x5c, 0x5b, 0x65, 0x0a, 0x2d, 0xeb, 0x1a, 0x60, 0xaa, 0x81, 0xe3,
    0x9a, 0xcd, 0xf4, 0x9d, 0x39, 0x89, 0x18, 0xb9, 0x64, 0xcc, 0xd9, 0x45,
    0x48, 0x61, 0x01, 0xc2, 0xe4, 0x39, 0xae, 0xa4, 0xf0, 0x82, 0x21, 0xff,
    0x7a, 0xad, 0xb1, 0x9d, 0x61, 0x4f, 0x33, 0xc5, 0xa9, 0xd5, 0x2f, 0x56,
    0x3f, 0x6a, 0x11, 0x79, 0xcf, 0xe1, 0x08, 0x8e, 0x06, 0x43, 0x0c, 0xc7,
    0x02, 0x50, 0xcf, 0xea, 0xbf, 0xa2, 0x7e, 0xf3, 0x1a, 0x7c, 0x6d, 0xd8,
    0xc4, 0xf3, 0x1c, 0x46, 0x09, 0xb1, 0x19, 0x64, 0x6e, 0xe1, 0x16, 0x25,
    0xb7, 0x6c, 0x5d, 0xcb, 0x02, 0x5e, 0xc0, 0x87, 0x07, 0xe8, 0x25, 0x29,
    0x54, 0x96, 0xc0, 0xdc, 0x75, 0x7a, 0xad, 0xf0, 0xb4, 0x91, 0x78, 0x2f,
    0xbe, 0x02, 0x59, 0x1b, 0xea, 0x42, 0x6b, 0x84, 0x34, 0x0c, 0x6d, 0x9a,
    0xa6, 0x24, 0x3e, 0xb6, 0xfe, 0xf3, 0x17, 0x2f, 0xf4, 0x11, 0xf3, 0x7f,
    0x54, 0x8e, 0x9d, 0x4a, 0xf2, 0x5e, 0x0e, 0x6e, 0x76, 0xf6, 0x1c, 0xf2,
    0xdd, 0xa0, 0xa8, 0xc7, 0x1c, 0xa6, 0xe6, 0xfc, 0x8c, 0x48, 0x0b, 0xd9,
    0x6c, 0xe7, 0x9a, 0x77,
};
static const unsigned char TA_E[3] = {
    0x01, 0x00, 0x01,
};

/* NOTA entropia: sin RNG del nucleo; la semilla mezcla reloj, ticks
   y tiempos de red (suficiente para el stub; revisar antes de
   internet real). */
static void semilla(unsigned char *s, int n) {
    int i = 0, k;
    unsigned int v6[6];
    unsigned int t;
    if (u_reloj(v6) == 0) {
        for (k = 0; k < 6 && i < n; k++) {
            unsigned int v = v6[k];
            int b;
            for (b = 0; b < 4 && i < n; b++) {
                s[i++] = (unsigned char)(v & 0xFFu);
                v >>= 8;
            }
        }
    }
    for (k = 0; k < 8 && i < n; k++) {
        t = ahora();
        s[i++] = (unsigned char)(t & 0xFFu);
        s[i++] = (unsigned char)((t >> 11) & 0xFFu);
        u_dormir(7 + k * 3);
        if (i + 2 > n) break;
        t = ahora() - t;
        s[i++] = (unsigned char)(t & 0xFFu);
    }
    while (i < n) {
        unsigned int q;
        t = ahora() * 2654435761u + (unsigned int)i * 40503u;
        q = (unsigned int)i & 15u;
        s[i++] = (unsigned char)((t >> q) & 0xFFu);
    }
}

int main(void) {
    const char *a = u_args();
    char url[U_HTTP_MAX_URL + 1];
    int nd = 0, id = -1, k = -1;
    red_cfg_t cfg;
    red_if_t rif;
    u_http_url_t u;
    unsigned int ip, fin, t0;
    static br_ssl_client_context sc;
    static br_x509_minimal_context xc;
    static const uint16_t suites[] = {
        BR_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
        BR_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
        BR_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
        BR_TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256
    };
    static unsigned char sem[64];
    unsigned char pet[2048];
    int npet = 0, nbody = 0, penv = 0;
    int cerrando = 0;
    unsigned int fin_cierre = 0;
    while (*a == ' ') a++;
    while (*a && *a != ' ' && nd < U_HTTP_MAX_URL) {
        url[nd++] = *a++;
    }
    url[nd] = 0;
    while (*a == ' ') a++;
    if (!nd || *a) {
        u_print("Uso: https https://dominio[:443][/ruta]\n");
        return 2;
    }
    if (u_http_url(url, &u) || !u.esquema) {
        u_print("https: URL invalida (solo https://dominio[:puerto][/ruta])\n");
        return 2;
    }
    {
        int i;
        for (i = 0; i < 32; i++) ((unsigned char *)&rif)[i] = 0;
        if (u_red_interfaz(&rif) || !rif.presente) {
            u_print("https: sin adaptador\n");
            return 1;
        }
        for (i = 0; i < 16; i++) ((unsigned char *)&cfg)[i] = 0;
        if (u_red_configurar_leer(&cfg) || !cfg.ip) {
            u_print("https: sin IP (DHCP primero)\n");
            return 1;
        }
    }
    if (u.es_ip) {
        ip = u.ip;
    } else {
        u_dns_res_t dr;
        unsigned int rest, tmo;
        int kd, i;
        rest = 30000;
        tmo = rest > 30000 ? 30000 : rest;
        for (i = 0; i < 16; i++) ((unsigned char *)&dr)[i] = 0;
        kd = u_dns_resuelve3(u.host, 0, 0, &dr, tmo);
        if (kd) {
            u_print("https: DNS fallo\n");
            return 4;
        }
        ip = dr.ip;
    }
    t0 = ahora();
    id = u_tcp_abrir(ip, u.puerto, 0);
    if (id <= 0) {
        u_print("https: sin TCP\n");
        return 1;
    }
    /* Peticion GET (se envia tras el handshake, via SENDAPP). */
    {
        int t = 0, j;
        static const char g1[] = "GET ";
        static const char g2[] = " HTTP/1.0\r\nHost: ";
        static const char g3[] = "\r\nConnection: close\r\n"
                                 "User-Agent: FacilOS32-HTTPS/0.1\r\n"
                                 "Accept: */*\r\n\r\n";
        for (j = 0; g1[j] && t < 2000; j++) pet[t++] = g1[j];
        for (j = 0; u.ruta[j] && t < 2000; j++) pet[t++] = u.ruta[j];
        for (j = 0; g2[j] && t < 2000; j++) pet[t++] = g2[j];
        for (j = 0; u.host[j] && t < 2000; j++) pet[t++] = u.host[j];
        for (j = 0; g3[j] && t < 2000; j++) pet[t++] = g3[j];
        if (t >= 2000 || !u.ruta[0]) {
            u_tcp_abortar(id);
            u_print("https: URL demasiado larga\n");
            return 2;
        }
        npet = t;
    }
    /* Cableado BearSSL (ver NOTAS en build32.py: sin UNIX/Win32). */
    {
        static br_x509_trust_anchor ta;
        long uni;
        br_ssl_client_zero(&sc);
        br_ssl_engine_set_buffer(&sc.eng, iobuf, sizeof(iobuf), 1);
        br_ssl_engine_set_versions(&sc.eng, BR_TLS12, BR_TLS12);
        br_ssl_engine_set_suites(&sc.eng, suites,
                                 sizeof(suites) / sizeof(suites[0]));
        br_ssl_engine_set_hash(&sc.eng, br_sha256_ID,
                               &br_sha256_vtable);
        br_ssl_engine_set_hash(&sc.eng, br_sha1_ID, &br_sha1_vtable);
        br_ssl_engine_set_prf_sha256(&sc.eng, &br_tls12_sha256_prf);
        br_ssl_engine_set_gcm(&sc.eng, &br_sslrec_in_gcm_vtable,
                              &br_sslrec_out_gcm_vtable);
        br_ssl_engine_set_chacha20(&sc.eng, &br_chacha20_ct_run);
        br_ssl_engine_set_poly1305(&sc.eng, &br_poly1305_ctmul32_run);
        br_ssl_engine_set_aes_ctr(&sc.eng, &br_aes_ct_ctr_vtable);
        br_ssl_engine_set_ghash(&sc.eng, &br_ghash_ctmul32);
        br_ssl_engine_set_ec(&sc.eng, &br_ec_prime_i31);
        br_ssl_engine_set_rsavrfy(&sc.eng, br_rsa_i31_pkcs1_vrfy);
        br_ssl_engine_set_ecdsa(&sc.eng, br_ecdsa_i31_vrfy_asn1);
        ta.dn.data = (unsigned char *)TA_DN;
        ta.dn.len = sizeof(TA_DN);
        ta.flags = BR_X509_TA_CA;
        ta.pkey.key_type = BR_KEYTYPE_RSA;
        ta.pkey.key.rsa.n = (unsigned char *)TA_N;
        ta.pkey.key.rsa.nlen = sizeof(TA_N);
        ta.pkey.key.rsa.e = (unsigned char *)TA_E;
        ta.pkey.key.rsa.elen = sizeof(TA_E);
        br_x509_minimal_init(&xc, &br_sha256_vtable, &ta, 1);
        br_x509_minimal_set_rsa(&xc, &br_rsa_i31_pkcs1_vrfy);
        br_x509_minimal_set_ecdsa(&xc, &br_ec_prime_i31,
                                  &br_ecdsa_i31_vrfy_asn1);
        br_x509_minimal_set_hash(&xc, br_sha256_ID,
                                 &br_sha256_vtable);
        br_x509_minimal_set_hash(&xc, br_sha1_ID, &br_sha1_vtable);
        uni = time(0);
        if (uni < 0) {
            u_tcp_abortar(id);
            u_print("https: sin reloj (u_reloj fallo)\n");
            return 1;
        }
        br_x509_minimal_set_time(&xc,
                                 (uint32_t)(uni / 86400L + 719163L),
                                 (uint32_t)(uni % 86400L));
        br_ssl_engine_set_x509(&sc.eng, &xc.vtable);
        semilla(sem, (int)sizeof(sem));
        br_ssl_engine_inject_entropy(&sc.eng, sem, sizeof(sem));
        br_ssl_client_reset(&sc, u.host, 0);
    }
    fin = t0 + 6000u;
    fin = t0 + 6000u;
    for (;;) {
        unsigned st;
        if ((int)(ahora() - fin) >= 0) {
            u_tcp_abortar(id);
            u_print("https: tiempo agotado\n");
            return 1;
        }
        if (cerrando && (int)(ahora() - fin_cierre) >= 0) {
            /* Cierre propio sin respuesta del par: se da por
               terminado con el ultimo error registrado. */
            k = br_ssl_engine_last_error(&sc.eng);
            break;
        }
        st = br_ssl_engine_current_state(&sc.eng);
        if (st & BR_SSL_CLOSED) {
            k = br_ssl_engine_last_error(&sc.eng);
            break;
        }
        if (st & BR_SSL_SENDREC) {
            unsigned char *b;
            size_t tl;
            int n, env = 0;
            b = (unsigned char *)br_ssl_engine_sendrec_buf(&sc.eng,
                                                           &tl);
            n = (int)tl;
            while (env < n) {
                int trozo = n - env, w;
                if (trozo > 536) trozo = 536;
                if ((int)(ahora() - fin) >= 0) break;
                w = u_tcp_enviar(id, b + env, trozo);
                if (w > 0) {
                    env += w;
                    continue;
                }
                if (w != -8) {
                    u_tcp_abortar(id);
                    id = -1;
                    break;
                }
                u_dormir(50);
            }
            if (id < 0) {
                k = -100;
                break;
            }
            if ((int)(ahora() - fin) >= 0) continue;
            br_ssl_engine_sendrec_ack(&sc.eng, (size_t)n);
            continue;
        }
        if ((st & BR_SSL_SENDAPP) && penv < npet) {
            unsigned char *b;
            size_t tl;
            int n, manda;
            b = (unsigned char *)br_ssl_engine_sendapp_buf(&sc.eng,
                                                           &tl);
            n = (int)tl;
            manda = npet - penv;
            if (manda > 0 && n > 0) {
                if (manda > n) manda = n;
                {
                    int j;
                    for (j = 0; j < manda; j++)
                        b[j] = pet[penv + j];
                }
                br_ssl_engine_sendapp_ack(&sc.eng, (size_t)manda);
                penv += manda;
                /* El ack no siempre formatea (oxb holgado): fuerza
                   el registro para que haya SENDREC ya. */
                br_ssl_engine_flush(&sc.eng, 0);
                continue;
            }
            br_ssl_engine_flush(&sc.eng, 0);
            /* Sin sitio: a drenar entrada antes de reintentar. */
        }
        if (st & BR_SSL_RECVREC) {
            unsigned char *b;
            size_t tl;
            int n, rr;
            b = (unsigned char *)br_ssl_engine_recvrec_buf(&sc.eng,
                                                           &tl);
            n = (int)tl;
            if (n <= 0) continue;
            {
                unsigned short esp;
                unsigned int rest =
                    (unsigned int)((int)(fin - ahora()));
                if (n > 4096) n = 4096;
                esp = rest > 5000 ? 5000 :
                      (rest ? (unsigned short)rest : 1);
                rr = u_tcp_recibir(id, b, n, esp);
            }
            if (rr > 0) {
                br_ssl_engine_recvrec_ack(&sc.eng, (size_t)rr);
                continue;
            }
            if (!rr) {
                /* Sin datos: se cae al flush final (no continue). */
            } else {
                /* FIN/error del par: si ya estamos cerrando, no hay
                   mas que esperar (el par no mandara close_notify). */
                if (cerrando) {
                    k = br_ssl_engine_last_error(&sc.eng);
                    break;
                }
                /* Cierre ordenado del par: fin de registros. */
                br_ssl_engine_close(&sc.eng);
                cerrando = 1;
                fin_cierre = ahora() + 500u;
                continue;
            }
        }
        if (st & BR_SSL_RECVAPP) {
            unsigned char *b;
            size_t tl;
            int n;
            b = (unsigned char *)br_ssl_engine_recvapp_buf(&sc.eng,
                                                           &tl);
            n = (int)tl;
            if (!n) {
                u_dormir(50);
                continue;
            }
            if (nbody + n > U_HTTP_MAX_CPO) {
                u_tcp_abortar(id);
                id = -1;
                k = -8;
                break;
            }
            {
                int j;
                for (j = 0; j < n; j++)
                    hcuerpo[nbody + j] = b[j];
            }
            nbody += n;
            br_ssl_engine_recvapp_ack(&sc.eng, (size_t)n);
            continue;
        }
        br_ssl_engine_flush(&sc.eng, 0);
        u_dormir(20);
    }
    if (id >= 0) {
        if (k == BR_ERR_OK)
            (void)u_tcp_cerrar(id);
        else
            u_tcp_abortar(id);
    }
    if (k == -100) {
        u_print("https: TCP roto\n");
        return 1;
    }
    if (k == -8) {
        u_print("https: cuerpo demasiado grande\n");
        return 4;
    }
    if (k != BR_ERR_OK) {
        pon_txt("https: TLS fallo ");
        pon_num((unsigned int)(k < 0 ? -k : k));
        pon_txt("\n");
        return 1;
    }
    {
        br_ssl_session_parameters prm;
        int suite;
        br_ssl_engine_get_session_parameters(&sc.eng, &prm);
        suite = (int)prm.cipher_suite;
        pon_txt("https: TLS ok suite 0x");
        {
            char h[8];
            int j;
            u_utoa((unsigned int)suite, h, 16);
            for (j = 0; h[j]; j++) {
                char b[2];
                b[0] = h[j];
                b[1] = 0;
                pon_txt(b);
            }
        }
        pon_txt(" bytes ");
        pon_num((unsigned int)nbody);
        pon_txt("\n");
    }
    if (nbody > 0) {
        int i, txt = 1;
        hcuerpo[nbody] = 0;
        for (i = 0; i < nbody; i++) {
            unsigned char c = hcuerpo[i];
            if (c == 9 || c == 10 || c == 13) continue;
            if (c < 32 || c > 126) {
                txt = 0;
                break;
            }
        }
        if (txt) {
            hcuerpo[nbody] = 0;
            pon_txt((const char *)hcuerpo);
            if (hcuerpo[nbody - 1] != '\n') pon_txt("\n");
        } else {
            pon_txt("(binario)\n");
        }
    }
    return 0;
}
