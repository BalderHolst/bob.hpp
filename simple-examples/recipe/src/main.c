#include <stdio.h>

// External function
int add(int a, int b);

int main() {
    int a = 10;
    int b = 20;
    printf("Adding %d and %d gives: %d\n", a, b, add(a, b));
    return 0;
}
