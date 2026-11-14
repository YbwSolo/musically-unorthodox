#include "Tuner.h"

namespace mu
{
void Tuner::prepare (double sampleRate, int, int numChannels)
{
    fs = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    delay = (int) std::round (fs * 0.020);          // lines the voice up with what the detector knows about it
    delayLow = (int) std::round (fs * 0.006);       // for recording: the least that still sounds right
    detector.prepare (fs);
    engine.prepare (channels, fs);
    reset();
}

void Tuner::reset()
{
    detector.reset();
    engine.reset();
    engine.resetVoice (voice, (float) (settings.lowLatency ? delayLow : delay));
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
void Tuner::control (double seconds) noexcept
{
    const float f = detector.frequency();
    const bool pitched = f > 0.0f && detector.clarity() >= 0.5f;
    const float note = pitched ? 69.0f + 12.0f * std::log2 (f / 440.0f) : 0.0f;

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
        if (voiced && ++unsureFrames <= 2) return;
        voiced = false; ratio = 1.0f; shift = 0.0f; target = -1; switchFrames = 0; unsureFrames = 0; framesOnNote = 0; snap = true;
        detectedNote.store (0.0f, std::memory_order_relaxed); outputNote.store (0.0f, std::memory_order_relaxed); targetNote.store (-1, std::memory_order_relaxed);
        return;
    }
    const bool starting = ! voiced;
    unsureFrames = 0;
    // how far the detector's answer can be trusted: a clean note scores near 1, a breathy or creaky one much less
    const bool sure = detector.clarity() >= 0.7f;

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
        if (target >= 0 && candidate != target && noteAllowed (target, settings.key, settings.scale, settings.noteMask))
        {
            const float margin = 0.12f + 0.33f * settings.hold;
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
        if (starting || speed < 1.0f) shift = wanted;
        else if (sure) shift += (wanted - shift) * (1.0f - std::exp (-10.0f / pace));
        // an answer that cannot be trusted changes nothing: the correction in force carries on
    }

    voiced = true;
    if (starting) snap = true;
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
    if (! wasEnabled || wasLow != settings.lowLatency) { reset(); wasEnabled = true; wasLow = settings.lowLatency; }

    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (channels, buffer.getNumChannels());
    const float d = (float) (settings.lowLatency ? delayLow : delay);
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
        if (detector.push (mono / (float) ch)) control (blockTime + (double) i / fs);
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
