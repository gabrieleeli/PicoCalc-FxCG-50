// ereditarieta', funzioni virtuali, classi astratte, distruttori (LP-05)
#include <iostream>
#include <string>
#include <vector>

class Shape {
public:
    virtual ~Shape() {}
    virtual double area() const = 0;
    virtual std::string name() const { return "forma"; }
    void print() const { std::cout << name() << " " << area() << "\n"; }
};
class Rect : public Shape {
protected:
    double w, h;
public:
    Rect(double w, double h) : w(w), h(h) {}
    double area() const override { return w * h; }
    std::string name() const override { return "rettangolo"; }
};
class Square : public Rect {
public:
    explicit Square(double s) : Rect(s, s) {}
    std::string name() const override { return "quadrato di lato " + std::to_string(static_cast<int>(w)); }
};
class Circle : public Shape {
    double r;
public:
    Circle(double r) : r(r) {}
    double area() const override { return 3.0 * r * r; }
};

class Base {
public:
    Base() { std::cout << "Base() "; }
    virtual ~Base() { std::cout << "~Base() "; }
    virtual void hello() { std::cout << "Base::hello "; }
};
class Derived : public Base {
public:
    Derived() { std::cout << "Derived() "; }
    ~Derived() override { std::cout << "~Derived() "; }
    void hello() override {
        Base::hello();
        std::cout << "Derived::hello ";
    }
};

int main() {
    std::vector<Shape*> v;
    v.push_back(new Rect(2, 3));
    v.push_back(new Square(2));
    v.push_back(new Circle(1));
    double tot = 0;
    for (Shape* s : v) {
        s->print();
        tot += s->area();
    }
    std::cout << "totale " << tot << "\n";
    for (Shape* s : v) delete s;
    Base* p = new Derived();
    p->hello();
    delete p;
    std::cout << "\n";
    {
        Derived d;
        std::cout << "| ";
    }
    std::cout << "\n";
    return 0;
}
