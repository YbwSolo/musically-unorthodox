#include "Tuner.h"

namespace mu
{
void Tuner::prepare (double sampleRate, int, int numChannels)
{
    fs = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    delay = (int) std::round (fs * 0.020);          // builds up to 17, and the comparison tests
    delayLow = (int) std::round (fs * 0.006);       // for recording: the least that still sounds right
    detector.prepare (fs);
    engine.prepare (channels, fs);
    reset();
}

// How far behind the incoming sound the voice is played: 50 ms. The detector's answer describes the sound of
// about 20 ms ago, a slice is decided one period before its middle, and the tuner waits 20 ms for the next two
// answers before believing one (see settle). So by the time a stretch of sound is cut into slices, the tuner
// knows what note it was, where it began and where it ended. The figure was set by measurement: on a voice
// leaping a fifth and on three real takes, 28 to 32 ms plus the wait gave the cleanest note changes, and 25 or
// 34 were measurably rougher.
int Tuner::delayNow() const noexcept
{
    if (settings.lowLatency) return delayLow;
    if (settings.labDelayMs > 0.0f) return (int) std::round (fs * 0.001 * (double) settings.labDelayMs);
    const int look = lookFrames();
    return look == 0 ? delay : (int) std::round (fs * (0.030 + 0.010 * (double) look));
}

// With a few answers in hand, a mistake can be told from a change of note by what came after it.
//  - A leap of more than three semitones is a new note if the next answer agrees with it. If the voice is back
//    where it was one or two answers later, it was a blip (a consonant, a crack, the wrong octave) and the note
//    carries on as if it had not happened. Before, a leap was only believed on its second answer, so every big
//    interval began with 10 to 30 ms of the new note cut up as if it were still the old one: a short gargle at
//    the start of the note.
//  - A moment with no pitch inside a note (a rough patch) is bridged if the same note is there again within
//    the answers in hand. If it is not, the note has ended, and it ends there, not 20 ms later.
Tuner::Heard Tuner::settle (Heard h) noexcept
{
    auto near = [] (float a, float b, float within) { return a > 0.0f && b > 0.0f && std::abs (a - b) <= within; };
    auto carryOn = [&]
    {
        if (++bridgedFrames > 4) { lastNote = 0.0f; lastHz = 0.0f; bridgedFrames = 0; h.note = 0.0f; h.hz = 0.0f; return; }
        h.note = lastNote; h.hz = lastHz; h.clarity = 0.0f; h.bridged = true;
    };
    if (h.note > 0.0f)
    {
        if (lastNote > 0.0f && std::abs (h.note - lastNote) > 3.0f)
        {
            bool real = aheadCount > 0 && near (ahead[0].note, h.note, 1.5f);
            if (real && aheadCount > 1 && near (ahead[1].note, lastNote, 1.5f)) real = false;      // there and back in 20 ms
            if (! real) { carryOn(); return h; }
        }
        lastNote = h.note; lastHz = h.hz; bridgedFrames = 0;
        return h;
    }
    bool comesBack = false;
    for (int k = 0; k < aheadCount; ++k) comesBack = comesBack || near (ahead[(size_t) k].note, lastNote, 2.0f);
    if (lastNote > 0.0f && comesBack) carryOn();
    else { lastNote = 0.0f; lastHz = 0.0f; bridgedFrames = 0; }
    return h;
}

void Tuner::reset()
{
    aheadCount = 0; bridgedFrames = 0; lastNote = 0.0f; lastHz = 0.0f; sinceStart = 100;
    detector.reset();
    engine.reset();
    engine.resetVoice (voice, (float) delayNow());
    voiced = false; ratio = 1.0f; shift = 0.0f; target = -1; switchFrames = 0; lastCaptured = -1; unsureFrames = 0; framesOnNote = 0; snap = true; sliding = 0.0f; recentCount = 0;
    period = (float) (fs / 200.0);
    detectedNote.store (0.0f); outputNote.store (0.0f); targetNote.store (-1);
}

void Tuner::setSettings (const TunerSettings& s) noexcept
{
    settings = s;
    formant = std::pow (2.0f, juce::jlimit (-12.0f, 12.0f, s.formantSemis) / 12.0f);
}

bool Tuner::noteAllowed (int note, int key, int scale, int customMask) noexcept
{
    static const int masks[] = { 0xFFF,                                   // chromatic
                                 (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11),     // major
                                 (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10),     // natural minor
                                 (1 << 0) | (1 << 2) | (1 << 4) | (1 << 7) | (1 << 9),                             // major pentatonic
                                 (1 << 0) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 10) };                          // minor pentatonic
    if (scale == kCustomScale)
    {
        // hand-picked notes are named outright (C, D, F#...), not counted from the key
        const int mask = (customMask & 0xFFF) != 0 ? customMask : 0xFFF;
        return (mask >> (((note % 12) + 12) % 12)) & 1;
    }
    const int degree = (((note - key) % 12) + 12) % 12;
    return (masks[juce::jlimit (0, 4, scale)] >> degree) & 1;
}

int Tuner::scaleMask (int key, int scale, int customMask) noexcept
{
    int mask = 0;
    for (int n = 0; n < 12; ++n) if (noteAllowed (n, key, scale, customMask)) mask |= 1 << n;
    return mask;
}

int Tuner::nearestAllowed (float note, int key, int scale, int customMask) noexcept
{
    const int centre = (int) std::lround (note);
    int best = centre; float bestDistance = 100.0f;
    for (int n = centre - 6; n <= centre + 6; ++n)
        if (noteAllowed (n, key, scale, customMask))
        {
            const float d = std::abs (note - (float) n);
            if (d < bestDistance) { bestDistance = d; best = n; }
        }
    return best;
}

// Runs each time the detector has a fresh answer (every 10 ms): decide where the voice should be moved to.
void Tuner::control (const Heard& heard) noexcept
{
    const double seconds = heard.seconds;
    const float f = heard.hz;
    const bool pitched = heard.note > 0.0f;
    const float note = heard.note;

    // Graph view: with capture on, write the pitch down against the song position, silence included, so a new
    // take replaces the old one.
    const bool graphMode = settings.mode == 1 && graph != nullptr && graph->isReady();
    int frame = -1;
    if (graphMode && playing)
    {
        frame = TuneGraph::frameAt (seconds);
        if (graph->isCapturing())
        {
            const int from = (lastCaptured >= 0 && frame > lastCaptured && frame - lastCaptured <= 4) ? lastCaptured + 1 : frame;
            for (int k = from; k <= frame; ++k) graph->capture (k, note);
            lastCaptured = frame;
        }
        else lastCaptured = -1;
    }
    else lastCaptured = -1;

    if (! pitched)
    {
        // A note does not stop for 10 or 20 ms and start again: that is a rough patch in the voice or a miss by the
        // detector. Carrying on as before through it stops the tuning cutting in and out inside a word.
        // (With answers in hand, settle has already bridged what can be bridged, so no pitch means the note is over.)
        if (lookFrames() == 0 && voiced && ++unsureFrames <= 2) return;
        voiced = false; ratio = 1.0f; shift = 0.0f; target = -1; switchFrames = 0; unsureFrames = 0; framesOnNote = 0; snap = true;
        detectedNote.store (0.0f, std::memory_order_relaxed); outputNote.store (0.0f, std::memory_order_relaxed); targetNote.store (-1, std::memory_order_relaxed);
        return;
    }
    const bool starting = ! voiced;
    unsureFrames = 0;
    // how far the detector's answer can be trusted: a clean note scores near 1, a breathy or creaky one much less
    const bool sure = heard.clarity >= 0.7f;

    // remember which notes are sung, for key finding
    const int pc = (((int) std::lround (note) % 12) + 12) % 12;
    sung[(size_t) pc].store (sung[(size_t) pc].load (std::memory_order_relaxed) + 1.0f, std::memory_order_relaxed);
    sungFrames.store (sungFrames.load (std::memory_order_relaxed) + 1.0f, std::memory_order_relaxed);

    if (graphMode)
    {
        // The target curve already holds every glide and every bit of vibrato that is being kept, so the voice
        // simply goes where it says. Outside the notes, while capturing, or with the song stopped, nothing moves.
        const float goal = (frame >= 0 && ! graph->isCapturing()) ? graph->targetAt (frame) : 0.0f;
        const float wanted = goal > 0.0f ? juce::jlimit (-12.0f, 12.0f, goal - note) : 0.0f;
        // inside a note the curve is followed exactly; stepping on to or off a note takes a couple of frames
        const bool inside = goal > 0.0f && target >= 0;
        shift = (starting || inside) ? wanted : shift + (wanted - shift) * 0.6f;
        target = goal > 0.0f ? (int) std::lround (goal) : -1;
        slowNote = note; farFrames = 0; switchFrames = 0;
    }
    else
    {
        // The slow version of the pitch is the centre of the note; the difference from it is vibrato and slides.
        // A real move to another note (well away from the current one for 40 ms) starts a new centre straight away.
        // The centre of the note is the middle value (the median) of what has been sung since the note began, over
        // at most the last 160 ms: about one cycle of vibrato. A median, because a note usually begins with a scoop
        // up to it, and an average of the scoop and the note sits flat of the note for as long as the note lasts.
        // (It used to be a slow average that started from the first moment of the note. On real singing most
        // notes were over before it had caught up, so gentle tuning left them where they were sung.)
        if (starting) { sliding = 0.0f; farFrames = 0; recentCount = 0; centreFrames = 0; }
        if (target >= 0 && std::abs (note - (float) target) > 0.8f) { if (++farFrames >= 4) { centreFrames = 0; farFrames = 0; } }
        else farFrames = 0;
        centreHist[(size_t) (centreFrames % (int) centreHist.size())] = note;
        ++centreFrames;
        {
            const int count = juce::jmin (centreFrames, (int) centreHist.size());
            auto sorted = centreHist;
            std::sort (sorted.begin(), sorted.begin() + count);
            slowNote = (count & 1) ? sorted[(size_t) (count / 2)] : 0.5f * (sorted[(size_t) (count / 2 - 1)] + sorted[(size_t) (count / 2)]);
        }
        // How fast the voice is travelling: how far it has moved in the last 180 ms. That is about one cycle of
        // vibrato, which ends where it began and so counts for little; a slide between notes, or the rise and
        // fall of a spoken line, ends somewhere else.
        const int span = juce::jmin (recentCount, (int) recent.size());
        const float was = span > 0 ? recent[(size_t) ((recentAt - span + (int) recent.size() * 2) % (int) recent.size())] : note;
        recent[(size_t) recentAt] = note; recentAt = (recentAt + 1) % (int) recent.size(); ++recentCount;
        const float speedNow = span >= 6 ? std::abs (note - was) / ((float) span * 0.01f) : 0.0f;
        sliding += (speedNow - sliding) * 0.35f;
        // what gets corrected: the note as sung (keep vibrato = 0) or only its centre (keep vibrato = 1)
        const float reference = note + settings.keepVibrato * (slowNote - note);

        // Nearest note of the scale. A voice that wavers between two notes would flicker from one to the other,
        // which is the warble that gives away a weak singer. Note hold makes the voice earn the move: it has to be
        // clearly nearer the new note, and stay there for a moment.
        int candidate = nearestAllowed (reference, settings.key, settings.scale, settings.noteMask);
        // Note hold is for a voice hovering between two neighbouring notes. A voice that has plainly gone somewhere
        // else (more than a tone and a half from the note it was on) has changed note, and waiting would pull the
        // new note back to the old one for a moment: with hard tune that was a blip of several semitones at every
        // big interval.
        const bool wentElsewhere = target >= 0 && std::abs (reference - (float) target) > 1.5f;
        if (target >= 0 && candidate != target && ! wentElsewhere && noteAllowed (target, settings.key, settings.scale, settings.noteMask))
        {
            const float margin = 0.2f + 0.4f * settings.hold;
            const bool clearlyNearer = std::abs (reference - (float) candidate) + margin <= std::abs (reference - (float) target);
            if (! clearlyNearer) { candidate = target; switchFrames = 0; }
            else if (++switchFrames <= (int) (settings.hold * 6.0f)) candidate = target;
            else switchFrames = 0;
        }
        else switchFrames = 0;
        framesOnNote = candidate == target ? framesOnNote + 1 : 0;
        if (candidate != target) centreFrames = 0;
        target = candidate;

        float off = (float) target - reference;
        // Flex: a note that is already close enough is left as the singer sang it; correction fades in beyond that
        const float flex = settings.flexCents * 0.01f;
        if (flex > 0.0f && std::abs (off) < flex * 1.5f)
        {
            const float x = juce::jlimit (0.0f, 1.0f, (std::abs (off) - flex * 0.5f) / flex);
            off *= x * x * (3.0f - 2.0f * x);
        }
        // Keep slides: a voice on its way from one note to the next is not on a wrong note, it is between notes.
        // Pulling it to the scale there is what makes gentle tuning sound like a machine, so the correction lets go
        // while the pitch is travelling (faster than about 3 semitones a second) and comes back once it settles.
        // (Keep slides and Humanise belong to gentle tuning. Under 20 ms of speed they fade out, and at zero they
        // do nothing, so turning Speed all the way down always gives the full hard-tune effect.)
        const float gentle = juce::jlimit (0.0f, 1.0f, settings.speedMs / 20.0f);
        // The travelling speed looks back 180 ms, so it still reads "moving" for a fifth of a second after the voice
        // has landed. On real singing that was most of the note, and the note stayed where it was sung. So a voice
        // that is close to its note and has all but stopped (over the last 50 ms) counts as arrived.
        const int size = (int) recent.size();
        const float lately = recentCount > 5 ? std::abs (note - recent[(size_t) ((recentAt - 6 + size * 2) % size)]) / 0.05f : 0.0f;
        const float near = 1.0f - juce::jlimit (0.0f, 1.0f, (std::abs (note - (float) target) - 0.3f) / 0.4f);
        const float still = 1.0f - juce::jlimit (0.0f, 1.0f, (lately - 2.0f) / 4.0f);
        const float moving = juce::jlimit (0.0f, 1.0f, (sliding - 3.0f) / 4.0f) * (1.0f - near * still);
        off *= 1.0f - settings.keepSlides * gentle * moving * moving * (3.0f - 2.0f * moving);
        const float wanted = juce::jlimit (-7.0f, 7.0f, off * settings.amount);
        // Humanise: once a note has been held for a fifth of a second it is corrected more slowly, so a long note
        // keeps its natural drift while short notes are still caught quickly
        const float held = juce::jlimit (0.0f, 1.0f, ((float) framesOnNote - 12.0f) / 25.0f);
        const float speed = settings.speedMs + settings.humanise * gentle * 160.0f * held;
        // The correction a note needed says nothing about the next one. For the first 80 ms of a new note, whatever
        // is left of the old correction is let go quickly (it happens while the voice is on the move, where it
        // cannot be heard as a step); the new note's own correction then comes in at the speed asked for.
        const bool lettingGo = framesOnNote < 8 && (wanted * shift < 0.0f || std::abs (wanted) < std::abs (shift));
        const float pace = lettingGo ? juce::jmin (speed, 25.0f) : speed;
        // The first 30 ms of a word are the least certain: the consonant is ending, the note is still arriving and
        // the detector has one or two answers to go on. The correction comes in over those 30 ms instead of at
        // once, so a first guess that turns out wrong is a small nudge and not a jump to the wrong note and back.
        if (starting) sinceStart = 0; else if (sinceStart < 100) ++sinceStart;
        const float easeIn = juce::jmin (1.0f, (float) (sinceStart + 1) / 3.0f);
        if (starting || speed < 1.0f) shift = wanted * easeIn;
        else if (sure) shift += (wanted * easeIn - shift) * (1.0f - std::exp (-10.0f / pace));
        // an answer that cannot be trusted changes nothing: the correction in force carries on
    }

    voiced = true;
    // A new word, or a leap to another note, starts at its own pitch: gliding there from the old one would cut the
    // first slices of the new note at sizes that belong to neither.
    if (starting || std::abs (note - appliedNote) > 3.0f) snap = true;
    appliedNote = note;
    ratio = std::pow (2.0f, shift / 12.0f);
    period = (float) (fs / (double) f);
    detectedNote.store (note, std::memory_order_relaxed);
    outputNote.store (note + shift, std::memory_order_relaxed);
    targetNote.store (target, std::memory_order_relaxed);
}

void Tuner::process (juce::AudioBuffer<float>& buffer, bool passThrough) noexcept
{
    if (! settings.enabled)
    {
        if (wasEnabled) { wasEnabled = false; detectedNote.store (0.0f); outputNote.store (0.0f); targetNote.store (-1); }
        return;
    }
    if (! wasEnabled || wasLow != settings.lowLatency || wasLook != lookFrames()) { wasLow = settings.lowLatency; wasLook = lookFrames(); reset(); wasEnabled = true; }

    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (channels, buffer.getNumChannels());
    const float d = (float) delayNow();
    const int look = lookFrames();
    float* data[2] = { buffer.getWritePointer (0), ch > 1 ? buffer.getWritePointer (1) : nullptr };
    // The detector answers every 10 ms. Handing its answers to the engine as they come would move the pitch in
    // 10 ms stair-steps, so the period of the voice and the period being aimed for both glide over a few
    // milliseconds. The aimed-for period is smoothed on its own, so on a hard-tuned note it does not move at all,
    // however much the detector's reading of the voice wobbles.
    const float glide = 1.0f - std::exp (-1.0f / (float) (fs * 0.003));
    for (int i = 0; i < n; ++i)
    {
        float mono = 0.0f;
        for (int c = 0; c < ch; ++c) { engine.write (c, data[c][i]); mono += data[c][i]; }
        if (detector.push (mono / (float) ch))
        {
            auto noteOf = [] (float hz) { return hz > 0.0f ? 69.0f + 12.0f * std::log2 (hz / 440.0f) : 0.0f; };
            Heard h;
            h.seconds = blockTime + (double) i / fs;
            if (look == 0)
            {
                // straight from the detector, which has applied its own rule about leaps
                const bool pitched = detector.frequency() > 0.0f && detector.clarity() >= 0.5f;
                h.hz = pitched ? detector.frequency() : 0.0f; h.note = noteOf (h.hz); h.clarity = detector.clarity();
                control (h);
            }
            else
            {
                h.hz = detector.rawFrequency(); h.note = noteOf (h.hz); h.clarity = detector.rawClarity();
                if (aheadCount < look) ahead[(size_t) aheadCount++] = h;
                else
                {
                    const Heard oldest = ahead[0];
                    for (int k = 1; k < aheadCount; ++k) ahead[(size_t) k - 1] = ahead[(size_t) k];
                    ahead[(size_t) aheadCount - 1] = h;
                    control (settle (oldest));
                }
            }
        }
        const float outGoal = period / ratio;
        if (snap) { periodNow = period; outPeriodNow = outGoal; snap = false; }
        else { periodNow += (period - periodNow) * glide; outPeriodNow += (outGoal - outPeriodNow) * glide; }
        float out[2] = { 0.0f, 0.0f };
        if (passThrough) engine.renderSync (voice, (float) (fs / 200.0), false, 1.0f, d, out);
        else if (settings.legacySlicing) engine.render (voice, period, voiced, ratio, d, out, formant);
        else             engine.renderSync (voice, periodNow, voiced, periodNow / outPeriodNow, d, out, formant);
        for (int c = 0; c < ch; ++c) data[c][i] = out[c];
        engine.advance();
    }
}

Tuner::KeyGuess Tuner::guessKey() const noexcept
{
    float h[12];
    for (int i = 0; i < 12; ++i) h[i] = sung[(size_t) i].load (std::memory_order_relaxed);
    auto guess = keyFromNotes (h);
    guess.secondsHeard = sungFrames.load (std::memory_order_relaxed) * 0.01f;
    return guess;
}

void Tuner::forgetNotes() noexcept
{
    for (auto& s : sung) s.store (0.0f);
    sungFrames.store (0.0f);
}
}
