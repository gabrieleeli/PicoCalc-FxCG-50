// new/delete, struct con puntatori, lista concatenata, nullptr (LP-04)
#include <iostream>

struct Node {
    int value;
    Node* next;
    Node(int v, Node* n = nullptr) : value(v), next(n) {}
};

class List {
    Node* head = nullptr;
    int count = 0;
public:
    ~List() {
        while (head) {
            Node* n = head->next;
            delete head;
            head = n;
        }
    }
    void pushFront(int v) {
        head = new Node(v, head);
        ++count;
    }
    void pushBack(int v) {
        Node* n = new Node(v);
        if (!head) head = n;
        else {
            Node* p = head;
            while (p->next) p = p->next;
            p->next = n;
        }
        ++count;
    }
    bool remove(int v) {
        Node** pp = &head;
        while (*pp) {
            if ((*pp)->value == v) {
                Node* dead = *pp;
                *pp = dead->next;
                delete dead;
                --count;
                return true;
            }
            pp = &(*pp)->next;
        }
        return false;
    }
    void reverse() {
        Node* prev = nullptr;
        while (head) {
            Node* n = head->next;
            head->next = prev;
            prev = head;
            head = n;
        }
        head = prev;
    }
    void print() const {
        for (Node* p = head; p != nullptr; p = p->next) std::cout << p->value << (p->next ? " -> " : "");
        std::cout << " (" << count << ")\n";
    }
};

int main() {
    List l;
    for (int i = 1; i <= 4; ++i) l.pushBack(i * 10);
    l.pushFront(5);
    l.print();
    std::cout << l.remove(20) << l.remove(99) << '\n';
    l.reverse();
    l.print();
    int* arr = new int[5];
    for (int i = 0; i < 5; ++i) arr[i] = i * i;
    int s = 0;
    for (int i = 0; i < 5; ++i) s += arr[i];
    delete[] arr;
    std::cout << s << '\n';
    return 0;
}
