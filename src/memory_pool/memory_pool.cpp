#include <cstddef>
#include <cstdlib>
#include <algorithm>
#include "memory_pool.hpp"

struct FreeList {
    FreeList *next;
};

template<typename T>
MemoryPool<T>::MemoryPool(size_t capacity_) {
    char *chunk = reinterpret_cast<char*>(malloc(capacity_));

    size_t itemSize = std::max(sizeof(FreeList), sizeof(T));
    size_t chunkSize = capacity_ / itemSize;

    if (chunkSize == 0) {
        this->headFreeList_ = nullptr;
        return;
    }

    headFreeList_ = reinterpret_cast<FreeList*>(chunk);
    FreeList *curr = headFreeList_;
    for (int i = 1; i < chunkSize; i++) {
        char *next = chunk + (i * itemSize);
        FreeList *nextFreeList = reinterpret_cast<FreeList*>(next);
        curr->next = nextFreeList;
        curr = curr->next;
    }
    curr->next = nullptr;
}

template<typename T>
T* MemoryPool<T>::allocate() {
    if (headFreeList_ == nullptr) {
        return nullptr;
    }

    FreeList *curr = headFreeList_;
    headFreeList_ = curr->next;

    T* result = new(reinterpret_cast<void*>(curr)) T();
    return result;
}

template<typename T>
void MemoryPool<T>::deallocate(T* ptr) {
    ptr->~T();
    FreeList *curr = reinterpret_cast<FreeList*>(ptr);
    curr->next = headFreeList_;
    headFreeList_ = curr;
}