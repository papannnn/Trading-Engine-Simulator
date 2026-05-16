#include <stdlib.h>
#include <algorithm>

struct FreeList;

struct MemoryPool {
    FreeList *freeListHead;
    MemoryPool();

    int* allocate();
    void deallocate(int*);
};