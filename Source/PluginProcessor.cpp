#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "license/License.h"

namespace mu
{
MUProcessor::MUProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MUState", createParameterLayout())
{
    auto p = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };
    pRole = p (ids::role); pBypass = p (ids::bypass); pTrim = p (ids::trim); pOut = p (ids::outputGain);
    pAutoBal = p (ids::autoBalance); pTargetLufs = p (ids::targetLufs); pTargetTp = p (ids::targetTruePeak);
    pEqOn = p (ids::eqEnabled);
    for (int b = 0; b < kNumEQBands; ++b)
        pBands[(size_t) b] = { p (ids::eqBandEnabled (b)), p (ids::eqBandType (b)), p (ids::eqBandFreq (b)), p (ids::eqBandGain (b)), p (ids::eqBandQ (b)) };
    pCompOn = p (ids::compEnabled); pThr = p (ids::compThreshold); pRatio = p (ids::compRatio); pAtt = p (ids::compAttack);
    pRel = p (ids::compRelease); pKnee = p (ids::compKnee); pMakeup = p (ids::compMakeup); pAutoMakeup = p (ids::compAutoMakeup); pMix = p (ids::compMix);
    pVsOn = p (ids::vsEnabled); pVsAmount = p (ids::vsAmount); pVsSpeed = p (ids::vsSpeed);

    License::get();   // load the licence on the message thread before the audio thread asks for it
    slot = bus->registerClient (this);
    startTimer (100);
}

MUProcessor::~MUProcessor()
{
    stopTimer();
    bus->unregisterClient (slot);
}

bool MUProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void MUProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int ch = juce::jmax (1, getTotalNumOutputChannels());
    loudness.prepare (sampleRate, samplesPerBlock, ch);
    spectrum.prepare (sampleRate);
    eq.prepare (sampleRate, samplesPerBlock, ch);
    comp.prepare (sampleRate, samplesPerBlock, ch);
    vocalSpace.prepare (sampleRate, samplesPerBlock, ch);
    trimGain.reset (sampleRate, 0.08);
    outGain.reset (sampleRate, 0.03);
    trimGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pTrim->load()));
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOut->load()));
    pullParameters();
}

void MUProcessor::releaseResources() {}

void MUProcessor::pullParameters()
{
    eq.setEnabled (pEqOn->load() > 0.5f);
    for (int b = 0; b < kNumEQBands; ++b)
    {
        auto& bp = pBands[(size_t) b];
        EQBandSettings s;
        s.enabled = bp.on->load() > 0.5f;
        s.type = (EQType) (int) bp.type->load();
        s.freqHz = bp.freq->load();
        s.gainDb = bp.gain->load();
        s.q = bp.q->load();
        eq.setBand (b, s);
    }
    CompressorSettings c;
    c.enabled = pCompOn->load() > 0.5f; c.thresholdDb = pThr->load(); c.ratio = pRatio->load();
    c.attackMs = pAtt->load(); c.releaseMs = pRel->load(); c.kneeDb = pKnee->load();
    c.makeupDb = pMakeup->load(); c.autoMakeup = pAutoMakeup->load() > 0.5f; c.mix = pMix->load() * 0.01f;
    comp.setSettings (c);

    VocalSpace::Settings v;
    const auto role = getEffectiveRole();
    const bool vsAllowed = role != Role::Vocal && role != Role::BackingVocal && role != Role::Master && License::get().isPro();
    v.enabled = vsAllowed && pVsOn->load() > 0.5f;
    v.amount = pVsAmount->load() * 0.01f;
    v.speed = (int) pVsSpeed->load();
    vocalSpace.setSettings (v);
}

void MUProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);
    if (n == 0) return;

    pullParameters();
    const bool bypassed = pBypass->load() > 0.5f;

    if (! bypassed)
    {
        trimGain.setTargetValue (juce::Decibels::decibelsToGain (pTrim->load()));
        if (trimGain.isSmoothing())
        {
            for (int i = 0; i < n; ++i)
            {
                const float g = trimGain.getNextValue();
                for (int c = 0; c < buffer.getNumChannels(); ++c) buffer.getWritePointer (c)[i] *= g;
            }
        }
        else buffer.applyGain (trimGain.getCurrentValue());

        eq.process (buffer);
        comp.process (buffer);
        vocalSpace.setKeyLevel (bus->getVocalKeyLevel());
        vocalSpace.process (buffer);

        outGain.setTargetValue (juce::Decibels::decibelsToGain (pOut->load()));
        if (outGain.isSmoothing())
        {
            for (int i = 0; i < n; ++i)
            {
                const float g = outGain.getNextValue();
                for (int c = 0; c < buffer.getNumChannels(); ++c) buffer.getWritePointer (c)[i] *= g;
            }
        }
        else buffer.applyGain (outGain.getCurrentValue());
    }

    loudness.process (buffer);
    spectrum.pushBlock (buffer);
    bus->publish (slot, loudness.getMomentary(), loudness.getShortTerm(), loudness.getIntegrated(),
                  loudness.getTruePeakHold(), loudness.getCrestFactorDb(), loudness.isActive(), getEffectiveRole());
}

void MUProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    loudness.process (buffer);
    spectrum.pushBlock (buffer);
}

void MUProcessor::timerCallback()
{
    loudness.readIntegrated();
    for (int i = 0; i < 4 && spectrum.update(); ++i) {}

    RoleFeatures f;
    f.valid = spectrum.hasLongTerm() && loudness.isActive();
    f.lowEnergyRatio = spectrum.lowEnergyRatio();
    f.highEnergyRatio = spectrum.highEnergyRatio();
    f.centroidHz = spectrum.centroidHz();
    f.crestFactorDb = loudness.getCrestFactorDb();
    if (f.valid)
    {
        detected = detectRole (f);
        detectedRoleAtomic.store ((int) detected.role);
    }

    if (isMasterInstance())
        bus->setAutoBalance (autoBalanceMode && License::get().isPro(), currentTargets());
}

BalanceTargets MUProcessor::currentTargets() const
{
    BalanceTargets t;
    t.masterLufs = pTargetLufs->load();
    t.masterTruePeak = pTargetTp->load();
    return t;
}

// ---- SessionClient -------------------------------------------------------------------------------
juce::String MUProcessor::getDisplayName() const
{
    if (trackName.isNotEmpty()) return trackName;
    const auto r = getEffectiveRole();
    if (r == Role::Master) return "Master";
    return roleNames()[(int) r] + " " + juce::String (instanceNumber);
}

Role MUProcessor::getEffectiveRole() const
{
    const int r = (int) pRole->load();
    if (r != (int) Role::Auto) return (Role) r;
    return (Role) detectedRoleAtomic.load();
}

float MUProcessor::getTrimDb() const { return pTrim->load(); }
bool MUProcessor::acceptsBalance() const { return pAutoBal->load() > 0.5f && ! isMasterInstance(); }

void MUProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

float MUProcessor::getParam (const juce::String& id) const
{
    if (auto* v = apvts.getRawParameterValue (id)) return v->load();
    return 0.0f;
}

void MUProcessor::setTrimDb (float db) { setParam (ids::trim, juce::jlimit (-24.0f, 24.0f, db)); }

bool MUProcessor::fillBandsDb (std::array<float, kAnalysisBands>& out) const
{
    if (! spectrum.hasLongTerm()) return false;
    const auto& b = spectrum.getLongTermBandsDb();
    for (int i = 0; i < kAnalysisBands; ++i) out[(size_t) i] = b[(size_t) i];
    return true;
}

void MUProcessor::applyMaskingCut (float hz, float db)
{
    // Prefer a free band in 3..6; otherwise reuse the enabled band nearest the frequency.
    int band = -1;
    for (int b = 2; b < kNumEQBands - 1; ++b)
        if (getParam (ids::eqBandEnabled (b)) < 0.5f) { band = b; break; }
    if (band < 0)
    {
        float best = 1.0e9f;
        for (int b = 2; b < kNumEQBands - 1; ++b)
        {
            const float d = std::abs (std::log (getParam (ids::eqBandFreq (b)) / hz));
            if (d < best) { best = d; band = b; }
        }
    }
    setParam (ids::eqBandType (band), (float) EQType::Bell);
    setParam (ids::eqBandFreq (band), hz);
    setParam (ids::eqBandGain (band), juce::jlimit (-6.0f, 0.0f, db));
    setParam (ids::eqBandQ (band), 1.4f);
    setParam (ids::eqBandEnabled (band), 1.0f);
    setParam (ids::eqEnabled, 1.0f);
}

// ---- Actions -------------------------------------------------------------------------------------
void MUProcessor::setAutoBalanceMode (bool on)
{
    autoBalanceMode = on;
    apvts.state.setProperty ("autoBalanceMode", on, nullptr);
    if (isMasterInstance()) bus->setAutoBalance (on && License::get().isPro(), currentTargets());
}

void MUProcessor::balanceNow()
{
    auto plan = bus->computePlan (currentTargets());
    if (plan.usable) bus->applyPlan (plan, 1.0f);
}

juce::String MUProcessor::cleanHarsh()
{
    auto res = spectrum.findResonances (1200.0f, 12000.0f, 5.0f);
    if (res.empty()) return "Nothing harsh stands out in the long-term spectrum yet.";
    int band = 3;
    int applied = 0;
    for (auto& r : res)
    {
        if (band > 5) break;
        setParam (ids::eqBandType (band), (float) EQType::Bell);
        setParam (ids::eqBandFreq (band), r.freqHz);
        setParam (ids::eqBandGain (band), -juce::jlimit (1.5f, 5.0f, r.prominenceDb * 0.5f));
        setParam (ids::eqBandQ (band), juce::jlimit (2.0f, 10.0f, r.q));
        setParam (ids::eqBandEnabled (band), 1.0f);
        ++band; ++applied;
    }
    setParam (ids::eqEnabled, 1.0f);
    juce::String s;
    s << "Cut " << applied << " resonance" << (applied == 1 ? "" : "s") << ": ";
    for (int i = 0; i < applied; ++i)
        s << (i ? ", " : "") << juce::String (juce::roundToInt (res[(size_t) i].freqHz)) << " Hz";
    return s;
}

juce::String MUProcessor::autoCompressor()
{
    const float level = loudness.getShortTerm() > -70.0f ? loudness.getShortTerm() : loudness.getMomentary();
    if (level < -60.0f) return "Play the track first so the compressor can measure it.";
    const float crest = loudness.getCrestFactorDb();
    const auto role = getEffectiveRole();
    float ratio = 2.5f, att = 10.0f, rel = 120.0f, thrOffset = 4.0f;
    switch (role)
    {
        case Role::Vocal:        ratio = 3.0f; att = 8.0f;  rel = 90.0f;  thrOffset = 3.0f; break;
        case Role::BackingVocal: ratio = 4.0f; att = 5.0f;  rel = 80.0f;  thrOffset = 2.0f; break;
        case Role::Kick:         ratio = 4.0f; att = 20.0f; rel = 60.0f;  thrOffset = 6.0f; break;
        case Role::Bass808:      ratio = 4.0f; att = 25.0f; rel = 150.0f; thrOffset = 4.0f; break;
        case Role::Drums:        ratio = 3.0f; att = 15.0f; rel = 100.0f; thrOffset = 6.0f; break;
        case Role::Beat:         ratio = 2.0f; att = 30.0f; rel = 200.0f; thrOffset = 6.0f; break;
        case Role::Master:       ratio = 1.5f; att = 30.0f; rel = 300.0f; thrOffset = 8.0f; break;
        default: break;
    }
    // Louder-than-average peaks (high crest) get caught; the threshold sits a little above the average level.
    const float thr = juce::jlimit (-60.0f, 0.0f, level + thrOffset + juce::jmax (0.0f, crest - 12.0f) * 0.3f);
    setParam (ids::compThreshold, thr);
    setParam (ids::compRatio, ratio);
    setParam (ids::compAttack, att);
    setParam (ids::compRelease, rel);
    setParam (ids::compKnee, 6.0f);
    setParam (ids::compAutoMakeup, 1.0f);
    setParam (ids::compMix, 100.0f);
    setParam (ids::compEnabled, 1.0f);
    return "Set for " + roleNames()[(int) role] + ": threshold " + juce::String (thr, 1) + " dB, " + juce::String (ratio, 1) + ":1";
}

void MUProcessor::resetEQ()
{
    for (int b = 0; b < kNumEQBands; ++b)
    {
        setParam (ids::eqBandEnabled (b), 0.0f);
        setParam (ids::eqBandType (b), (float) kDefaultBands[b].type);
        setParam (ids::eqBandFreq (b), kDefaultBands[b].freq);
        setParam (ids::eqBandGain (b), 0.0f);
        setParam (ids::eqBandQ (b), 0.71f);
    }
}

void MUProcessor::resetTrim() { setParam (ids::trim, 0.0f); }
void MUProcessor::resetMeters() { loudness.resetPeakHold(); loudness.resetIntegrated(); }

EQBandSettings MUProcessor::getBandSettings (int b) const
{
    const auto& bp = pBands[(size_t) juce::jlimit (0, kNumEQBands - 1, b)];
    EQBandSettings s;
    s.enabled = bp.on->load() > 0.5f;
    s.type = (EQType) (int) bp.type->load();
    s.freqHz = bp.freq->load();
    s.gainDb = bp.gain->load();
    s.q = bp.q->load();
    return s;
}

float MUProcessor::eqBandMagnitudeDbAt (int band, float hz) const
{
    const auto s = getBandSettings (band);
    if (! s.enabled) return 0.0f;
    const double fs = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    return EQEngine::magnitudeDb (EQEngine::computeCoefficients (s, fs), hz, fs);
}

float MUProcessor::eqMagnitudeDbAt (float hz) const
{
    float sum = 0.0f;
    for (int b = 0; b < kNumEQBands; ++b) sum += eqBandMagnitudeDbAt (b, hz);
    return sum;
}

juce::Colour MUProcessor::roleColour (Role r)
{
    switch (r)
    {
        case Role::Vocal:        return juce::Colour (0xffff5c7a);
        case Role::BackingVocal: return juce::Colour (0xffff8fa5);
        case Role::Kick:         return juce::Colour (0xfff2b84b);
        case Role::Bass808:      return juce::Colour (0xffd98c2b);
        case Role::Drums:        return juce::Colour (0xffc9c9c9);
        case Role::Melody:       return juce::Colour (0xff6fc3ff);
        case Role::Beat:         return juce::Colour (0xff9b7bff);
        case Role::FX:           return juce::Colour (0xff5be0c8);
        case Role::Master:       return juce::Colour (0xffffffff);
        default:                 return juce::Colour (0xff8a8a8a);
    }
}

// ---- State ---------------------------------------------------------------------------------------
void MUProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("autoBalanceMode", autoBalanceMode, nullptr);
    state.setProperty ("uiTab", currentTab.load(), nullptr);
    state.setProperty ("version", MU_VERSION_STRING, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void MUProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (tree);
            autoBalanceMode = (bool) tree.getProperty ("autoBalanceMode", false);
            currentTab.store ((int) tree.getProperty ("uiTab", 0));
        }
    }
}

void MUProcessor::updateTrackProperties (const TrackProperties& properties)
{
    if (properties.name.isNotEmpty()) trackName = properties.name;
    if (bus->onSessionChanged) bus->onSessionChanged();
}

juce::AudioProcessorEditor* MUProcessor::createEditor() { return new MUEditor (*this); }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new mu::MUProcessor(); }
