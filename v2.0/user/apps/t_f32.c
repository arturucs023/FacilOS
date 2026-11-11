void __main(void) {
}

#define F32_LIB 1
#include "f32.c"

/* t_f32: bateria de F32 Lang (v0.1 y 0.2). Escribe cada programa
   en /t.f32, lo corre con f32_corre y compara salida
   (/f32sal.tmp) y codigo de retorno. Sin red; todo en QEMU. */

static int t_pruebas = 0;
static int t_fallos = 0;

static void t_ok(int cond, const char *nom) {
    char b[96];
    int i = 0, t = 0;
    t_pruebas++;
    if (!cond) {
        t_fallos++;
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

static int t_igual(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == 0 && b[i] == 0;
}

/* Escribe src en ruta (0 ok). */
static int t_escribe(const char *ruta, const char *src) {
    UFILE *f;
    int n = 0;
    while (src[n]) n++;
    f = u_fopen(ruta, "w");
    if (!f) return -1;
    if (n > 0 && u_fwrite(src, n, f) != n) {
        u_fclose(f);
        return -1;
    }
    u_fclose(f);
    return 0;
}

/* Lee /f32sal.tmp en sal (0 ok, "" si no existe/vacio). */
static int t_lee_sal(char *sal, int max) {
    UFILE *f;
    int n, t = 0;
    sal[0] = 0;
    f = u_fopen("/f32sal.tmp", "r");
    if (!f) return -1;
    n = u_fsize(f);
    if (n < 0) n = 0;
    if (n >= max) n = max - 1;
    if (n > 0) {
        if (u_fread(sal, n, f) != n) {
            u_fclose(f);
            return -1;
        }
        t = n;
    }
    sal[t] = 0;
    u_fclose(f);
    return 0;
}

/* Corre un programa: compara rc y salida exacta. */
static void t_corre(const char *nom, const char *ruta, const char *src,
                    int rc_esp, const char *sal_esp) {
    char sal[2048];
    int rc;
    if (t_escribe(ruta, src) != 0) {
        t_ok(0, nom);
        return;
    }
    rc = f32_corre(ruta, 0);
    if (t_lee_sal(sal, (int)sizeof(sal)) != 0) sal[0] = 0;
    t_ok(rc == rc_esp && t_igual(sal, sal_esp), nom);
}

/* Corre esperando FALLO con codigo F32-0xx en la salida. */
static void t_falla(const char *nom, const char *ruta, const char *src,
                    const char *cod) {
    char sal[2048];
    int rc, i = 0, hall = 0;
    if (t_escribe(ruta, src) != 0) {
        t_ok(0, nom);
        return;
    }
    rc = f32_corre(ruta, 0);
    if (t_lee_sal(sal, (int)sizeof(sal)) != 0) sal[0] = 0;
    if (rc == 1) {
        while (sal[i]) {
            int j = 0, bien = 1;
            while (cod[j] && sal[i + j]) {
                if (sal[i + j] != cod[j]) {
                    bien = 0;
                    break;
                }
                j++;
            }
            if (bien && !cod[j]) {
                hall = 1;
                break;
            }
            i++;
        }
    }
    t_ok(rc == 1 && hall, nom);
}

/* F32-0.1: regresion (hola, variables, control, funciones).
   OJO sintaxis 0.1: el cuerpo de programa/funcion/bloque va
   indentado (4 espacios por nivel); fin/finsi/... al nivel
   de su apertura. */
static void prueba_base(void) {
    u_print("t_f32: base...\n");
    t_corre("base-hola", "/t.f32",
            "programa\n"
            "    escribir \"hola\"\n"
            "fin\n",
            0, "hola\n");
    t_corre("base-num", "/t.f32",
            "programa\n"
            "    variable x = 0\n"
            "    x = 40 + 2\n"
            "    escribir x\n"
            "fin\n",
            0, "42\n");
    t_corre("base-si", "/t.f32",
            "programa\n"
            "    variable x = 0\n"
            "    x = 3\n"
            "    si x > 2\n"
            "        escribir \"mayor\"\n"
            "    sino\n"
            "        escribir \"menor\"\n"
            "    finsi\n"
            "fin\n",
            0, "mayor\n");
    t_corre("base-sinosi", "/t.f32",
            "programa\n"
            "    variable x = 0\n"
            "    x = 2\n"
            "    si x > 5\n"
            "        escribir \"a\"\n"
            "    sinosi x > 1\n"
            "        escribir \"b\"\n"
            "    sino\n"
            "        escribir \"c\"\n"
            "    finsi\n"
            "fin\n",
            0, "b\n");
    t_corre("base-mientras", "/t.f32",
            "programa\n"
            "    variable i = 0\n"
            "    variable s = 0\n"
            "    i = 1\n"
            "    s = 0\n"
            "    mientras i <= 4\n"
            "        s = s + i\n"
            "        i = i + 1\n"
            "    finmientras\n"
            "    escribir s\n"
            "fin\n",
            0, "10\n");
    t_corre("base-compara", "/t.f32",
            "programa\n"
            "    escribir 1 < 2\n"
            "    escribir 2 <= 2\n"
            "    escribir 3 > 4\n"
            "    escribir 4 >= 5\n"
            "    escribir 1 == 1\n"
            "    escribir 1 != 2\n"
            "fin\n",
            0,
            "verdadero\nverdadero\nfalso\nfalso\n"
            "verdadero\nverdadero\n");
    t_corre("base-funcion", "/t.f32",
            "programa\n"
            "funcion doble(n)\n"
            "    retornar n * 2\n"
            "finfuncion\n"
            "    escribir doble(21)\n"
            "fin\n",
            0, "42\n");
    t_corre("base-fib", "/t.f32",
            "programa\n"
            "funcion fib(n)\n"
            "    si n < 2\n"
            "        retornar n\n"
            "    finsi\n"
            "    retornar fib(n - 1) + fib(n - 2)\n"
            "finfuncion\n"
            "    escribir fib(10)\n"
            "fin\n",
            0, "55\n");
    t_corre("base-sombra", "/t.f32",
            "programa\n"
            "funcion f()\n"
            "    variable x = 0\n"
            "    x = 2\n"
            "    retornar x\n"
            "finfuncion\n"
            "    variable x = 0\n"
            "    x = 1\n"
            "    escribir f()\n"
            "    escribir x\n"
            "fin\n",
            0, "2\n1\n");
    t_corre("base-bool", "/t.f32",
            "programa\n"
            "    escribir verdadero y falso\n"
            "    escribir verdadero o falso\n"
            "    escribir no falso\n"
            "fin\n",
            0, "falso\nverdadero\nverdadero\n");
    t_falla("base-err-ne", "/t.f32",
            "programa\n"
            "    escribir 1 +\n"
            "fin\n",
            "F32-033");
    t_falla("base-err-nombre", "/t.f32",
            "programa\n"
            "    escribir nose\n"
            "fin\n",
            "F32-042");
    t_falla("base-err-div0", "/t.f32",
            "programa\n"
            "    escribir 1 / 0\n"
            "fin\n",
            "F32-050");
    t_falla("base-err-falta", "/t.f32",
            "programa\n"
            "    escribir f(1)\n"
            "fin\n",
            "F32-083");
    t_falla("base-err-nargs", "/t.f32",
            "programa\n"
            "funcion f(a, b)\n"
            "    retornar a\n"
            "finfuncion\n"
            "    escribir f(1)\n"
            "fin\n",
            "F32-084");
    t_falla("base-err-tipo", "/t.f32",
            "programa\n"
            "    escribir 1 + verdadero\n"
            "fin\n",
            "F32-043");
    t_falla("base-var-sin-valor", "/t.f32",
            "programa\n"
            "    variable x\n"
            "    escribir x\n"
            "fin\n",
            "F32-040");
}

/* F32-0.2: archivos (abrir/leer/escribir/cerrar/existe). */
static void prueba_archivos(void) {
    u_print("t_f32: archivos...\n");
    t_corre("arch-eco", "/t.f32",
            "programa\n"
            "    variable f = abrir(\"/t_f32_datos.txt\", \"escritura\")\n"
            "    escribir_linea(f, \"hola\")\n"
            "    escribir_linea(f, \"mundo\")\n"
            "    cerrar(f)\n"
            "    variable g = abrir(\"/t_f32_datos.txt\", \"lectura\")\n"
            "    escribir leer_linea(g)\n"
            "    escribir leer_linea(g)\n"
            "    cerrar(g)\n"
            "fin\n",
            0, "hola\nmundo\n");
    t_corre("arch-eof", "/t.f32",
            "programa\n"
            "    variable g = abrir(\"/t_f32_datos.txt\", \"lectura\")\n"
            "    escribir leer_linea(g)\n"
            "    escribir leer_linea(g)\n"
            "    escribir leer_linea(g)\n"
            "    cerrar(g)\n"
            "fin\n",
            0, "hola\nmundo\n\n");
    t_corre("arch-anadir", "/t.f32",
            "programa\n"
            "    variable f = abrir(\"/t_f32_datos.txt\", \"anadir\")\n"
            "    escribir_linea(f, \"!\")\n"
            "    cerrar(f)\n"
            "    variable g = abrir(\"/t_f32_datos.txt\", \"lectura\")\n"
            "    escribir leer_linea(g)\n"
            "    escribir leer_linea(g)\n"
            "    escribir leer_linea(g)\n"
            "    cerrar(g)\n"
            "fin\n",
            0, "hola\nmundo\n!\n");
    t_corre("arch-existe", "/t.f32",
            "programa\n"
            "    escribir existe(\"/t_f32_datos.txt\")\n"
            "    escribir existe(\"/no_existe_xyz\")\n"
            "fin\n",
            0, "verdadero\nfalso\n");
    t_falla("arch-noexiste", "/t.f32",
            "programa\n"
            "    variable f = abrir(\"/no_existe_xyz\", \"lectura\")\n"
            "    escribir f\n"
            "fin\n",
            "F32-100");
    t_falla("arch-modo", "/t.f32",
            "programa\n"
            "    variable f = abrir(\"/t_f32_datos.txt\", \"rara\")\n"
            "    escribir f\n"
            "fin\n",
            "F32-104");
    t_falla("arch-doblecierre", "/t.f32",
            "programa\n"
            "    variable f = abrir(\"/t_f32_datos.txt\", \"lectura\")\n"
            "    cerrar(f)\n"
            "    cerrar(f)\n"
            "fin\n",
            "F32-103");
    t_falla("arch-tipo", "/t.f32",
            "programa\n"
            "    escribir_linea(\"hola\", \"x\")\n"
            "fin\n",
            "F32-043");
}

/* F32-0.2: estructuras (arreglos, para, cadenas, registros). */
static void prueba_estructuras(void) {
    u_print("t_f32: estructuras...\n");
    t_corre("arr-lit", "/t.f32",
            "programa\n"
            "    variable a = [1, 2, 3]\n"
            "    escribir a[0]\n"
            "    escribir a[2]\n"
            "    escribir longitud(a)\n"
            "fin\n",
            0, "1\n3\n3\n");
    t_corre("arr-asi", "/t.f32",
            "programa\n"
            "    variable a = [1, 2, 3]\n"
            "    a[1] = 99\n"
            "    escribir a[1]\n"
            "    escribir a\n"
            "fin\n",
            0, "99\n[1, 99, 3]\n");
    t_corre("arr-vacio", "/t.f32",
            "programa\n"
            "    variable a = []\n"
            "    escribir longitud(a)\n"
            "    escribir a\n"
            "fin\n",
            0, "0\n[]\n");
    t_falla("arr-fuera", "/t.f32",
            "programa\n"
            "    variable a = [1]\n"
            "    escribir a[5]\n"
            "fin\n",
            "F32-120");
    t_falla("arr-noarr", "/t.f32",
            "programa\n"
            "    variable a = 1\n"
            "    a[0] = 2\n"
            "fin\n",
            "F32-121");
    t_corre("para-basico", "/t.f32",
            "programa\n"
            "    variable s = 0\n"
            "    para i desde 1 hasta 4\n"
            "        s = s + i\n"
            "    finpara\n"
            "    escribir s\n"
            "fin\n",
            0, "10\n");
    t_corre("para-paso", "/t.f32",
            "programa\n"
            "    variable s = 0\n"
            "    para i desde 0 hasta 10 paso 2\n"
            "        s = s + i\n"
            "    finpara\n"
            "    escribir s\n"
            "fin\n",
            0, "30\n");
    t_corre("para-inv", "/t.f32",
            "programa\n"
            "    variable s = 0\n"
            "    para i desde 3 hasta 1\n"
            "        s = s + i\n"
            "    finpara\n"
            "    escribir s\n"
            "fin\n",
            0, "6\n");
    t_corre("cad-concat", "/t.f32",
            "programa\n"
            "    variable s = \"ho\" + \"la\"\n"
            "    escribir s\n"
            "    escribir s[1]\n"
            "    escribir longitud(s)\n"
            "fin\n",
            0, "hola\no\n4\n");
    t_corre("cad-lex", "/t.f32",
            "programa\n"
            "    escribir \"a\" < \"b\"\n"
            "    escribir \"b\" <= \"b\"\n"
            "    escribir \"c\" > \"b\"\n"
            "    escribir \"b\" >= \"c\"\n"
            "    escribir \"abc\" < \"abd\"\n"
            "fin\n",
            0, "verdadero\nverdadero\nverdadero\nfalso\nverdadero\n");
    t_corre("cad-buscar", "/t.f32",
            "programa\n"
            "    escribir buscar(\"hola mundo\", \"mundo\")\n"
            "    escribir buscar(\"hola\", \"z\")\n"
            "    escribir subcadena(\"hola\", 1, 2)\n"
            "    escribir subcadena(\"hola\", 2)\n"
            "fin\n",
            0, "5\n-1\nol\nla\n");
    t_corre("reg-base", "/t.f32",
            "programa\n"
            "    variable r = [nombre: \"ana\", edad: 30]\n"
            "    escribir r.nombre\n"
            "    escribir r.edad\n"
            "    escribir longitud(r)\n"
            "fin\n",
            0, "ana\n30\n2\n");
    t_corre("reg-asi", "/t.f32",
            "programa\n"
            "    variable r = [a: 1]\n"
            "    r.a = 7\n"
            "    escribir r.a\n"
            "    escribir r\n"
            "fin\n",
            0, "7\n[a: 7]\n");
    t_corre("reg-corchete", "/t.f32",
            "programa\n"
            "    variable r = [a: 5]\n"
            "    escribir r[\"a\"]\n"
            "fin\n",
            0, "5\n");
    t_falla("reg-nocampo", "/t.f32",
            "programa\n"
            "    variable r = [a: 1]\n"
            "    escribir r.b\n"
            "fin\n",
            "F32-122");
    t_falla("reg-noasig", "/t.f32",
            "programa\n"
            "    variable x = 1\n"
            "    x.campo = 2\n"
            "fin\n",
            "F32-121");
    t_corre("reg-eq", "/t.f32",
            "programa\n"
            "    variable a = [1, 2]\n"
            "    variable b = [1, 2]\n"
            "    escribir a == b\n"
            "    variable r1 = [x: 1]\n"
            "    variable r2 = [x: 1]\n"
            "    escribir r1 == r2\n"
            "fin\n",
            0, "verdadero\nverdadero\n");
}

/* F32-0.2: modulos (importar, cualificados, aislamiento). */
static void prueba_modulos(void) {
    u_print("t_f32: modulos...\n");
    t_corre("mod-base", "/m1.f32",
            "programa\n"
            "funcion doble(n)\n"
            "    retornar n * 2\n"
            "finfuncion\n"
            "    variable base = 100\n"
            "fin\n",
            0, "");
    t_corre("mod-uso", "/t.f32",
            "programa\n"
            "importar m1\n"
            "    escribir m1.doble(21)\n"
            "    escribir m1.base\n"
            "fin\n",
            0, "42\n100\n");
    t_corre("mod-unavez", "/m3.f32",
            "programa\n"
            "funcion quien()\n"
            "    retornar 7\n"
            "finfuncion\n"
            "    escribir \"cargo\"\n"
            "fin\n",
            0, "cargo\n");
    t_corre("mod-doble-import", "/t.f32",
            "programa\n"
            "importar m3\n"
            "importar m3\n"
            "    escribir m3.quien()\n"
            "fin\n",
            0, "cargo\n7\n");
    t_falla("mod-inexst", "/t.f32",
            "programa\n"
            "importar nosuch\n"
            "    escribir 1\n"
            "fin\n",
            "F32-086");
    t_corre("mod-circ-a", "/m6a.f32",
            "programa\n"
            "funcion fa()\n"
            "    retornar 1\n"
            "finfuncion\n"
            "fin\n",
            0, "");
    t_corre("mod-circ-b", "/m6b.f32",
            "programa\n"
            "importar m6a\n"
            "funcion fb()\n"
            "    retornar m6a.fa()\n"
            "finfuncion\n"
            "fin\n",
            0, "");
    t_corre("mod-usa-b", "/t.f32",
            "programa\n"
            "importar m6b\n"
            "    escribir m6b.fb()\n"
            "fin\n",
            0, "1\n");
    {
        /* Circular real: ambos existen y se importan mutuamente. */
        int w1 = t_escribe("/m7a.f32",
                           "programa\n"
                           "importar m7b\n"
                           "fin\n");
        int w2 = t_escribe("/m7b.f32",
                           "programa\n"
                           "importar m7a\n"
                           "fin\n");
        t_ok(w1 == 0 && w2 == 0, "mod-circ-escribe");
        if (w1 == 0 && w2 == 0)
            t_falla("mod-circular", "/m7a.f32",
                    "programa\n"
                    "importar m7b\n"
                    "fin\n",
                    "F32-087");
    }
    t_falla("mod-nofunc", "/t.f32",
            "programa\n"
            "importar m1\n"
            "    escribir m1.nada()\n"
            "fin\n",
            "F32-083");
    t_falla("mod-nomod", "/t.f32",
            "programa\n"
            "    escribir zz.top()\n"
            "fin\n",
            "F32-086");
    t_falla("mod-fuera-nivel", "/t.f32",
            "programa\n"
            "    si 1 == 1\n"
            "        importar m1\n"
            "    finsi\n"
            "fin\n",
            "F32-016");
    t_corre("mod-sombra-mod", "/t.f32",
            "programa\n"
            "funcion doble(n)\n"
            "    retornar n * 100\n"
            "finfuncion\n"
            "importar m1\n"
            "    escribir doble(2)\n"
            "    escribir m1.doble(2)\n"
            "fin\n",
            0, "200\n4\n");
    t_falla("mod-args", "/t.f32",
            "programa\n"
            "importar m1\n"
            "    escribir m1.doble(1, 2)\n"
            "fin\n",
            "F32-084");
}

/* F32-0.1: cadenas y comparaciones de texto. */
static void prueba_texto(void) {
    u_print("t_f32: texto...\n");
    t_corre("texto-igual", "/t.f32",
            "programa\n"
            "    escribir \"a\" == \"a\"\n"
            "    escribir \"a\" != \"b\"\n"
            "    escribir \"a\" == \"b\"\n"
            "fin\n",
            0, "verdadero\nverdadero\nfalso\n");
    t_corre("texto-var", "/t.f32",
            "programa\n"
            "    variable s = \"\"\n"
            "    s = \"hola\"\n"
            "    escribir s\n"
            "fin\n",
            0, "hola\n");
}

int main(void) {
    u_print("t_f32: bateria F32 Lang (v0.1)...\n");
    prueba_base();
    prueba_texto();
    prueba_modulos();
    prueba_archivos();
    prueba_estructuras();
    {
        char nb[12];
        int q = 0;
        u_print("t_f32: ");
        u_itoa(t_pruebas, nb);
        while (nb[q]) {
            char b2[2];
            b2[0] = nb[q++];
            b2[1] = 0;
            u_print(b2);
        }
        u_print(" pruebas ");
        q = 0;
        u_itoa(t_fallos, nb);
        while (nb[q]) {
            char b2[2];
            b2[0] = nb[q++];
            b2[1] = 0;
            u_print(b2);
        }
        u_print(" fallos\n");
        return t_fallos ? 1 : 0;
    }
}
