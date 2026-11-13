/* imgdec: PNG + JPEG (baseline) propios, freestanding.
 *
 * Sin UNIX/BSD/Linux/Windows: solo aritmetica entera, sin libc del
 * anfitrion, sin FPU (IDCT entera). La memoria sale de u_malloc/u_free
 * (en host de prueba: malloc/free via IMGDEC_HOST).
 *
 * Salida RGB24 en heap (el llamador la libera). 0 ok, -1 error.
 */
#ifndef IMGDEC_H
#define IMGDEC_H

int imgdec_png(const unsigned char *d, int n, int maxpx,
               int *w, int *h, unsigned char **rgb);
int imgdec_jpg(const unsigned char *d, int n, int maxpx,
               int *w, int *h, unsigned char **rgb);

#endif
