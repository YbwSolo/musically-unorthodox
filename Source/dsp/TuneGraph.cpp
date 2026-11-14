#include "TuneGraph.h"
#include <algorithm>
#include <cmath>

namespace mu
{
void TuneGraph::allocate()
{
    if (ready.load()) return;
    sung = std::make_unique<std::atomic<float>[]> ((size_t) kMaxFrames);
    target = std::make_unique<std::atomic<float>[]> ((size_t) kMaxFrames);
    for (int i = 0; i < kMaxFrames; ++i) { sung[(size_t) i].store (0.0f, std::memory_order_relaxed); target[(size_t) i].store (0.0f, std::memory_order_relaxed); }
    ready.store (true, std::memory_order_release);
}

void TuneGraph::capture (int frame, float note) noexcept
{
    if (! isReady() || frame < 0 || frame >= kMaxFrames) return;
    sung[(size_t) frame].store (note, std::memory_order_relaxed);
    target[(size_t) frame].store (0.0f, std::memory_order_relaxed);       // what was planned for the old take no longer applies
    if (frame < first.load (std::memory_order_relaxed)) first.store (frame, std::memory_order_relaxed);
    if (frame > last.load (std::memory_order_relaxed)) last.store (frame, std::memory_order_relaxed);
    if (frame < passLo.load (std::memory_order_relaxed)) passLo.store (frame, std::memory_order_relaxed);
    if (frame > passHi.load (std::memory_order_relaxed)) passHi.store (frame, std::memory_order_relaxed);
}

float TuneGraph::targetAt (int frame) const noexcept
{
    if (! isReady() || frame < 0 || frame >= kMaxFrames) return 0.0f;
    return target[(size_t) frame].load (std::memory_order_relaxed);
}

float TuneGraph::sungAt (int frame) const noexcept
{
    if (! isReady() || frame < 0 || frame >= kMaxFrames) return 0.0f;
    return sung[(size_t) frame].load (std::memory_order_relaxed);
}

void TuneGraph::clear()
{
    notes.clear();
    if (! isReady()) return;
    for (int i = 0; i < kMaxFrames; ++i) { sung[(size_t) i].store (0.0f, std::memory_order_relaxed); target[(size_t) i].store (0.0f, std::memory_order_relaxed); }
    first.store (kMaxFrames); last.store (-1);
}

// The centre of a note is the middle value of its pitch, which ignores the scoop in and the fall off the end.
void TuneGraph::measure (GraphNote& n) const
{
    std::vector<float> values;
    values.reserve ((size_t) juce::jmax (1, n.end - n.start));
    for (int f = n.start; f < n.end; ++f) { const float v = sungAt (f); if (v > 0.0f) values.push_back (v); }
    if (values.empty()) { n.sung = 0.0f; return; }
    std::nth_element (values.begin(), values.begin() + (std::ptrdiff_t) (values.size() / 2), values.end());
    n.sung = values[values.size() / 2];
}

void TuneGraph::segment (int from, int to)
{
    if (! isReady()) return;
    from = juce::jlimit (0, kMaxFrames, from); to = juce::jlimit (0, kMaxFrames, to);
    // a note that reaches into the stretch is cut back to the part outside it
    std::vector<GraphNote> kept;
    for (auto n : notes)
    {
        if (n.end <= from || n.start >= to) { kept.push_back (n); continue; }
        if (n.start < from) { auto a = n; a.end = from; if (a.end - a.start >= 4) kept.push_back (a); }
        if (n.end > to)     { auto b = n; b.start = to; if (b.end - b.start >= 4) kept.push_back (b); }
    }

    std::vector<GraphNote> found;
    int f = from;
    while (f < to)
    {
        while (f < to && sungAt (f) <= 0.0f) ++f;
        if (f >= to) break;
        // one stretch of singing: it ends at the first gap longer than 30 ms
        int runEnd = f, gap = 0;
        for (int i = f; i < to; ++i)
        {
            if (sungAt (i) > 0.0f) { runEnd = i + 1; gap = 0; }
            else if (++gap > 3) break;
        }
        // inside it, a new note starts when the pitch has sat more than three quarters of a semitone away from
        // the current note for 50 ms
        int noteStart = f, far = 0;
        float centre = sungAt (f);
        int counted = 1;
        auto close = [&] (int end)
        {
            if (end - noteStart < 1) return;
            GraphNote n; n.start = noteStart; n.end = end;
            measure (n);
            if (n.sung > 0.0f) { n.pitch = n.sung; found.push_back (n); }
        };
        for (int i = f + 1; i < runEnd; ++i)
        {
            const float v = sungAt (i);
            if (v <= 0.0f) continue;
            if (std::abs (v - centre) > 0.75f)
            {
                if (++far >= 5)
                {
                    const int split = i - 4;
                    close (split);
                    noteStart = split; centre = v; counted = 1; far = 0;
                }
            }
            else
            {
                far = 0;
                ++counted;
                centre += (v - centre) / (float) juce::jmin (counted, 12);
            }
        }
        close (runEnd);
        f = runEnd;
    }

    // a sliver shorter than 50 ms is not a note: it joins the neighbour it touches, or is dropped
    std::vector<GraphNote> tidy;
    for (size_t i = 0; i < found.size(); ++i)
    {
        auto n = found[i];
        if (n.end - n.start >= 5) { tidy.push_back (n); continue; }
        if (! tidy.empty() && tidy.back().end == n.start && std::abs (tidy.back().sung - n.sung) < 2.0f) { tidy.back().end = n.end; continue; }
        if (i + 1 < found.size() && found[i + 1].start == n.end && std::abs (found[i + 1].sung - n.sung) < 2.0f) { found[i + 1].start = n.start; continue; }
    }
    for (auto& n : tidy) { measure (n); n.pitch = n.sung; }

    kept.insert (kept.end(), tidy.begin(), tidy.end());
    std::sort (kept.begin(), kept.end(), [] (const GraphNote& a, const GraphNote& b) { return a.start < b.start; });
    notes = std::move (kept);
    rasterise();
}

void TuneGraph::rasterise()
{
    if (! isReady()) return;
    const int lo = juce::jmax (0, first.load()), hi = juce::jmin (kMaxFrames - 1, last.load());
    for (int f = lo; f <= hi; ++f) target[(size_t) f].store (0.0f, std::memory_order_relaxed);

    // Notes that touch are handled as one phrase. Each note asks for its own correction (how far its centre is
    // moved). Stepping from one correction to the next at the join would make the pitch jump, so the step is
    // smoothed, and smoothed the same amount either side of the join: half of the glide happens at the end of the
    // one note and half at the start of the next, which is where a singer slides anyway.
    std::vector<float> wanted, k;
    size_t i = 0;
    while (i < notes.size())
    {
        size_t j = i;
        while (j + 1 < notes.size() && notes[j + 1].start - notes[j].end <= 6) ++j;
        const int from = juce::jmax (0, notes[i].start), to = juce::jmin (kMaxFrames, notes[j].end);
        if (to > from)
        {
            wanted.assign ((size_t) (to - from), 0.0f);
            k.assign ((size_t) (to - from), 1.0f);
            for (size_t n = i; n <= j; ++n)
            {
                const auto& note = notes[n];
                const int end = n < j ? notes[n + 1].start : note.end;           // a short gap takes the note before it
                const float step = 1.0f - std::exp (-10.0f / juce::jmax (1.0f, note.glideMs * 0.5f));
                for (int f = juce::jmax (from, note.start); f < juce::jmin (to, end); ++f)
                {
                    wanted[(size_t) (f - from)] = note.sung > 0.0f ? note.pitch - note.sung : 0.0f;
                    k[(size_t) (f - from)] = step;
                }
            }
            float run = wanted.front();
            for (size_t f = 0; f < wanted.size(); ++f) { run += (wanted[f] - run) * k[f]; wanted[f] = run; }
            run = wanted.back();
            for (size_t f = wanted.size(); f-- > 0;) { run += (wanted[f] - run) * k[f]; wanted[f] = run; }

            for (size_t n = i; n <= j; ++n)
            {
                const auto& note = notes[n];
                if (note.sung <= 0.0f) continue;
                for (int f = juce::jmax (from, note.start); f < juce::jmin (to, note.end); ++f)
                {
                    const float s = sungAt (f);
                    if (s <= 0.0f) continue;
                    const float move = wanted[(size_t) (f - from)];
                    // a stretch nobody has touched is left exactly as it was sung
                    const float t = note.isEdited() || std::abs (move) > 0.01f ? s + move - note.straighten * (s - note.sung) : 0.0f;
                    target[(size_t) f].store (t, std::memory_order_relaxed);
                }
            }
        }
        i = j + 1;
    }
}

int TuneGraph::noteAt (int frame) const noexcept
{
    for (size_t i = 0; i < notes.size(); ++i)
        if (frame >= notes[i].start && frame < notes[i].end) return (int) i;
    return -1;
}

int TuneGraph::splitNote (int index, int frame)
{
    if (index < 0 || index >= (int) notes.size()) return -1;
    auto left = notes[(size_t) index];
    if (frame - left.start < 4 || left.end - frame < 4) return -1;
    auto right = left;
    left.end = frame; right.start = frame;
    const float offset = left.pitch - left.sung;
    measure (left); measure (right);
    left.pitch = left.sung + offset; right.pitch = right.sung + offset;     // both halves keep the move the whole note had
    notes[(size_t) index] = left;
    notes.insert (notes.begin() + index + 1, right);
    rasterise();
    return index + 1;
}

bool TuneGraph::mergeWithNext (int index)
{
    if (index < 0 || index + 1 >= (int) notes.size()) return false;
    auto a = notes[(size_t) index];
    const auto b = notes[(size_t) index + 1];
    if (b.start - a.end > 10) return false;
    const float offset = a.pitch - a.sung;
    a.end = b.end;
    measure (a);
    a.pitch = a.sung + offset;
    a.straighten = juce::jmax (a.straighten, b.straighten);
    notes[(size_t) index] = a;
    notes.erase (notes.begin() + index + 1);
    rasterise();
    return true;
}

//==================================================================== saving
juce::String TuneGraph::toString() const
{
    if (! isReady() || isEmpty()) return {};
    const int lo = first.load(), hi = last.load();
    juce::MemoryOutputStream pitch;
    for (int f = lo; f <= hi; ++f) pitch.writeShort ((short) juce::jlimit (0, 32767, juce::roundToInt (sungAt (f) * 256.0f)));
    juce::String s;
    s << "1|" << lo << "|" << (hi - lo + 1) << "|" << juce::Base64::toBase64 (pitch.getData(), pitch.getDataSize()) << "|";
    for (auto& n : notes)
        s << n.start << "," << n.end << "," << juce::String (n.sung, 3) << "," << juce::String (n.pitch, 3) << "," << juce::String (n.straighten, 2) << "," << juce::String (n.glideMs, 0) << ";";
    return s;
}

bool TuneGraph::fromString (const juce::String& text)
{
    const auto parts = juce::StringArray::fromTokens (text, "|", "");
    if (parts.size() < 5 || parts[0] != "1") return false;
    const int lo = parts[1].getIntValue(), count = parts[2].getIntValue();
    if (lo < 0 || count <= 0 || lo + count > kMaxFrames) return false;
    juce::MemoryOutputStream decoded;
    if (! juce::Base64::convertFromBase64 (decoded, parts[3]) || (int) decoded.getDataSize() < count * 2) return false;
    allocate();
    clear();
    juce::MemoryInputStream in (decoded.getData(), decoded.getDataSize(), false);
    for (int i = 0; i < count; ++i) sung[(size_t) (lo + i)].store ((float) (unsigned short) in.readShort() / 256.0f, std::memory_order_relaxed);
    first.store (lo); last.store (lo + count - 1);
    for (auto& item : juce::StringArray::fromTokens (parts[4], ";", ""))
    {
        const auto v = juce::StringArray::fromTokens (item, ",", "");
        if (v.size() < 6) continue;
        GraphNote n;
        n.start = v[0].getIntValue(); n.end = v[1].getIntValue(); n.sung = v[2].getFloatValue(); n.pitch = v[3].getFloatValue();
        n.straighten = juce::jlimit (0.0f, 1.0f, v[4].getFloatValue()); n.glideMs = juce::jlimit (0.0f, 500.0f, v[5].getFloatValue());
        if (n.end > n.start && n.start >= 0 && n.end <= kMaxFrames && n.sung > 0.0f) notes.push_back (n);
    }
    rasterise();
    return true;
}
}
