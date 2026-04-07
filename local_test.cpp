// Simple sanity test for the buddy allocator
#include "src.hpp"
#include <iostream>
int main(){
    init(64, 4);
    int a = malloc(8);
    std::cout << a << std::endl;
    int b = malloc(4);
    std::cout << b << std::endl;
    int c = malloc_at(16, 16);
    std::cout << c << std::endl;
    free_at(b,4);
    int d = malloc(4);
    std::cout << d << std::endl;
    return 0;
}
