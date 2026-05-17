#include <cstddef>

struct RingBuffer {
    RingBuffer(size_t capacity);
    bool push(int val);
    int pop();
};