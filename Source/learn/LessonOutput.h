#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include "LessonSound.h"

namespace mu
{
// The computer's own sound output, opened only for the Learn section's examples and only while the DAW is not
// running the plugin. A DAW stops calling an effect when its transport is stopped, so without this a student
// would have to load a song and press play just to hear a metronome click. Output only: it never opens an
// input, so it never asks for the microphone.
class LessonOutput : private juce::AudioIODeviceCallback
{
public:
    explicit LessonOutput (LessonSound& s) : sound (s) {}
    ~LessonOutput() override { close(); }

    bool open()                                           // message thread; false when no output could be opened
    {
        if (opened) return true;
        if (failed) return false;                         // do not try again on every tick
        const auto error = manager.initialise (0, 2, nullptr, true);
        if (error.isNotEmpty() || manager.getCurrentAudioDevice() == nullptr) { manager.closeAudioDevice(); failed = true; return false; }
        manager.addAudioCallback (this);
        opened = true;
        return true;
    }
    void close()
    {
        if (! opened) return;
        manager.removeAudioCallback (this);
        manager.closeAudioDevice();
        opened = false;
    }
    bool isOpen() const { return opened; }
    bool hasFailed() const { return failed; }
    void retry() { failed = false; }
    double sampleRate() const { return rate; }

private:
    void audioDeviceIOCallbackWithContext (const float* const*, int, float* const* out, int numOut, int numSamples, const juce::AudioIODeviceCallbackContext&) override
    {
        juce::AudioBuffer<float> buffer (out, numOut, numSamples);
        buffer.clear();
        sound.processDirect (buffer, LessonSound::Route::Direct);
    }
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override
    {
        rate = device != nullptr ? device->getCurrentSampleRate() : 48000.0;
        if (sound.getRoute() == LessonSound::Route::Direct) sound.setRoute (LessonSound::Route::Direct, rate);
    }
    void audioDeviceStopped() override {}

    LessonSound& sound;
    juce::AudioDeviceManager manager;
    double rate = 48000.0;
    bool opened = false, failed = false;
};
}
