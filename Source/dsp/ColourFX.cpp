#include "ColourFX.h"

namespace mu
{
namespace
{
const float kTwoPi = juce::MathConstants<float>::twoPi;
}

//==================================================================== filter
void ColourBiquad::highPass (float hz, float q, double fs) noexcept
{
    const float w = kTwoPi * hz / (float) fs, c = std::cos (w), alpha = std::sin (w) / (2.0f * q), a0 = 1.0f + alpha;
    b0 = (1.0f + c) * 0.5f / a0; b1 = -(1.0f + c) / a0; b2 = b0; a1 = -2.0f * c / a0; a2 = (1.0f - alpha) / a0;
}

void ColourBiquad::lowPass (float hz, float q, double fs) noexcept
{
    const float w = kTwoPi * juce::jmin (hz, (float) fs * 0.45f) / (float) fs, c = std::cos (w), alpha = std::sin (w) / (2.0f * q), a0 = 1.0f + alpha;
    b0 = (1.0f - c) * 0.5f / a0; b1 = (1.0f - c) / a0; b2 = b0; a1 = -2.0f * c / a0; a2 = (1.0f - alpha) / a0;
}

void ColourBiquad::peak (float hz, float q, float gainDb, double fs) noexcept
{
    const float A = std::pow (10.0f, gainDb / 40.0f);
    const float w = kTwoPi * hz / (float) fs, c = std::cos (w), alpha = std::sin (w) / (2.0f * q), a0 = 1.0f + alpha / A;
    b0 = (1.0f + alpha * A) / a0; b1 = -2.0f * c / a0; b2 = (1.0f - alpha * A) / a0; a1 = -2.0f * c / a0; a2 = (1.0f - alpha / A) / a0;
}

float ColourBiquad::magnitudeDb (float hz, double fs) const noexcept
{
    const double w = juce::MathConstants<double>::twoPi * (double) hz / fs;
    const double c1 = std::cos (w), s1 = std::sin (w), c2 = std::cos (2.0 * w), s2 = std::sin (2.0 * w);
    const double nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2);
    const double dr = 1.0 + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
    return (float) (10.0 * std::log10 (juce::jmax (1.0e-20, (nr * nr + ni * ni) / juce::jmax (1.0e-20, dr * dr + di * di))));
}

//==================================================================== Phone
namespace
{
struct PhoneVoicing { float lo0, lo1, hi0, hi1, noseHz, noseQ, noseDb, hornHz, hornQ, hornDb, hiss, crackle, squelch; };
// lo and hi are the band edges at Tone 0 and Tone 1. A noseHz of 0 means "a little above the middle of the band".
const PhoneVoicing kPhone[5] = {
    { 600.0f, 250.0f, 2200.0f, 5000.0f,    0.0f, 1.1f, 5.0f,    0.0f, 1.0f, 0.0f, 0.5f, 0.0f,  0.0f },      // small speaker
    { 400.0f, 280.0f, 2800.0f, 3700.0f, 1800.0f, 1.0f, 4.0f,    0.0f, 1.0f, 0.0f, 1.0f, 0.15f, 0.0f },      // telephone
    { 600.0f, 400.0f, 3500.0f, 5500.0f, 1100.0f, 2.2f, 8.0f, 2800.0f, 2.5f, 6.0f, 0.6f, 0.0f,  0.0f },      // megaphone
    { 250.0f, 130.0f, 3200.0f, 5500.0f, 1200.0f, 0.8f, 3.0f,    0.0f, 1.0f, 0.0f, 1.0f, 1.0f,  0.0f },      // radio
    { 650.0f, 450.0f, 2400.0f, 3200.0f, 1500.0f, 1.4f, 6.0f,    0.0f, 1.0f, 0.0f, 0.8f, 0.0f,  1.0f } };    // walkie-talkie
const PhoneVoicing& voicing (int mode) noexcept { return kPhone[juce::jlimit (0, 4, mode)]; }
}

float Phone::lowEdgeHz (float tone, int mode) noexcept  { const auto& v = voicing (mode); return v.lo0 + (v.lo1 - v.lo0) * juce::jlimit (0.0f, 1.0f, tone); }
float Phone::highEdgeHz (float tone, int mode) noexcept { const auto& v = voicing (mode); return v.hi0 + (v.hi1 - v.hi0) * juce::jlimit (0.0f, 1.0f, tone); }

void Phone::prepare (double sampleRate, int numChannels)
{
    fs = sampleRate; channels = juce::jlimit (1, 2, numChannels);
    shownTone = -1.0f; shownMode = -1;
    update();
    reset();
}

void Phone::reset() noexcept
{
    for (auto* f : { &hp1, &hp2, &lp1, &lp2, &nose, &horn }) f->clear();
    mixNow = 0.0f; activity = 0.0f; quick = 0.0f; agc = 0.1f; noiseHp = 0.0f; crackle = 0.0f; burst = 0.0f; noiseNow = 0.0f; wasActive = false;
}

void Phone::update() noexcept
{
    if (settings.tone == shownTone && settings.mode == shownMode) return;
    shownTone = settings.tone; shownMode = settings.mode;
    const auto& v = voicing (settings.mode);
    const float lo = lowEdgeHz (settings.tone, settings.mode), hi = highEdgeHz (settings.tone, settings.mode);
    hp1.highPass (lo, 0.5412f, fs); hp2.highPass (lo, 1.3066f, fs);
    lp1.lowPass (hi, 0.5412f, fs);  lp2.lowPass (hi, 1.3066f, fs);
    nose.peak (v.noseHz > 0.0f ? v.noseHz : std::sqrt (lo * hi) * 1.25f, v.noseQ, v.noseDb, fs);
    horn.peak (v.hornHz > 0.0f ? v.hornHz : 1000.0f, v.hornQ, v.hornDb, fs);
}

float Phone::responseDb (float hz) const noexcept
{
    return hp1.magnitudeDb (hz, fs) + hp2.magnitudeDb (hz, fs) + lp1.magnitudeDb (hz, fs) + lp2.magnitudeDb (hz, fs) + nose.magnitudeDb (hz, fs)
         + (voicing (settings.mode).hornHz > 0.0f ? horn.magnitudeDb (hz, fs) : 0.0f);
}

void Phone::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! settings.enabled) { if (mixNow > 0.0f) reset(); return; }
    update();
    const int n = buffer.getNumSamples(), ch = juce::jmin (channels, buffer.getNumChannels());
    const int mode = juce::jlimit (0, 4, settings.mode);
    const auto& v = voicing (mode);
    const bool hasHorn = v.hornHz > 0.0f;
    const float pre = 1.0f + 9.0f * juce::jlimit (0.0f, 1.0f, settings.drive);
    const float post = 1.6f / std::sqrt (pre);                  // keeps the loudness roughly where it was as the drive goes up
    const float step = 1.0f - std::exp (-1.0f / (float) (fs * 0.02));
    // noise: only while the track is sounding, so a vocal with gaps in it does not hiss through them
    const float noiseAmount = juce::jlimit (0.0f, 1.0f, settings.noise);
    const float noiseLevel = noiseAmount * noiseAmount * 0.035f;
    const bool noisy = noiseLevel > 0.0f || noiseNow > 1.0e-6f;
    const float actUp = 1.0f - std::exp (-1.0f / (float) (fs * 0.01)), actDown = 1.0f - std::exp (-1.0f / (float) (fs * 0.4));
    const float crackleDecay = std::exp (-1.0f / (float) (fs * 0.0015)), burstDecay = std::exp (-1.0f / (float) (fs * 0.045));
    const float quickDown = 1.0f - std::exp (-1.0f / (float) (fs * 0.06));
    const float lopsided = std::tanh (0.2f);                    // taken off again, so silence in is silence out
    const float crackleChance = v.crackle * (2.0f + 10.0f * noiseAmount) / (float) fs;
    const float noiseHpCoeff = std::exp (-kTwoPi * lowEdgeHz (settings.tone, mode) / (float) fs);
    const float agcUp = 1.0f - std::exp (-1.0f / (float) (fs * 0.003)), agcDown = 1.0f - std::exp (-1.0f / (float) (fs * 0.25));
    const float fadeStep = kTwoPi * 0.23f / (float) fs;
    for (int i = 0; i < n; ++i)
    {
        mixNow += (settings.amount - mixNow) * step;
        float in = 0.0f;
        for (int c = 0; c < ch; ++c) in = juce::jmax (in, std::abs (buffer.getReadPointer (c)[i]));
        const float want = in > 0.003f ? 1.0f : 0.0f;
        activity += (want - activity) * (want > activity ? actUp : actDown);
        float noise = 0.0f, reception = 1.0f;
        if (noisy)
        {
            noiseNow += (noiseLevel - noiseNow) * step;
            float raw = v.hiss * nextNoise();
            if (v.crackle > 0.0f)
            {
                if (std::abs (nextNoise()) < crackleChance) crackle = nextNoise() > 0.0f ? 1.0f : -1.0f;
                raw += 7.0f * crackle; crackle *= crackleDecay;
            }
            raw *= activity;
            if (v.squelch > 0.0f)
            {
                // a short burst of open-channel noise as the talker lets go of the button, and a click as they press it
                // listened for on a quicker clock than the hiss, so the burst comes as the talker stops and not a beat later
                quick += (want - quick) * (want > quick ? actUp : quickDown);
                const bool active = quick > 0.5f;
                if (wasActive && ! active) burst = 1.0f;
                else if (! wasActive && active) burst = juce::jmax (burst, 0.45f);
                wasActive = active;
                raw += 6.0f * burst * nextNoise(); burst *= burstDecay;
            }
            noiseHp = noiseHpCoeff * noiseHp + (1.0f - noiseHpCoeff) * raw;
            noise = (raw - noiseHp) * noiseNow;
            if (mode == 3)
            {
                // reception that comes and goes, more of it the noisier the signal
                fade += fadeStep; if (fade > kTwoPi) fade -= kTwoPi;
                reception = 1.0f - 0.3f * noiseAmount * (0.5f + 0.5f * std::sin (fade + 0.7f * std::sin (fade * 0.31f)));
            }
        }
        float level = 1.0f;
        if (mode == 3)
        {
            // a radio's automatic gain: quiet words are pulled up, loud ones held, and it breathes as it recovers
            agc += (in - agc) * (in > agc ? agcUp : agcDown);
            level = juce::jlimit (0.5f, 3.5f, 0.12f / juce::jmax (0.01f, agc));
        }
        for (int c = 0; c < ch; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            float x = hp2.run (c, hp1.run (c, d[i]));
            x = nose.run (c, x);
            if (hasHorn) x = horn.run (c, x);
            const float pushed = x * pre * level;
            switch (mode)
            {
                case 1:  x = (std::tanh (pushed + 0.2f) - lopsided) * post; break;                          // a carbon microphone distorts more one way than the other
                case 2:  { const float y = juce::jlimit (-1.0f, 1.0f, pushed); x = (1.2f * y - 0.2f * y * y * y) * post; break; }      // a horn driver runs out of travel
                case 4:  { const float y = juce::jlimit (-1.0f, 1.0f, pushed * 2.0f); x = (1.2f * y - 0.2f * y * y * y) * post * 0.8f; break; }
                default: x = std::tanh (pushed) * post; break;
            }
            x = lp2.run (c, lp1.run (c, x + noise)) * reception;
            d[i] = d[i] * (1.0f - mixNow) + x * mixNow;
        }
    }
}

//==================================================================== Lo-fi
void LoFi::prepare (double sampleRate, int numChannels)
{
    fs = sampleRate; channels = juce::jlimit (1, 2, numChannels);
    for (auto& l : line) l.assign ((size_t) (fs * 0.05) + 8, 0.0f);
    crackleBand.highPass (1200.0f, 0.7f, fs);
    shownTone = -1.0f;
    reset();
}

void LoFi::reset() noexcept
{
    for (auto& l : line) std::fill (l.begin(), l.end(), 0.0f);
    pos = 0; mixNow = 0.0f;
    for (int c = 0; c < 2; ++c) { phase[c] = 1.0f; held[c] = 0.0f; lp[c] = 0.0f; hissLp[c] = 0.0f; }
    toneFilter.clear(); crackleBand.clear();
    activity = 0.0f; noiseNow = 0.0f; rumble = 0.0f; crackle = 0.0f; sag = 0.0f; dropLeft = 0; dropDepth = 0.0f; wearGain = 1.0f;
}

void LoFi::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! settings.enabled) { if (mixNow > 0.0f) reset(); return; }
    const int n = buffer.getNumSamples(), ch = juce::jmin (channels, buffer.getNumChannels());
    const int size = (int) line[0].size();
    const float crush = juce::jlimit (0.0f, 1.0f, settings.crush);
    const float step = rateHz (crush, fs) / (float) fs;                       // 1 = every sample is kept
    const float levels = std::pow (2.0f, bits (crush) - 1.0f);
    const float lpCoeff = std::exp (-kTwoPi * (14000.0f * std::pow (0.25f, crush)) / (float) fs);
    const float base = (float) (fs * 0.012), swing = (float) (fs * 0.005) * juce::jlimit (0.0f, 1.0f, settings.wobble);
    const float wowStep = kTwoPi * 0.55f / (float) fs, flutterStep = kTwoPi * 6.3f / (float) fs;
    const float mixStep = 1.0f - std::exp (-1.0f / (float) (fs * 0.02));
    // the modules that are off by default
    const float drive = juce::jlimit (0.0f, 1.0f, settings.drive);
    const bool driven = drive > 0.001f;
    const float pre = 1.0f + 7.0f * drive * drive, makeup = 1.0f / std::pow (pre, 0.6f);
    const float wear = juce::jlimit (0.0f, 1.0f, settings.wear);
    const bool worn = wear > 0.001f;
    const float sagStep = kTwoPi * 0.7f / (float) fs, dropChance = wear * wear * 0.9f / (float) fs;
    const bool dulled = settings.tone < 0.999f;
    if (dulled && settings.tone != shownTone) { shownTone = settings.tone; toneFilter.lowPass (toneHz (settings.tone), 0.7071f, fs); }
    const float noiseAmount = juce::jlimit (0.0f, 1.0f, settings.noise);
    const float noiseLevel = noiseAmount * noiseAmount * 0.03f;
    const bool noisy = noiseLevel > 0.0f || noiseNow > 1.0e-6f;
    const bool vinyl = settings.noiseType == 0;
    const float actUp = 1.0f - std::exp (-1.0f / (float) (fs * 0.01)), actDown = 1.0f - std::exp (-1.0f / (float) (fs * 1.5));
    const float crackleDecay = std::exp (-1.0f / (float) (fs * 0.0004)), rumbleCoeff = std::exp (-kTwoPi * 45.0f / (float) fs);
    const float crackleChance = (6.0f + 30.0f * noiseAmount) / (float) fs, hissCoeff = std::exp (-kTwoPi * 9000.0f / (float) fs);
    for (int i = 0; i < n; ++i)
    {
        wow += wowStep; if (wow > kTwoPi) wow -= kTwoPi;
        flutter += flutterStep; if (flutter > kTwoPi) flutter -= kTwoPi;
        const float back = base + swing * (std::sin (wow) + 0.025f * std::sin (flutter));      // a slow drift with a faint fast shiver on top
        const int whole = (int) back; const float frac = back - (float) whole;
        mixNow += (settings.mix - mixNow) * mixStep;
        if (worn)
        {
            // the level sags slowly, and now and then drops away for a tenth of a second where the tape has worn thin
            sag += sagStep; if (sag > kTwoPi) sag -= kTwoPi;
            const float sagDb = -1.5f * wear * (0.5f + 0.5f * std::sin (sag + 0.6f * std::sin (sag * 0.37f)));
            if (dropLeft <= 0 && std::abs (nextNoise()) < dropChance)
            {
                dropLength = juce::jmax (1, (int) (fs * (0.04 + 0.12 * (double) std::abs (nextNoise()))));
                dropLeft = dropLength; dropDepth = wear * (0.35f + 0.45f * std::abs (nextNoise()));
            }
            float dip = 0.0f;
            if (dropLeft > 0) { const float s = std::sin (juce::MathConstants<float>::pi * (1.0f - (float) dropLeft / (float) dropLength)); dip = dropDepth * s * s; --dropLeft; }
            wearGain = std::pow (10.0f, sagDb / 20.0f) * (1.0f - dip);
        }
        float common = 0.0f;
        if (noisy)
        {
            float in = 0.0f;
            for (int c = 0; c < ch; ++c) in = juce::jmax (in, std::abs (buffer.getReadPointer (c)[i]));
            const float want = in > 0.003f ? 1.0f : 0.0f;
            activity += (want - activity) * (want > activity ? actUp : actDown);
            // the last of the noise is let go completely, so a long silence is silent
            noiseNow += (noiseLevel * juce::jmax (0.0f, (activity - 0.05f) / 0.95f) - noiseNow) * mixStep;
            if (vinyl)
            {
                // dust: sharp ticks; and the low rumble of the turntable under them
                if (std::abs (nextNoise()) < crackleChance) crackle = (0.3f + 0.7f * std::abs (nextNoise())) * (nextNoise() > 0.0f ? 1.0f : -1.0f);
                const float tick = crackleBand.run (0, crackle); crackle *= crackleDecay;
                rumble = rumbleCoeff * rumble + (1.0f - rumbleCoeff) * nextNoise();
                common = 9.0f * tick + 14.0f * rumble;
            }
        }
        for (int c = 0; c < ch; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            const float in = driven ? std::tanh (d[i] * pre) * makeup : d[i];
            line[(size_t) c][(size_t) pos] = in;
            int a = pos - whole; if (a < 0) a += size;
            int b = a - 1; if (b < 0) b += size;
            const float x = swing > 0.0f ? line[(size_t) c][(size_t) a] * (1.0f - frac) + line[(size_t) c][(size_t) b] * frac : in;
            // keep one sample in every few and hold it, then round it to a coarse level
            phase[c] += step;
            if (phase[c] >= 1.0f) { phase[c] -= 1.0f; held[c] = std::round (x * levels) / levels; }
            lp[c] = lpCoeff * lp[c] + (1.0f - lpCoeff) * held[c];
            float y = lp[c];
            if (worn) y *= wearGain;
            if (dulled) y = toneFilter.run (c, y);
            if (noisy)
            {
                // hiss is different on each side, the way tape noise is; ticks and rumble sit in the middle
                const float white = nextNoise();
                hissLp[c] = hissCoeff * hissLp[c] + (1.0f - hissCoeff) * white;
                y += noiseNow * (common + (vinyl ? 0.25f : 1.0f) * hissLp[c]);
            }
            d[i] = d[i] * (1.0f - mixNow) + y * mixNow;
        }
        pos = pos + 1 == size ? 0 : pos + 1;
    }
}

//==================================================================== Motion
void Motion::prepare (double sampleRate, int numChannels)
{
    fs = sampleRate; channels = juce::jlimit (1, 2, numChannels);
    for (auto& l : line) l.assign ((size_t) (fs * 0.06) + 8, 0.0f);
    reset();
}

void Motion::reset() noexcept
{
    for (auto& l : line) std::fill (l.begin(), l.end(), 0.0f);
    pos = 0; feedback[0] = feedback[1] = 0.0f; mixNow = 0.0f;
    for (auto& channel : ap) for (auto& s : channel) s = 0.0f;
    for (int c = 0; c < 2; ++c) { svLow[c] = svBand[c] = 0.0f; glide[c] = glideSlow[c] = 0.5f; }
}

float Motion::valueAt (const MotionSettings& s, float phase01) noexcept
{
    const float p = phase01 - std::floor (phase01);
    switch (s.shape)
    {
        case 1:  return p < 0.5f ? 2.0f * p : 2.0f - 2.0f * p;
        case 2:  return p;
        case 3:  return 1.0f - p;
        case 4:  return p < 0.5f ? 1.0f : 0.0f;
        case 5:
        {
            // sixteen points round the cycle: flat steps, a line through them, or anything in between
            const float x = p * (float) MotionSettings::kPoints;
            const int k = juce::jlimit (0, MotionSettings::kPoints - 1, (int) x);
            const float f = x - (float) k;
            const float a = juce::jlimit (0.0f, 1.0f, s.curve[(size_t) k]), b = juce::jlimit (0.0f, 1.0f, s.curve[(size_t) ((k + 1) % MotionSettings::kPoints)]);
            const float through = a + (b - a) * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * f));
            const float smooth = juce::jlimit (0.0f, 1.0f, s.smooth);
            return a * (1.0f - smooth) + through * smooth;
        }
        default: return 0.5f + 0.5f * std::sin (kTwoPi * p);
    }
}

void Motion::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! settings.enabled) { if (mixNow > 0.0f) reset(); return; }
    if (settings.type != lastType) { lastType = settings.type; const float keep = mixNow; reset(); mixNow = keep; }
    const int n = buffer.getNumSamples(), ch = juce::jmin (channels, buffer.getNumChannels());
    const int size = (int) line[0].size();
    const int type = juce::jlimit (0, 5, settings.type);
    const bool sine = settings.shape == 0;
    const double beats = (double) noteBeats (settings.note);
    const double perSample = settings.sync ? juce::jlimit (40.0, 300.0, settings.bpm) / 60.0 / beats / fs
                                           : (double) juce::jlimit (0.02f, 12.0f, settings.rateHz) / fs;
    if (settings.sync && playing)
    {
        // locked to the song: the cycle is always at the same place on the same beat
        phase = ppq / beats; phase -= std::floor (phase);
        slowPhase = ppq / (beats * 2.0); slowPhase -= std::floor (slowPhase);
    }
    const float depth = juce::jlimit (0.0f, 1.0f, settings.depth);
    const float mixStep = 1.0f - std::exp (-1.0f / (float) (fs * 0.02));
    // a drawn curve or a square can jump; what it moves must not. Delay times need longer to move than gains do.
    const float glideStep = 1.0f - std::exp (-1.0f / (float) (fs * (type <= 1 ? 0.012 : 0.003)));
    auto readLine = [&] (int c, float back)
    {
        const int whole = (int) back; const float frac = back - (float) whole;
        int a = pos - whole; if (a < 0) a += size;
        int b = a - 1; if (b < 0) b += size;
        return line[(size_t) c][(size_t) a] * (1.0f - frac) + line[(size_t) c][(size_t) b] * frac;
    };
    for (int i = 0; i < n; ++i)
    {
        phase += perSample; if (phase >= 1.0) phase -= 1.0;
        slowPhase += perSample * 0.5; if (slowPhase >= 1.0) slowPhase -= 1.0;
        mixNow += (settings.mix - mixNow) * mixStep;
        float panned[2] = { 1.0f, 1.0f };
        for (int c = 0; c < ch; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            const float dry = d[i];
            // the right side runs a quarter of a cycle behind for width, except where both sides have to move as one
            const float offset = (c == 1 && type <= 3) ? 0.25f : 0.0f;
            float sweep = valueAt (settings, (float) phase + offset);
            if (! sine) { glide[c] += (sweep - glide[c]) * glideStep; sweep = glide[c]; }
            const float wave = 2.0f * sweep - 1.0f;
            float wet = 0.0f;
            if (type == 0)
            {
                // the second copy moves at half the speed, on a clock of its own so it never jumps
                float second = sine ? std::sin (kTwoPi * (float) slowPhase + offset * juce::MathConstants<float>::pi + 2.1f)
                                    : 2.0f * valueAt (settings, (float) slowPhase + 0.33f + offset * 0.5f) - 1.0f;
                if (! sine) { glideSlow[c] += (second - glideSlow[c]) * glideStep; second = glideSlow[c]; }
                line[(size_t) c][(size_t) pos] = dry;
                const float a = (float) fs * 0.001f * (16.0f + 6.0f * depth * wave);
                const float b = (float) fs * 0.001f * (24.0f + 5.0f * depth * second);
                wet = 0.6f * (readLine (c, a) + readLine (c, b));
                d[i] = dry + mixNow * (wet - dry * 0.25f);
            }
            else if (type == 1)
            {
                line[(size_t) c][(size_t) pos] = dry + 0.55f * feedback[c];
                wet = readLine (c, (float) fs * 0.001f * (0.35f + (0.3f + 5.0f * depth) * sweep));
                feedback[c] = wet;
                d[i] = dry + mixNow * wet;
            }
            else if (type == 2)
            {
                // six stages that each turn the wave round at one frequency; mixed back with the dry sound they cut notches
                const float hz = 300.0f * std::pow (10.0f, sweep * (0.3f + 0.7f * depth));
                const float t = std::tan (juce::MathConstants<float>::pi * juce::jmin (hz, (float) fs * 0.4f) / (float) fs);
                const float k = (t - 1.0f) / (t + 1.0f);
                float x = dry + 0.5f * feedback[c];
                for (int s = 0; s < 6; ++s)
                {
                    const float y = k * x + ap[c][s];
                    ap[c][s] = x - k * y;
                    x = y;
                }
                feedback[c] = x;
                d[i] = dry + mixNow * x;
            }
            else if (type == 3)
            {
                // a resonant low-pass: fully open at the top of the movement, closing as far as Depth says at the bottom
                const float hz = 18000.0f * std::pow (220.0f / 18000.0f, depth * (1.0f - sweep));
                const float g = std::tan (juce::MathConstants<float>::pi * juce::jmin (hz, (float) fs * 0.45f) / (float) fs);
                const float k = 1.0f / 1.4f;
                const float high = (dry - (k + g) * svBand[c] - svLow[c]) / (1.0f + k * g + g * g);
                const float band = g * high + svBand[c]; svBand[c] = g * high + band;
                const float low = g * band + svLow[c];  svLow[c] = g * band + low;
                d[i] = dry + mixNow * (low - dry);
            }
            else if (type == 4)
            {
                // equal power, so the sound keeps its loudness as it crosses the middle
                const float angle = (wave * depth + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                panned[0] = std::cos (angle) * 1.41421f; panned[1] = std::sin (angle) * 1.41421f;
                if (ch > 1) d[i] = dry * (1.0f + mixNow * (panned[c] - 1.0f));
            }
            else
            {
                d[i] = dry * (1.0f + mixNow * (-depth * (1.0f - sweep)));
            }
        }
        pos = pos + 1 == size ? 0 : pos + 1;
    }
    shownPhase.store ((float) phase, std::memory_order_relaxed);
}

//==================================================================== Chop
void Chop::prepare (double sampleRate)
{
    fs = sampleRate;
    smooth = 1.0f - std::exp (-1.0f / (float) (fs * 0.0025));       // edges take a few milliseconds, so the gate does not click
    reset();
}

float Chop::gateAt (float p, float shape) noexcept
{
    // Open around the middle of the first half of the step, shut around the middle of the second. Hardness sets
    // how quickly it moves between the two: at zero the whole step is one smooth rise and fall, at full it switches.
    float x = std::abs (p - std::floor (p) - 0.25f);
    if (x > 0.5f) x = 1.0f - x;
    const float w = juce::jmax (0.004f, 0.5f * (1.0f - juce::jlimit (0.0f, 1.0f, shape)));
    const float t = juce::jlimit (0.0f, 1.0f, (0.25f + w * 0.5f - x) / w);
    return 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * t);
}

float Chop::patternAt (const ChopSettings& s, double position) noexcept
{
    const int length = juce::jlimit (1, ChopSettings::kSteps, s.length);
    double p = std::fmod (position, (double) length); if (p < 0.0) p += (double) length;
    int k = juce::jlimit (0, length - 1, (int) p);
    float f = (float) (p - (double) k);
    // swing: every second step starts late, and the step before it runs on until it does
    const float late = 0.5f * juce::jlimit (0.0f, 1.0f, s.swing);
    if (late > 0.0f)
    {
        if ((k & 1) == 1)
        {
            if (f < late) { k -= 1; f = 1.0f + f; }                 // still the tail of the step before
            else f = (f - late) / (1.0f - late);
        }
        else f = f / (1.0f + late);
    }
    const float gate = juce::jlimit (0.05f, 1.0f, s.gate);
    const bool open = gate >= 0.999f || f < gate;
    return open ? juce::jlimit (0.0f, 1.0f, s.steps[(size_t) k]) : 0.0f;
}

void Chop::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! settings.enabled) { gain = 1.0f; return; }
    const int n = buffer.getNumSamples(), ch = buffer.getNumChannels();
    const double beats = (double) rateBeats (settings.rate);
    const double perSample = juce::jlimit (40.0, 300.0, settings.bpm) / 60.0 / fs / beats;
    const float depth = juce::jlimit (0.0f, 1.0f, settings.depth);
    if (settings.mode == 0)
    {
        if (playing) { phase = ppq / beats; phase -= std::floor (phase); }
        else if (phase >= 1.0) phase -= std::floor (phase);
        for (int i = 0; i < n; ++i)
        {
            const float wanted = 1.0f - depth * (1.0f - gateAt ((float) phase, settings.shape));
            gain += (wanted - gain) * smooth;
            for (int c = 0; c < ch; ++c) buffer.getWritePointer (c)[i] *= gain;
            phase += perSample; if (phase >= 1.0) phase -= 1.0;
        }
        shownPhase.store ((float) phase, std::memory_order_relaxed);
        return;
    }
    // Pattern: the position is counted in steps from the start of the pattern. With the song playing it comes from
    // the song itself, so step 1 always falls where the pattern begins.
    const double length = (double) juce::jlimit (1, ChopSettings::kSteps, settings.length);
    if (playing) phase = std::fmod (ppq / beats, length);
    if (phase < 0.0) phase += length;
    const float hard = juce::jlimit (0.0f, 1.0f, settings.shape);
    const float edge = 1.0f - std::exp (-1.0f / (float) (fs * (0.0008 + 0.012 * (double) ((1.0f - hard) * (1.0f - hard)))));
    for (int i = 0; i < n; ++i)
    {
        const float wanted = 1.0f - depth * (1.0f - patternAt (settings, phase));
        gain += (wanted - gain) * edge;
        for (int c = 0; c < ch; ++c) buffer.getWritePointer (c)[i] *= gain;
        phase += perSample; if (phase >= length) phase -= length;
    }
    shownPhase.store ((float) (phase / length), std::memory_order_relaxed);
}
}
