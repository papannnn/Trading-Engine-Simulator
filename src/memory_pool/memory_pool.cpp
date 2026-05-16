#include <stdlib.h>
#include <algorithm>
#include "memory_pool.hpp"

struct FreeList {
    FreeList* next;
};

MemoryPool::MemoryPool() {
    char *chunk = reinterpret_cast<char*>(malloc(1024));
    size_t blockSize = std::max(sizeof(FreeList), sizeof(int));
    size_t blockCnt = 1024 / blockSize;

    this->freeListHead = reinterpret_cast<FreeList*>(chunk);
    FreeList* curr = this->freeListHead;
    for (int i = 1; i < blockCnt; i++) {
        char* next = chunk + (blockSize * i);
        FreeList* nextPtr = reinterpret_cast<FreeList*>(next);
        curr->next = nextPtr;
        curr = nextPtr;
    }

    curr->next = nullptr;
}

int* MemoryPool::allocate() {
    if (this->freeListHead == nullptr) {
        return nullptr;
    }

    FreeList* curr = freeListHead;
    freeListHead = freeListHead->next;
    return reinterpret_cast<int*>(curr);
}

void MemoryPool::deallocate(int* ptr) {
    FreeList* block = reinterpret_cast<FreeList*>(ptr);
    block->next = this->freeListHead;
    this->freeListHead = block;
}