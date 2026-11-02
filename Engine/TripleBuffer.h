#pragma once
// Lock-free single-producer / single-consumer triple buffer.
// The message thread fills writeBuffer() and calls publish(); the audio thread calls acquire()
// at the top of each block and, if it returns true (or regardless), reads read(). Neither side
// ever blocks, allocates or sees a half-written value.
#include <array>
#include <atomic>

namespace ms
{
template <class T>
class TripleBuffer
{
public:
    T& writeBuffer() { return bufs[size_t (writeIdx)]; }

    void publish()
    {
        const int prev = middle.exchange (writeIdx | kDirty, std::memory_order_acq_rel);
        writeIdx = prev & 3;
    }

    /** Consumer: swaps in the newest published buffer. Returns true if there was one. */
    bool acquire()
    {
        if ((middle.load (std::memory_order_relaxed) & kDirty) == 0) return false;
        const int prev = middle.exchange (readIdx, std::memory_order_acq_rel);
        readIdx = prev & 3;
        return true;
    }

    const T& read() const { return bufs[size_t (readIdx)]; }

private:
    static constexpr int kDirty = 4;
    std::array<T, 3> bufs {};
    int writeIdx = 0, readIdx = 1;   // each owned by exactly one thread
    std::atomic<int> middle { 2 };
};
} // namespace ms
