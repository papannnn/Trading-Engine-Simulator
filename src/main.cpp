#include <iostream>
#include "memory_pool/memory_pool.hpp"

int main () {
    MemoryPool pool;
    int* a = pool.allocate();
    *a = 10;
    std::cout << *a << std::endl;
    pool.deallocate(a);
    std::cout << *a << std::endl;
}