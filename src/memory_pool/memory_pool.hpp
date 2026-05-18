#pragma once

#include <cstddef>
#include <cstdlib>
#include <algorithm>
#include <new>

struct FreeList {
    FreeList *next;
};

template<typename T>
struct MemoryPool {
    FreeList *headFreeList_;
    char *chunk_;

    MemoryPool(size_t capacity_) {
        chunk = reinterpret_cast<char*>(malloc(capacity_));

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

            FreeList *nextFreeList =
                reinterpret_cast<FreeList*>(next);

            curr->next = nextFreeList;
            curr = curr->next;
        }

        curr->next = nullptr;
    }

    ~MemoryPool() {
        free(chunk_);
    }

    T* allocate() {
        if (headFreeList_ == nullptr) {
            return nullptr;
        }

        FreeList *curr = headFreeList_;
        headFreeList_ = curr->next;

        T* result =
            new(reinterpret_cast<void*>(curr)) T();

        return result;
    }

    void deallocate(T* ptr) {
        ptr->~T();

        FreeList *curr =
            reinterpret_cast<FreeList*>(ptr);

        curr->next = headFreeList_;
        headFreeList_ = curr;
    }
};