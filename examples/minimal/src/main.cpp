#include <iostream>

// External function
int add(int a, int b);

int main() {
    int a = 10;
    int b = 20;
    std::cout << "Adding " << a << " and " << b << " gives " << add(a, b) << std::endl;
    return 0;
}
