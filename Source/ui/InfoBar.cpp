#include "InfoBar.h"
#include "../PluginProcessor.h"

namespace mu
{
using namespace theme;

InfoBar::InfoBar (MUProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }

void InfoBar::setStatus (const juce::String& s) { status = s; statusLeft = 9.0f; }

HelpEntry InfoBar::coach (int tab) const
{
    const auto& l = proc.getLoudness();
    if (! proc.isPlaying())
        return { "Press play", "The plugin measures and teaches from what it hears. Start playback, then hover over any control to find out what it does." };
    if (l.getTruePeakHold() > -0.05f)
        return { "Clipping", "The output has gone over 0 dB. Lower the output or the track's trim, then click the meter on the right to clear the warning." };
    const float compGr = proc.getCompressor().getGainReductionDb();
    if (compGr < -10.0f)
        return { "Heavy compression", "The compressor is taking off " + juce::String (juce::roundToInt (-compGr)) + " dB. Unless you want it squashed, raise the threshold or lower the ratio until the loud parts show 3 to 6 dB." };
    if (proc.isMasterInstance() && proc.getMaximizer().getGainReductionDb() < -5.0f)
        return { "Limiter working hard", "The limiter is holding back more than 5 dB. The master will be loud but flat. Lower the loudness target or turn down the loudest element in the mix." };
    if (proc.getDeEsser().getGainReductionDb() < -9.0f)
        return { "De-esser biting", "The de-esser is cutting more than 9 dB, which starts to sound like a lisp. Raise its threshold or reduce the range." };
    if (proc.getStereo().getCorrelation() < -0.1f)
        return { "Mono problem", "Left and right are fighting each other. Parts of this track will vanish on phone speakers. Narrow the width or check for a flipped channel." };
    for (int b = 0; b < kNumEQBands; ++b)
    {
        const auto s = proc.getBandSettings (b);
        if (s.enabled && (s.type == EQType::Bell || s.type == EQType::LowShelf || s.type == EQType::HighShelf) && s.gainDb > 8.0f)
            return { "Big boost", "Band " + juce::String (b + 1) + " is boosting " + juce::String (s.gainDb, 1) + " dB. When a boost that large is needed, try cutting what is covering that range instead, or go back to the source sound." };
    }
    switch (tab)
    {
        case 0: return { "Mix", "Put the plugin on every track and on the Stereo Out (role: Master). Play the loudest section and press Learn and balance. Then fix the collisions it lists." };
        case 1: return { "EQ", "Cut what is wrong before boosting what is right. Press Analyse for suggestions, or double-click the display to add a band and drag it." };
        case 2: return { "Dynamics", "Watch the yellow reduction while you listen. It should move with the performance: down on loud words, back up in the gaps." };
        case 3: return { "Tone", "These are finishing tools. Switch each one off and on while the song plays; keep it only if the track sounds better in the mix, not just on its own." };
        case 4: return { "Master", "Choose where the song is going, play the loudest section, and let the level settle. Then read the release check." };
        default: return { "Learn", "Pick a lesson on the left. The diagrams are live: drag them, then press Try it to use the idea on your own track." };
    }
}

void InfoBar::tick (float dt, juce::Component& root, int tab)
{
    statusLeft = juce::jmax (0.0f, statusLeft - dt);
    coachTimer -= dt;
    if (coachTimer <= 0.0f) { coachEntry = coach (tab); coachTimer = 0.5f; }

    HelpEntry next;
    bool isHelp = false;
    auto* under = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();
    for (auto* c = under; c != nullptr; c = c->getParentComponent())
    {
        if (c == &root) break;
        if (c->getProperties().contains ("help"))
        {
            if (root.isParentOf (c)) { next = helpFor (c->getProperties()["help"].toString()); isHelp = next.title.isNotEmpty(); }
            break;
        }
    }
    if (! isHelp) next = statusLeft > 0.0f ? HelpEntry { "Done", status } : coachEntry;

    if (next.title != shown.title || next.body != shown.body)
    {
        shown = next; shownIsHelp = isHelp; fade = 0.0f;
    }
    if (fade < 1.0f) { fade = juce::jmin (1.0f, fade + dt * 7.0f); repaint(); }
}

void InfoBar::paint (juce::Graphics& g)
{
    g.fillAll (black);
    g.setColour (line);
    g.fillRect (0, 0, getWidth(), 1);
    auto r = getLocalBounds().reduced (16, 0);
    r.removeFromTop (1);

    g.setColour (shownIsHelp ? yellow : textMute);
    g.fillRect (r.removeFromLeft (3).withSizeKeepingCentre (3, 26));
    r.removeFromLeft (12);

    const auto titleFont = bodyBold (12.5f);
    const int titleW = juce::jmin (190, juce::roundToInt (titleFont.getStringWidthFloat (shown.title)) + 18);
    g.setFont (titleFont);
    g.setColour (text.withAlpha (fade));
    g.drawText (shown.title, r.removeFromLeft (titleW), juce::Justification::centredLeft);

    const auto bodyFont = body (12.5f);
    const float h = wrappedHeight (shown.body, (float) r.getWidth(), bodyFont, 1.3f);
    drawWrapped (g, shown.body, r.toFloat().withSizeKeepingCentre ((float) r.getWidth(), juce::jmin (h, (float) r.getHeight())), bodyFont, textDim.withAlpha (fade), 1.3f);
}
}
