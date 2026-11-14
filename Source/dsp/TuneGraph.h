#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <memory>
#include <vector>

namespace mu
{
// One sung note in the graph view: where it is in the song, the pitch it was sung at and the pitch it is moved to.
struct GraphNote
{
    int start = 0, end = 0;        // frames, 100 a second, counted from the start of the song; end is one past the last
    float sung = 0.0f;             // the centre of the note as it was sung (MIDI note number with fraction)
    float pitch = 0.0f;            // the centre it is moved to
    float straighten = 0.0f;       // 0..1: how much of the wobble and drift inside the note is flattened
    float glideMs = 40.0f;         // how long the move from the previous note's correction to this one takes
    bool isEdited() const noexcept { return std::abs (pitch - sung) > 0.005f || straighten > 0.005f; }
};

// The hand-edited side of Tune. The plugin cannot see the audio file, so the graph is built by listening: with
// capture on, the pitch of the voice is written down against the song position as the song plays. That record is
// split into notes, each of which can be moved, snapped to the scale and straightened. From the notes a target
// pitch is worked out for every hundredth of a second of the song, and during playback Tune moves the voice to it.
//
// The pitch record and the target curve are plain arrays of atomics, written from one thread and read from the
// other without locks. The list of notes belongs to the message thread.
class TuneGraph
{
public:
    static constexpr int kRate = 100;                    // frames a second
    static constexpr int kMaxFrames = kRate * 60 * 12;   // twelve minutes of song

    void allocate();                                      // message thread, before capture or playback can use it
    bool isReady() const noexcept { return ready.load (std::memory_order_acquire); }
    static int frameAt (double seconds) noexcept { return (int) std::floor (seconds * (double) kRate); }

    // audio thread
    void capture (int frame, float note) noexcept;        // note 0 = nothing pitched there
    float targetAt (int frame) const noexcept;            // 0 = leave the voice as it is
    void setCapturing (bool on) noexcept { if (on) { passLo.store (kMaxFrames); passHi.store (-1); } capturing.store (on); }
    int passFirst() const noexcept { return passLo.load(); }           // the stretch written during the current or last capture
    int passLast() const noexcept { return passHi.load(); }
    bool isCapturing() const noexcept { return capturing.load (std::memory_order_relaxed); }

    // message thread
    float sungAt (int frame) const noexcept;
    float shownTargetAt (int frame) const noexcept { return targetAt (frame); }
    int firstFrame() const noexcept { return first.load(); }       // the stretch that has been captured; first > last when empty
    int lastFrame() const noexcept { return last.load(); }
    bool isEmpty() const noexcept { return first.load() > last.load(); }

    std::vector<GraphNote>& getNotes() noexcept { return notes; }
    const std::vector<GraphNote>& getNotes() const noexcept { return notes; }
    void segment (int from, int to);                       // rebuild the notes in this stretch from the captured pitch
    void rasterise();                                      // notes -> target curve
    int noteAt (int frame) const noexcept;                 // index, or -1
    int splitNote (int index, int frame);                  // returns the index of the new right-hand note, or -1
    bool mergeWithNext (int index);
    void clear();

    juce::String toString() const;                         // for saving with the song
    bool fromString (const juce::String&);

private:
    void measure (GraphNote& n) const;

    std::unique_ptr<std::atomic<float>[]> sung, target;
    std::atomic<bool> ready { false }, capturing { false };
    std::atomic<int> first { kMaxFrames }, last { -1 }, passLo { kMaxFrames }, passHi { -1 };
    std::vector<GraphNote> notes;
};
}
