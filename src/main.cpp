#include <iostream>
#include <string>
#include "memory_pool/memory_pool.hpp"

struct Person {
    std::string name;
    int age;
};

int main () {
    MemoryPool<Person> pool(1000);
    Person* a = pool.allocate();
    
    a->age = 10;
    a->name = "hendra";
    std::cout << a->name << std::endl;
    std::cout << a->age << std::endl;
    pool.deallocate(a);
    // *a = 10;
    Person* b = pool.allocate();
    std::cout << b->name << std::endl;
    std::cout << b->age << std::endl;
}