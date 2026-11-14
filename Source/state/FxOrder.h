#pragma once
#include <array>

// The order the six in-line FX tools run in. Doubler, Harmony and Shift are not part of it: they work side by
// side on the clean voice (they have to hear it to follow its pitch), and what they make goes into this chain.
// The order is kept as one number, so it travels with presets, A/B and the song like any other control.
// Number 0 is the order every build up to 16 used: Phone, Lo-fi, Motion, Chop, Delay, Reverb.
namespace mu::fxorder
{
enum Tool { Phone = 0, Lofi, Motion, Chop, Delay, Reverb, kCount };
constexpr int kOrders = 720;                                   // 6 factorial
using Order = std::array<int, kCount>;

inline const char* moduleId (int tool)
{
    static const char* const ids[kCount] = { "phone", "lofi", "motion", "chop", "delay", "reverb" };
    return tool >= 0 && tool < kCount ? ids[tool] : "";
}

inline Order decode (int number)
{
    number = number < 0 ? 0 : number >= kOrders ? kOrders - 1 : number;
    int left[kCount] = { 0, 1, 2, 3, 4, 5 };
    int radix = 120;                                           // 5 factorial
    Order out {};
    for (int place = 0; place < kCount; ++place)
    {
        const int remaining = kCount - place;
        const int pick = number / radix;
        number %= radix;
        out[(size_t) place] = left[pick];
        for (int i = pick; i < remaining - 1; ++i) left[i] = left[i + 1];
        if (remaining > 1) radix /= (remaining - 1);
    }
    return out;
}

inline int encode (const Order& order)
{
    int left[kCount] = { 0, 1, 2, 3, 4, 5 };
    int radix = 120, number = 0;
    for (int place = 0; place < kCount; ++place)
    {
        const int remaining = kCount - place;
        int pick = 0;
        while (pick < remaining && left[pick] != order[(size_t) place]) ++pick;
        if (pick >= remaining) return 0;                       // not a real order: fall back to the standard one
        number += pick * radix;
        for (int i = pick; i < remaining - 1; ++i) left[i] = left[i + 1];
        if (remaining > 1) radix /= (remaining - 1);
    }
    return number;
}

// the order with one tool moved to another place, everything else keeping its sequence
inline Order moved (Order order, int tool, int toPlace)
{
    int from = 0;
    while (from < kCount && order[(size_t) from] != tool) ++from;
    if (from >= kCount) return order;
    toPlace = toPlace < 0 ? 0 : toPlace >= kCount ? kCount - 1 : toPlace;
    while (from < toPlace) { order[(size_t) from] = order[(size_t) from + 1]; ++from; }
    while (from > toPlace) { order[(size_t) from] = order[(size_t) from - 1]; --from; }
    order[(size_t) toPlace] = tool;
    return order;
}
}
