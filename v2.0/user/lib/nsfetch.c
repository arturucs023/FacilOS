/* nsfetch.c: fetch HTTP/HTTPS para NetSurf (Hito 2).
 * Motor TLS movido de https.c (sin duplicar); http via u_http_trae;
 * redirecciones 301/302/303/307/308 con Location (max 3).
 * Sin UNIX/BSD/Windows: solo u_* + BearSSL, enlazado estatico. */
#include "ulib.h"
#include "nsfetch.h"
#include "nsta.h"
#include "bearssl.h"
#include <time.h>

/* ---- buffer comun (http y https comparten; static como https.c) ---- */
static unsigned char HCUERPO[U_HTTP_MAX_CPO + 1];
static unsigned char iobuf[36864];

static unsigned int ahora(void) {
    int t = u_ticks();
    return t < 0 ? 0u : (unsigned int)t;
}

/* Entropia de test (movida de https.c): reloj + ticks + jitter.
   Suficiente para el stub; internet real pendiente (Hito 4). */
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

/* Peticion HTTPS cruda a traves de BearSSL; alimenta el parser
   incremental u_http (mismo que http). Rellena h (codigo y
   cabeceras) y deja el cuerpo en HCUERPO.
   Devuelve codigo NSFETCH_* y suite/detalle. */
static int https_pide(const char *url, u_http_res_t *h,
                      unsigned *psuite, int *pdetalle) {
    u_http_url_t u;
    unsigned int ip;
    int id = -1, npet = 0, penv = 0, k = -100, cerrando = 0;
    unsigned int fin_cierre = 0, t0, fin;
    unsigned char pet[2048];
    int nb = 0;
    static br_ssl_client_context sc;
    static br_x509_minimal_context xc;
    static unsigned char sem[64];
    if (u_http_url(url, &u) || u.esquema != 1) return NSFETCH_E_URL;
    {
        int i;
        red_if_t rif;
        red_cfg_t cfg;
        for (i = 0; i < 32; i++) ((unsigned char *)&rif)[i] = 0;
        if (u_red_interfaz(&rif) || !rif.presente) return NSFETCH_E_RED;
        for (i = 0; i < 16; i++) ((unsigned char *)&cfg)[i] = 0;
        if (u_red_configurar_leer(&cfg) || !cfg.ip) return NSFETCH_E_RED;
    }
    if (u.es_ip) {
        ip = u.ip;
    } else {
        u_dns_res_t dr;
        int i, kd;
        for (i = 0; i < 16; i++) ((unsigned char *)&dr)[i] = 0;
        kd = u_dns_resuelve3(u.host, 0, 0, &dr, 10000u);
        if (kd) {
            *pdetalle = kd;
            return NSFETCH_E_DNS;
        }
        ip = dr.ip;
    }
    t0 = ahora();
    id = u_tcp_abrir(ip, u.puerto, 0);
    if (id <= 0) return NSFETCH_E_RED;
    {
        int t = 0, j;
        static const char g1[] = "GET ";
        static const char g2[] = " HTTP/1.0\r\nHost: ";
        static const char g3[] = "\r\nConnection: close\r\n"
                                 "User-Agent: FacilOS32-Fetch/0.2\r\n"
                                 "Accept: */*\r\n\r\n";
        for (j = 0; g1[j] && t < 2000; j++) pet[t++] = g1[j];
        for (j = 0; u.ruta[j] && t < 2000; j++) pet[t++] = u.ruta[j];
        for (j = 0; g2[j] && t < 2000; j++) pet[t++] = g2[j];
        for (j = 0; u.host[j] && t < 2000; j++) pet[t++] = u.host[j];
        for (j = 0; g3[j] && t < 2000; j++) pet[t++] = g3[j];
        if (t >= 2000 || !u.ruta[0]) {
            u_tcp_abortar(id);
            return NSFETCH_E_URL;
        }
        npet = t;
    }
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
            return NSFETCH_E_RED;
        }
        br_x509_minimal_set_time(&xc,
                                 (uint32_t)(uni / 86400L + 719163L),
                                 (uint32_t)(uni % 86400L));
        br_ssl_engine_set_x509(&sc.eng, &xc.vtable);
        semilla(sem, (int)sizeof(sem));
        br_ssl_engine_inject_entropy(&sc.eng, sem, sizeof(sem));
        br_ssl_client_reset(&sc, u.host, 0);
    }
    u_http_res_inicia(h);
    fin = t0 + 6000u;
    for (;;) {
        unsigned st;
        if ((int)(ahora() - fin) >= 0) {
            u_tcp_abortar(id);
            id = -1;
            k = -1;
            break;
        }
        if (cerrando && (int)(ahora() - fin_cierre) >= 0) {
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
                int j;
                if (manda > n) manda = n;
                for (j = 0; j < manda; j++)
                    b[j] = pet[penv + j];
                br_ssl_engine_sendapp_ack(&sc.eng, (size_t)manda);
                penv += manda;
                br_ssl_engine_flush(&sc.eng, 0);
                continue;
            }
            br_ssl_engine_flush(&sc.eng, 0);
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
                /* sin datos: cae al flush final */
            } else {
                if (cerrando) {
                    k = br_ssl_engine_last_error(&sc.eng);
                    break;
                }
                br_ssl_engine_close(&sc.eng);
                cerrando = 1;
                fin_cierre = ahora() + 500u;
                continue;
            }
        }
        if (st & BR_SSL_RECVAPP) {
            unsigned char *b;
            size_t tl;
            int n, rc;
            b = (unsigned char *)br_ssl_engine_recvapp_buf(&sc.eng,
                                                           &tl);
            n = (int)tl;
            if (!n) {
                u_dormir(50);
                continue;
            }
            if (nb + n > U_HTTP_MAX_CPO) {
                u_tcp_abortar(id);
                id = -1;
                return NSFETCH_E_CUERPO;
            }
            rc = u_http_res_datos(h, b, n, HCUERPO,
                                  U_HTTP_MAX_CPO, &nb);
            if (rc < 0) {
                u_tcp_abortar(id);
                id = -1;
                *pdetalle = h->err;
                return NSFETCH_E_RED;
            }
            br_ssl_engine_recvapp_ack(&sc.eng, (size_t)n);
            if (rc > 0 || h->estado == U_HTTP_ST_OK) {
                k = BR_ERR_OK;
                (void)u_tcp_cerrar(id);
                id = -1;
                break;
            }
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
    if (k != BR_ERR_OK) {
        *pdetalle = k < 0 ? -k : k;
        return NSFETCH_E_RED;
    }
    {
        br_ssl_session_parameters prm;
        br_ssl_engine_get_session_parameters(&sc.eng, &prm);
        *psuite = (unsigned int)prm.cipher_suite;
    }
    {
        int e = u_http_res_fin(h);
        if (e < 0 && h->estado != U_HTTP_ST_OK) {
            *pdetalle = h->err;
            return NSFETCH_E_RED;
        }
    }
    HCUERPO[h->cuerpo_n < U_HTTP_MAX_CPO ? h->cuerpo_n
                                         : U_HTTP_MAX_CPO] = 0;
    return NSFETCH_OK;
}

/* Sigue redirecciones 301/302/303/307/308 con Location (max 3).
   http via u_http_trae; https via motor BearSSL. */
int nsfetch_get(const char *url, nsfetch_res_t *r) {
    char cur[U_HTTP_MAX_URL + 1];
    int redir = 0, i = 0;
    if (!url || !r) return NSFETCH_E_URL;
    r->cuerpo = (const char *)HCUERPO;
    r->tam = 0;
    r->codigo = 0;
    r->suite = 0;
    r->detalle = 0;
    HCUERPO[0] = 0;
    while (url[i] && i < U_HTTP_MAX_URL) {
        cur[i] = url[i];
        i++;
    }
    cur[i] = 0;
    if (!cur[0] || i >= U_HTTP_MAX_URL) return NSFETCH_E_URL;
    for (;;) {
        u_http_url_t u;
        u_http_res_t h;
        int cod;
        if (u_http_url(cur, &u)) return NSFETCH_E_URL;
        if (u.esquema != 0 && u.esquema != 1) return NSFETCH_E_URL;
        u_http_res_inicia(&h);
        if (u.esquema == 0) {
            int rc = u_http_trae(cur, &h, HCUERPO,
                                 U_HTTP_MAX_CPO, 10000u);
            if (rc < 0) {
                r->detalle = -rc;
                if (rc == -2 || rc == -1) return NSFETCH_E_URL;
                if (rc == -3 || rc == -4) return NSFETCH_E_DNS;
                return NSFETCH_E_RED;
            }
            cod = h.codigo;
        } else {
            int det = 0, rc;
            unsigned su = 0;
            rc = https_pide(cur, &h, &su, &det);
            if (rc != NSFETCH_OK) {
                r->detalle = det;
                return rc;
            }
            cod = h.codigo;
            r->suite = su;
        }
        if (cod != 301 && cod != 302 && cod != 303 &&
            cod != 307 && cod != 308) {
            r->codigo = cod;
            r->tam = (int)h.cuerpo_n;
            HCUERPO[r->tam < U_HTTP_MAX_CPO ? r->tam
                                            : U_HTTP_MAX_CPO] = 0;
            return NSFETCH_OK;
        }
        if (++redir > 3) return NSFETCH_E_REDIR;
        {
            char loc[U_HTTP_MAX_URL + 1];
            char nxt[U_HTTP_MAX_URL + 1];
            int q;
            if (u_http_cabecera(&h, "Location", loc,
                                (int)sizeof(loc)) < 0)
                return NSFETCH_E_REDIR;
            if (u_html_resuelve_url(cur, loc, nxt)) return NSFETCH_E_URL;
            for (q = 0; nxt[q] && q < U_HTTP_MAX_URL; q++)
                cur[q] = nxt[q];
            cur[q] = 0;
        }
    }
}
