// main.cpp

#include <iostream>

extern "C" {
    int add(int a, int b);
}

int main() {
    int wynik = add(10, 5);
    std::cout << "ARM ASM:: " << wynik << std::endl;
    return 0;
}