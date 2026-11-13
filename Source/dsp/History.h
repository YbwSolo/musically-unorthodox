#pragma once
#include <array>
#include <atomic>

namespace mu
{
// Tiny lock-free scrolling history written by the audio thread and read by the UI.
// A torn read only ever shows one stale point, which is fine for a display.
template <int Size>
class History
{
public:
    void push (float v) noexcept
    {
        const int w = writePos.load (std::memory_order_relaxed);
        data[(size_t) w] = v;
        writePos.store ((w + 1) % Size, std::memory_order_release);
    }
    // age 0 = newest
    float get (int age) const noexcept
    {
        const int w = writePos.load (std::memory_order_acquire);
        return data[(size_t) (((w - 1 - age) % Size + Size) % Size)];
    }
    void fill (float v) noexcept { data.fill (v); }
    static constexpr int size = Size;

private:
    std::array<float, (size_t) Size> data {};
    std::atomic<int> writePos { 0 };
};
}
