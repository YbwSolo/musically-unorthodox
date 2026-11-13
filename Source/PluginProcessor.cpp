#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace mu
{
MUProcessor::MUProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MUState", createParameterLayout())
{
    auto p = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };
    pRole = p (ids::role); pBypass = p (ids::bypass); pTrim = p (ids::trim); pOut = p (ids::outputGain); pAutoBal = p (ids::autoBalance);
    pTargetLufs = p (ids::targetLufs); pTargetTp = p (ids::targetTruePeak); pMasterAutoTrim = p (ids::masterAutoTrim); pPeakGuard = p (ids::peakGuard);
    for (int r = (int) Role::Vocal; r < (int) Role::Master; ++r) pRoleOff[(size_t) r] = p (ids::roleOffset ((Role) r));
    pGateOn = p (ids::gateOn); pGateThr = p (ids::gateThreshold); pGateRange = p (ids::gateRange); pGateAtt = p (ids::gateAttack); pGateRel = p (ids::gateRelease); pGateHold = p (ids::gateHold);
    pEqOn = p (ids::eqEnabled);
    for (int b = 0; b < kNumEQBands; ++b)
        pBands[(size_t) b] = { p (ids::eqBandEnabled (b)), p (ids::eqBandType (b)), p (ids::eqBandFreq (b)), p (ids::eqBandGain (b)), p (ids::eqBandQ (b)),
                               p (ids::eqBandDyn (b)), p (ids::eqBandThr (b)), p (ids::eqBandRange (b)) };
    pCompOn = p (ids::compEnabled); pThr = p (ids::compThreshold); pRatio = p (ids::compRatio); pAtt = p (ids::compAttack);
    pRel = p (ids::compRelease); pKnee = p (ids::compKnee); pMakeup = p (ids::compMakeup); pAutoMakeup = p (ids::compAutoMakeup); pMix = p (ids::compMix);
    pDsOn = p (ids::dsOn); pDsFreq = p (ids::dsFreq); pDsThr = p (ids::dsThreshold); pDsRange = p (ids::dsRange); pDsMode = p (ids::dsMode);
    pSatOn = p (ids::satOn); pSatDrive = p (ids::satDrive); pSatMix = p (ids::satMix); pSatType = p (ids::satType);
    pRiderOn = p (ids::riderOn); pRiderTarget = p (ids::riderTarget); pRiderRange = p (ids::riderRange); pRiderSpeed = p (ids::riderSpeed);
    pVsOn = p (ids::vsEnabled); pVsAmount = p (ids::vsAmount); pVsSpeed = p (ids::vsSpeed);

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
    gate.prepare (sampleRate, samplesPerBlock, ch);
    eq.prepare (sampleRate, samplesPerBlock, ch);
    comp.prepare (sampleRate, samplesPerBlock, ch);
    deEsser.prepare (sampleRate, samplesPerBlock, ch);
    saturator.prepare (sampleRate, samplesPerBlock, ch);
    rider.prepare (sampleRate, samplesPerBlock, ch);
    vocalSpace.prepare (sampleRate, samplesPerBlock, ch);
    peakGuard.prepare (sampleRate, samplesPerBlock, ch);
    trimGain.reset (sampleRate, 0.08);
    outGain.reset (sampleRate, 0.03);
    masterTrimGain.reset (sampleRate, 0.5);
    trimGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pTrim->load()));
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOut->load()));
    masterTrimGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (masterTrimDb.load()));
    pullParameters();
    reportedLatency = -1;
    updateLatency();
}

void MUProcessor::releaseResources() {}

void MUProcessor::updateLatency()
{
    // Saturator oversampling is always running (constant latency). The peak guard's look-ahead delay also
    // always runs, so the total is fixed per instance regardless of what is switched on.
    const int latency = saturator.getLatencySamples() + peakGuard.getLatencySamples();
    if (latency != reportedLatency)
    {
        reportedLatency = latency;
        setLatencySamples (latency);
    }
}

void MUProcessor::pullParameters()
{
    GateSettings g;
    g.enabled = pGateOn->load() > 0.5f; g.thresholdDb = pGateThr->load(); g.rangeDb = pGateRange->load();
    g.attackMs = pGateAtt->load(); g.releaseMs = pGateRel->load(); g.holdMs = pGateHold->load();
    gate.setSettings (g);

    eq.setEnabled (pEqOn->load() > 0.5f);
    for (int b = 0; b < kNumEQBands; ++b) eq.setBand (b, getBandSettings (b));

    CompressorSettings c;
    c.enabled = pCompOn->load() > 0.5f; c.thresholdDb = pThr->load(); c.ratio = pRatio->load();
    c.attackMs = pAtt->load(); c.releaseMs = pRel->load(); c.kneeDb = pKnee->load();
    c.makeupDb = pMakeup->load(); c.autoMakeup = pAutoMakeup->load() > 0.5f; c.mix = pMix->load() * 0.01f;
    comp.setSettings (c);

    DeEsserSettings d;
    d.enabled = pDsOn->load() > 0.5f; d.freqHz = pDsFreq->load(); d.thresholdDb = pDsThr->load(); d.rangeDb = pDsRange->load(); d.splitBand = pDsMode->load() < 0.5f;
    deEsser.setSettings (d);

    SaturatorSettings s;
    s.enabled = pSatOn->load() > 0.5f; s.driveDb = pSatDrive->load(); s.mix = pSatMix->load() * 0.01f; s.type = (int) pSatType->load();
    saturator.setSettings (s);

    RiderSettings r;
    r.enabled = pRiderOn->load() > 0.5f; r.targetDb = pRiderTarget->load(); r.rangeDb = pRiderRange->load(); r.speed = (int) pRiderSpeed->load();
    rider.setSettings (r);

    VocalSpace::Settings v;
    const auto role = getEffectiveRole();
    const bool vsAllowed = role != Role::Vocal && role != Role::BackingVocal && role != Role::Master;
    v.enabled = vsAllowed && pVsOn->load() > 0.5f;
    v.amount = pVsAmount->load() * 0.01f;
    v.speed = (int) pVsSpeed->load();
    vocalSpace.setSettings (v);

    peakGuard.setEnabled (role == Role::Master && pPeakGuard->load() > 0.5f);
    peakGuard.setCeilingDb (pTargetTp->load());
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
    const bool master = getEffectiveRole() == Role::Master;

    auto applySmoothedGain = [&] (juce::SmoothedValue<float>& g)
    {
        if (g.isSmoothing())
        {
            for (int i = 0; i < n; ++i)
            {
                const float v = g.getNextValue();
                for (int c = 0; c < buffer.getNumChannels(); ++c) buffer.getWritePointer (c)[i] *= v;
            }
        }
        else buffer.applyGain (g.getCurrentValue());
    };

    if (! bypassed)
    {
        trimGain.setTargetValue (juce::Decibels::decibelsToGain (pTrim->load()));
        applySmoothedGain (trimGain);
        gate.process (buffer);
        eq.process (buffer);
        comp.process (buffer);
        deEsser.process (buffer);
        saturator.process (buffer);
        rider.process (buffer);
        vocalSpace.setKeyLevel (bus->getVocalKeyLevel());
        vocalSpace.process (buffer);
        outGain.setTargetValue (juce::Decibels::decibelsToGain (pOut->load()));
        applySmoothedGain (outGain);

        masterTrimGain.setTargetValue (juce::Decibels::decibelsToGain (master ? masterTrimDb.load() : 0.0f));
        applySmoothedGain (masterTrimGain);
        peakGuard.process (buffer);
    }
    else
    {
        // keep latency-bearing stages running so bypass does not shift timing
        saturator.process (buffer);
        peakGuard.process (buffer);
    }

    loudness.process (buffer);
    spectrum.pushBlock (buffer);
    samplesProcessed.fetch_add ((uint64_t) n, std::memory_order_relaxed);
    bus->publish (slot, loudness.getMomentary(), loudness.getShortTerm(), loudness.getIntegrated(),
                  loudness.getTruePeakHold(), loudness.getCrestFactorDb(), loudness.isActive(), getEffectiveRole());
}

void MUProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    saturator.process (buffer);
    peakGuard.process (buffer);
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
    {
        bus->setAutoBalance (autoBalanceMode, currentTargets());

        // Master auto trim: every couple of seconds, step the master's own gain by the measured error and
        // restart the integrated measurement so the next reading reflects the new gain (converges in 2-3 steps).
        if (pMasterAutoTrim->load() > 0.5f)
        {
            const double now = (double) samplesProcessed.load() / juce::jmax (1.0, getSampleRate());   // audio time, not wall time
            const float integ = loudness.getIntegrated();
            if (integ > -60.0f && now - lastAutoTrimStepMs > 2.5)
            {
                const float error = pTargetLufs->load() - integ;        // positive = too quiet
                if (std::abs (error) > 0.2f)
                {
                    masterTrimDb.store (juce::jlimit (-12.0f, 12.0f, masterTrimDb.load() + juce::jlimit (-6.0f, 6.0f, error)));
                    loudness.resetIntegrated();
                }
                lastAutoTrimStepMs = now;
            }
        }
        else if (std::abs (masterTrimDb.load()) > 0.001f)
            masterTrimDb.store (0.0f);
    }
    else if (std::abs (masterTrimDb.load()) > 0.001f)
        masterTrimDb.store (0.0f);
}

BalanceTargets MUProcessor::currentTargets() const
{
    BalanceTargets t;
    t.masterLufs = pTargetLufs->load();
    t.masterTruePeak = pTargetTp->load();
    for (int r = (int) Role::Vocal; r < (int) Role::Master; ++r)
        if (pRoleOff[(size_t) r] != nullptr) t.offsets[(size_t) r] = pRoleOff[(size_t) r]->load();
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
    setParam (ids::eqBandGain (band), 0.0f);
    setParam (ids::eqBandQ (band), 1.4f);
    // Masking cuts are dynamic: they only bite when this track is actually loud there.
    setParam (ids::eqBandDyn (band), 1.0f);
    setParam (ids::eqBandThr (band), -30.0f);
    setParam (ids::eqBandRange (band), juce::jlimit (-6.0f, -1.0f, db));
    setParam (ids::eqBandEnabled (band), 1.0f);
    setParam (ids::eqEnabled, 1.0f);
}

// ---- Actions -------------------------------------------------------------------------------------
void MUProcessor::setAutoBalanceMode (bool on)
{
    autoBalanceMode = on;
    apvts.state.setProperty ("autoBalanceMode", on, nullptr);
    if (isMasterInstance()) bus->setAutoBalance (on, currentTargets());
}

void MUProcessor::balanceNow()
{
    auto plan = bus->computePlan (currentTargets());
    if (plan.usable) bus->applyPlan (plan, 1.0f);
}

void MUProcessor::learnAndBalance (float seconds)
{
    bus->startLearn (seconds, currentTargets());
}

juce::String MUProcessor::cleanHarsh()
{
    auto res = spectrum.findResonances (1200.0f, 12000.0f, 5.0f);
    if (res.empty()) return "Nothing harsh stands out in the long-term spectrum yet. Play the track for 10 seconds first.";
    int band = 3;
    int applied = 0;
    for (auto& r : res)
    {
        if (band > 5) break;
        setParam (ids::eqBandType (band), (float) EQType::Bell);
        setParam (ids::eqBandFreq (band), r.freqHz);
        setParam (ids::eqBandGain (band), 0.0f);
        setParam (ids::eqBandQ (band), juce::jlimit (2.0f, 10.0f, r.q));
        setParam (ids::eqBandDyn (band), 1.0f);
        setParam (ids::eqBandThr (band), -36.0f);
        setParam (ids::eqBandRange (band), -juce::jlimit (2.0f, 6.0f, r.prominenceDb * 0.6f));
        setParam (ids::eqBandEnabled (band), 1.0f);
        ++band; ++applied;
    }
    setParam (ids::eqEnabled, 1.0f);
    juce::String s;
    s << "Dynamic cuts on " << applied << " resonance" << (applied == 1 ? "" : "s") << ": ";
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
        case Role::Vocal:        ratio = 4.0f; att = 3.0f;  rel = 80.0f;  thrOffset = 2.0f; break;
        case Role::BackingVocal: ratio = 6.0f; att = 2.0f;  rel = 70.0f;  thrOffset = 1.0f; break;
        case Role::Kick:         ratio = 4.0f; att = 20.0f; rel = 60.0f;  thrOffset = 6.0f; break;
        case Role::Bass808:      ratio = 4.0f; att = 25.0f; rel = 160.0f; thrOffset = 4.0f; break;
        case Role::Drums:        ratio = 3.0f; att = 15.0f; rel = 100.0f; thrOffset = 6.0f; break;
        case Role::Beat:         ratio = 2.0f; att = 30.0f; rel = 200.0f; thrOffset = 6.0f; break;
        case Role::Melody:       ratio = 2.5f; att = 20.0f; rel = 150.0f; thrOffset = 5.0f; break;
        case Role::Master:       ratio = 1.5f; att = 30.0f; rel = 300.0f; thrOffset = 8.0f; break;
        default: break;
    }
    const float thr = juce::jlimit (-60.0f, 0.0f, level + thrOffset + juce::jmax (0.0f, crest - 12.0f) * 0.3f);
    setParam (ids::compThreshold, thr);
    setParam (ids::compRatio, ratio);
    setParam (ids::compAttack, att);
    setParam (ids::compRelease, rel);
    setParam (ids::compKnee, role == Role::Vocal ? 3.0f : 6.0f);
    setParam (ids::compAutoMakeup, 1.0f);
    setParam (ids::compMix, 100.0f);
    setParam (ids::compEnabled, 1.0f);
    return "Set for " + roleNames()[(int) role] + ": threshold " + juce::String (thr, 1) + " dB, " + juce::String (ratio, 1) + ":1";
}

juce::String MUProcessor::applyChainPreset (int index)
{
    const auto& presets = chainPresets();
    if (index < 0 || index >= (int) presets.size()) return {};
    const auto& p = presets[(size_t) index];
    for (auto& pv : p.values) setParam (pv.id, pv.value);
    currentPreset = index;
    apvts.state.setProperty ("chainPreset", index, nullptr);

    // Fit the dynamics to what the track is actually doing, if it has been heard.
    juce::String extra;
    const float level = loudness.getShortTerm();
    if (level > -60.0f && getParam (ids::compEnabled) > 0.5f)
    {
        const float thrOffset = p.role == Role::Vocal ? 2.0f : p.role == Role::BackingVocal ? 1.0f : 5.0f;
        setParam (ids::compThreshold, juce::jlimit (-60.0f, 0.0f, level + thrOffset));
        if (getParam (ids::dsOn) > 0.5f) setParam (ids::dsThreshold, juce::jlimit (-60.0f, 0.0f, level - 8.0f));
        if (getParam (ids::gateOn) > 0.5f) setParam (ids::gateThreshold, juce::jlimit (-80.0f, 0.0f, level - 30.0f));
        if (getParam (ids::riderOn) > 0.5f) setParam (ids::riderTarget, juce::jlimit (-40.0f, -6.0f, level - 1.0f));
        extra = " Thresholds fitted to the measured level (" + juce::String (level, 1) + " LUFS).";
    }
    else if (getParam (ids::compEnabled) > 0.5f)
        extra = " Play the track and re-apply (or hit Auto Set) to fit the thresholds to it.";
    return p.name + " loaded." + extra;
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
        setParam (ids::eqBandDyn (b), 0.0f);
        setParam (ids::eqBandThr (b), -30.0f);
        setParam (ids::eqBandRange (b), -6.0f);
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
    s.dynamic = bp.dyn->load() > 0.5f;
    s.thresholdDb = bp.thr->load();
    s.rangeDb = bp.range->load();
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
        case Role::Vocal:        return juce::Colour (0xfffbf20d);
        case Role::BackingVocal: return juce::Colour (0xfff7e67a);
        case Role::Kick:         return juce::Colour (0xffffffff);
        case Role::Bass808:      return juce::Colour (0xffd9d9d9);
        case Role::Drums:        return juce::Colour (0xffb0b0b0);
        case Role::Melody:       return juce::Colour (0xff8c8c8c);
        case Role::Beat:         return juce::Colour (0xffe8c200);
        case Role::FX:           return juce::Colour (0xff6e6e6e);
        case Role::Master:       return juce::Colour (0xfffbf20d);
        default:                 return juce::Colour (0xff5a5a5a);
    }
}

// ---- State ---------------------------------------------------------------------------------------
void MUProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("autoBalanceMode", autoBalanceMode, nullptr);
    state.setProperty ("uiTab", currentTab.load(), nullptr);
    state.setProperty ("chainPreset", currentPreset, nullptr);
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
            currentPreset = (int) tree.getProperty ("chainPreset", -1);
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
