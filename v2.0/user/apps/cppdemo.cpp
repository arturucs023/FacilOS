/* cppdemo: demostracion F32 C++ en Ring 3 (v2.4.2).
 * Clases, herencia + virtual, template, namespace, operador,
 * memoria dinamica, objeto global y APIs C de FacilOS.
 * OJO C++: no definir __main (lo pone el runtime f32cxx). */
#include "ulib.h"
#include "sys.h"

namespace demo {

struct Punto {
    int x, y;
    Punto(int a, int b) : x(a), y(b) {}
    Punto operator+(const Punto &o) const { return Punto(x + o.x, y + o.y); }
};

class Forma {
  public:
    virtual int lados() const { return 0; }
    virtual ~Forma() {}
};

class Triangulo : public Forma {
    int l;
  public:
    explicit Triangulo(int l_) : l(l_) { u_print("ctor Triangulo\n"); }
    ~Triangulo() { u_print("dtor Triangulo\n"); }
    int lados() const { return 3 * l / l; }
};

template <typename T> T doble(T v) { return v + v; }

template <typename T> struct Caja {
    T v;
    explicit Caja(T v_) : v(v_) {}
    T dos() const { return v + v; }
};

static int nglobales = 0;

class Testigo {
  public:
    Testigo() {
        nglobales++;
        u_print("ctor global\n");
    }
    ~Testigo() { u_print("dtor global\n"); }
};

static Testigo testigo;

} // namespace demo

int main(void) {
    int cs;
    __asm__ volatile("mov %%cs,%0" : "=r"(cs));
    u_print((cs & 3) == 3 ? "cppdemo: anillo 3\n" : "cppdemo: ANILLO MAL\n");

    demo::Punto a(3, 4), b(1, 2);
    demo::Punto c = a + b;
    u_print("punto=");
    u_print_num(c.x);
    u_print(",");
    u_print_num(c.y);
    u_print("\n");

    demo::Triangulo *t = new demo::Triangulo(2);
    demo::Forma *f = t;
    u_print("lados=");
    u_print_num(f->lados());
    u_print("\n");
    delete t;

    int *v = new int[4];
    for (int i = 0; i < 4; i++) v[i] = i * 10;
    u_print("v3=");
    u_print_num(v[3]);
    u_print("\n");
    delete[] v;

    u_print("doble=");
    u_print_num(demo::doble(21));
    u_print("\n");
    demo::Caja<int> cj(20);
    u_print("caja=");
    u_print_num(cj.dos());
    u_print("\n");

    int t0 = sys_ticks();
    char *buf = (char *)u_malloc(32);
    u_strcpy(buf, "interop-ok");
    u_print(buf);
    u_print("\n");
    u_free(buf);
    u_print("ticks+");
    u_print_num(sys_ticks() - t0 >= 0 ? 1 : 0);
    u_print("\n");

    u_print("globales=");
    u_print_num(demo::nglobales);
    u_print("\n");
    u_print("cppdemo: fin\n");
    return 0;
}
