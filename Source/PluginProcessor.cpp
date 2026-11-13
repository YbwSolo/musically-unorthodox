#include "PluginProcessor.h"
#include "ai/TutorKnowledge.h"
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
    pTargetLufs = p (ids::targetLufs); pTargetTp = p (ids::targetTruePeak); pMasterAutoTrim = p (ids::masterAutoTrim); pPeakGuard = p (ids::peakGuard); pMaxGain = p (ids::maxGain);
    for (int r = (int) Role::Vocal; r < (int) Role::Master; ++r) pRoleOff[(size_t) r] = p (ids::roleOffset ((Role) r));
    pGateOn = p (ids::gateOn); pGateThr = p (ids::gateThreshold); pGateRange = p (ids::gateRange); pGateAtt = p (ids::gateAttack); pGateRel = p (ids::gateRelease); pGateHold = p (ids::gateHold);
    pEqOn = p (ids::eqEnabled); pEqAutoGain = p (ids::eqAutoGain);
    for (int b = 0; b < kNumEQBands; ++b)
        pBands[(size_t) b] = { p (ids::eqBandEnabled (b)), p (ids::eqBandType (b)), p (ids::eqBandFreq (b)), p (ids::eqBandGain (b)), p (ids::eqBandQ (b)),
                               p (ids::eqBandDyn (b)), p (ids::eqBandThr (b)), p (ids::eqBandRange (b)), p (ids::eqBandSlope (b)), p (ids::eqBandMode (b)) };
    pCompOn = p (ids::compEnabled); pThr = p (ids::compThreshold); pRatio = p (ids::compRatio); pAtt = p (ids::compAttack);
    pRel = p (ids::compRelease); pKnee = p (ids::compKnee); pMakeup = p (ids::compMakeup); pAutoMakeup = p (ids::compAutoMakeup); pMix = p (ids::compMix);
    pCompStyle = p (ids::compStyle); pCompScHp = p (ids::compScHp);
    pDsOn = p (ids::dsOn); pDsFreq = p (ids::dsFreq); pDsThr = p (ids::dsThreshold); pDsRange = p (ids::dsRange); pDsMode = p (ids::dsMode);
    pSmOn = p (ids::smOn); pSmDepth = p (ids::smDepth); pSmFocus = p (ids::smFocus); pSmSpeed = p (ids::smSpeed);
    pSatOn = p (ids::satOn); pSatDrive = p (ids::satDrive); pSatMix = p (ids::satMix); pSatType = p (ids::satType);
    pRiderOn = p (ids::riderOn); pRiderTarget = p (ids::riderTarget); pRiderRange = p (ids::riderRange); pRiderSpeed = p (ids::riderSpeed);
    pVsOn = p (ids::vsEnabled); pVsAmount = p (ids::vsAmount); pVsSpeed = p (ids::vsSpeed);
    pStWidth = p (ids::stWidth); pStMonoBass = p (ids::stMonoBass); pStMonoFreq = p (ids::stMonoFreq);

    // A/B covers everything that shapes the sound; routing and session balance stay put.
    for (auto* param : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            const auto id = ranged->getParameterID();
            if (id == ids::role || id == ids::bypass || id == ids::trim || id == ids::autoBalance || id.startsWith ("roleOff_")) continue;
            abParams.push_back (ranged);
        }

    lufsHistory.fill (-100.0f);
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
    mixMeter.prepare (sampleRate, samplesPerBlock, ch);
    spectrum.prepare (sampleRate);
    preSpectrum.prepare (sampleRate);
    gate.prepare (sampleRate, samplesPerBlock, ch);
    eq.prepare (sampleRate, samplesPerBlock, ch);
    comp.prepare (sampleRate, samplesPerBlock, ch);
    deEsser.prepare (sampleRate, samplesPerBlock, ch);
    smoother.prepare (sampleRate, samplesPerBlock, ch);
    saturator.prepare (sampleRate, samplesPerBlock, ch);
    rider.prepare (sampleRate, samplesPerBlock, ch);
    vocalSpace.prepare (sampleRate, samplesPerBlock, ch);
    stereo.prepare (sampleRate, samplesPerBlock, ch);
    maximizer.prepare (sampleRate, samplesPerBlock, ch);
    deltaBuffer.setSize (ch, juce::jmax (64, samplesPerBlock), false, true, false);
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
    // The saturator's oversampling delay is always in the path. The maximizer's look-ahead only exists on the
    // master instance, so vocal and instrument tracks stay at about a millisecond for recording.
    const int latency = saturator.getLatencySamples() + (isMasterInstance() ? maximizer.getLatencySamples() : 0);
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
    eq.setAutoGain (pEqAutoGain->load() > 0.5f);
    for (int b = 0; b < kNumEQBands; ++b) eq.setBand (b, getBandSettings (b));

    CompressorSettings c;
    c.enabled = pCompOn->load() > 0.5f; c.thresholdDb = pThr->load(); c.ratio = pRatio->load();
    c.attackMs = pAtt->load(); c.releaseMs = pRel->load(); c.kneeDb = pKnee->load();
    c.makeupDb = pMakeup->load(); c.autoMakeup = pAutoMakeup->load() > 0.5f; c.mix = pMix->load() * 0.01f;
    c.style = (CompStyle) (int) pCompStyle->load(); c.sidechainHpHz = pCompScHp->load();
    comp.setSettings (c);

    DeEsserSettings d;
    d.enabled = pDsOn->load() > 0.5f; d.freqHz = pDsFreq->load(); d.thresholdDb = pDsThr->load(); d.rangeDb = pDsRange->load(); d.splitBand = pDsMode->load() < 0.5f;
    deEsser.setSettings (d);

    SmootherSettings sm;
    sm.enabled = pSmOn->load() > 0.5f; sm.depth = pSmDepth->load() * 0.01f; sm.focus = (int) pSmFocus->load(); sm.speed = (int) pSmSpeed->load();
    smoother.setSettings (sm);

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

    StereoSettings st;
    st.width = pStWidth->load() * 0.01f; st.monoBass = pStMonoBass->load() > 0.5f; st.monoBelowHz = pStMonoFreq->load();
    stereo.setSettings (st);

    maximizer.setActive (role == Role::Master);
    maximizer.setEnabled (role == Role::Master && pPeakGuard->load() > 0.5f);
    maximizer.setGainDb (pMaxGain->load());
    maximizer.setCeilingDb (pTargetTp->load());
}

void MUProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);
    if (n == 0) return;

    pullParameters();
    const bool bypassed = pBypass->load() > 0.5f;
    const bool master = getEffectiveRole() == Role::Master;
    if (editorsOpen.load (std::memory_order_relaxed) > 0) preSpectrum.pushBlock (buffer);

    auto applySmoothedGain = [&] (juce::SmoothedValue<float>& g)
    {
        if (g.isSmoothing())
        {
            for (int i = 0; i < n; ++i)
            {
                const float v = g.getNextValue();
                for (int c = 0; c < numCh; ++c) buffer.getWritePointer (c)[i] *= v;
            }
        }
        else buffer.applyGain (g.getCurrentValue());
    };

    if (! bypassed)
    {
        const auto listen = (Listen) listenMode.load (std::memory_order_relaxed);
        const bool canDelta = n <= deltaBuffer.getNumSamples() && numCh <= deltaBuffer.getNumChannels();
        bool deltaOnly = false;

        trimGain.setTargetValue (juce::Decibels::decibelsToGain (pTrim->load()));
        applySmoothedGain (trimGain);
        gate.process (buffer);

        if (listen == Listen::EqDelta && canDelta)
        {
            for (int c = 0; c < numCh; ++c) deltaBuffer.copyFrom (c, 0, buffer, c, 0, n);
            eq.process (buffer);
            for (int c = 0; c < numCh; ++c) buffer.addFrom (c, 0, deltaBuffer, c, 0, n, -1.0f);   // what the EQ changed
            deltaOnly = true;
        }
        else eq.process (buffer);

        if (! deltaOnly)
        {
            comp.process (buffer);
            deEsser.process (buffer);
            if (listen == Listen::SmoothDelta && canDelta)
            {
                for (int c = 0; c < numCh; ++c) deltaBuffer.copyFrom (c, 0, buffer, c, 0, n);
                smoother.process (buffer);
                for (int c = 0; c < numCh; ++c)
                {
                    auto* out = buffer.getWritePointer (c);
                    const auto* in = deltaBuffer.getReadPointer (c);
                    for (int i = 0; i < n; ++i) out[i] = in[i] - out[i];                          // what Smooth removed
                }
                deltaOnly = true;
            }
            else smoother.process (buffer);
        }

        if (deltaOnly) saturator.setSettings ({});     // delay only, so timing stays the same while auditioning
        saturator.process (buffer);

        if (! deltaOnly)
        {
            rider.process (buffer);
            vocalSpace.setKeyLevel (bus->getVocalKeyLevel());
            vocalSpace.process (buffer);
            stereo.process (buffer);
            outGain.setTargetValue (juce::Decibels::decibelsToGain (pOut->load()));
            applySmoothedGain (outGain);
        }

        if (master) mixMeter.process (buffer);         // the mix as it arrives, before any mastering gain
        masterTrimGain.setTargetValue (juce::Decibels::decibelsToGain (master ? masterTrimDb.load() : 0.0f));
        applySmoothedGain (masterTrimGain);
        maximizer.process (buffer);
    }
    else
    {
        // keep the latency-bearing stages running so bypass does not shift timing
        saturator.setSettings ({});
        saturator.process (buffer);
        if (master) mixMeter.process (buffer);
        maximizer.setEnabled (false);
        maximizer.process (buffer);
    }

    loudness.process (buffer);
    spectrum.pushBlock (buffer);
    samplesProcessed.fetch_add ((uint64_t) n, std::memory_order_relaxed);
    // The session balance works on the mix itself, so the master reports its level before the mastering stage.
    const auto& shared = master ? mixMeter : loudness;
    bus->publish (slot, shared.getMomentary(), shared.getShortTerm(), shared.getIntegrated(),
                  shared.getTruePeakHold(), shared.getCrestFactorDb(), shared.isActive(), getEffectiveRole());
}

void MUProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (buffer.getNumSamples() == 0) return;
    maximizer.setActive (getEffectiveRole() == Role::Master);
    maximizer.setEnabled (false);
    saturator.setSettings ({});
    saturator.process (buffer);
    maximizer.process (buffer);
    loudness.process (buffer);
    spectrum.pushBlock (buffer);
}

void MUProcessor::pumpAnalysis()
{
    spectrum.pump();
    preSpectrum.pump();
}

void MUProcessor::timerCallback()
{
    loudness.readIntegrated();
    mixMeter.readIntegrated();
    pumpAnalysis();
    updateLatency();
    lufsHistory.push (loudness.getShortTerm());

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

        // A peak that was held before the limiter or ceiling changed says nothing about the current settings.
        const float guardState = pPeakGuard->load() + pTargetTp->load() * 10.0f;
        if (std::abs (guardState - lastGuardState) > 0.01f) { lastGuardState = guardState; loudness.resetPeakHold(); }

        // Master auto trim: every couple of seconds of audio, step the master's own gain by the measured error
        // and restart the integrated measurement so the next reading reflects the new gain. Because the level
        // is read after the limiter, this also finds the drive needed to reach a loud target.
        if (pMasterAutoTrim->load() > 0.5f)
        {
            const double now = (double) samplesProcessed.load() / juce::jmax (1.0, getSampleRate());
            const float integ = loudness.getIntegrated();
            if (integ > -60.0f && now - lastAutoTrimStep > 2.5)
            {
                float error = pTargetLufs->load() - integ;              // positive = too quiet
                // Never chase loudness by crushing the mix: once the limiter is taking more than 6 dB off the
                // peaks, stop pushing, and back off if it is beyond 8 dB.
                float worst = 0.0f;
                for (int i = 0; i < 250; ++i) worst = juce::jmin (worst, maximizer.reductionHistory().get (i));
                bool held = false;
                if (error > 0.0f && worst < -6.0f) { error = worst < -8.0f ? juce::jmax (-3.0f, worst + 8.0f) : 0.0f; held = true; }
                autoLevelHeld.store (held);
                if (std::abs (error) > 0.2f)
                {
                    masterTrimDb.store (juce::jlimit (-24.0f, 24.0f, masterTrimDb.load() + juce::jlimit (-6.0f, 6.0f, error)));
                    loudness.resetIntegrated();
                }
                lastAutoTrimStep = now;
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
    // Tracks are balanced to a mix level with headroom; the mastering stage then takes the mix to its final loudness.
    t.masterLufs = juce::jmin (-18.0f, pTargetLufs->load());
    t.masterTruePeak = juce::jmin (-6.0f, pTargetTp->load());
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
        const float norm = p->convertTo0to1 (value);
        if (std::abs (norm - p->getValue()) < 1.0e-6f) return;
        p->beginChangeGesture();
        p->setValueNotifyingHost (norm);
        p->endChangeGesture();
    }
}

float MUProcessor::getParam (const juce::String& id) const
{
    if (auto* v = apvts.getRawParameterValue (id)) return v->load();
    return 0.0f;
}

juce::String MUProcessor::getParamText (const juce::String& id) const
{
    if (auto* p = apvts.getParameter (id)) return p->getCurrentValueAsText();
    return {};
}

void MUProcessor::setTrimDb (float db) { setParam (ids::trim, juce::jlimit (-24.0f, 24.0f, db)); }

bool MUProcessor::fillBandsDb (std::array<float, kAnalysisBands>& out) const
{
    if (! spectrum.hasLongTerm()) return false;
    const auto& b = spectrum.getLongTermBandsDb();
    for (int i = 0; i < kAnalysisBands; ++i) out[(size_t) i] = b[(size_t) i];
    return true;
}

int MUProcessor::pickBandFor (const EQBandSettings& s) const
{
    auto isFree = [this] (int b) { return getParam (ids::eqBandEnabled (b)) < 0.5f; };
    auto firstFree = [&] (std::initializer_list<int> order) { for (int b : order) if (isFree (b)) return b; return -1; };
    switch (s.type)
    {
        case EQType::LowCut:    return 0;
        case EQType::HighCut:   return kNumEQBands - 1;
        case EQType::LowShelf:  { const int b = firstFree ({ 1, 2, 3 }); return b >= 0 ? b : 1; }
        case EQType::HighShelf: { const int b = firstFree ({ 6, 5, 4 }); return b >= 0 ? b : 6; }
        default: break;
    }
    // A bell goes to an existing bell that is already working the same area, otherwise to a free band.
    for (int b = 1; b < kNumEQBands - 1; ++b)
        if (! isFree (b) && (EQType) (int) getParam (ids::eqBandType (b)) == EQType::Bell
            && std::abs (std::log2 (getParam (ids::eqBandFreq (b)) / juce::jmax (20.0f, s.freqHz))) < 0.2f)
            return b;
    const int freeBand = firstFree ({ 2, 3, 4, 5, 1, 6 });
    if (freeBand >= 0) return freeBand;
    int best = 2; float bestDist = 1.0e9f;
    for (int b = 2; b <= 5; ++b)
    {
        const float d = std::abs (std::log2 (getParam (ids::eqBandFreq (b)) / juce::jmax (20.0f, s.freqHz)));
        if (d < bestDist) { bestDist = d; best = b; }
    }
    return best;
}

void MUProcessor::setBandSettings (int b, const EQBandSettings& s)
{
    setParam (ids::eqBandType (b), (float) s.type);
    setParam (ids::eqBandFreq (b), s.freqHz);
    setParam (ids::eqBandGain (b), s.gainDb);
    setParam (ids::eqBandQ (b), s.q);
    setParam (ids::eqBandDyn (b), s.dynamic ? 1.0f : 0.0f);
    setParam (ids::eqBandThr (b), s.thresholdDb);
    setParam (ids::eqBandRange (b), s.rangeDb);
    setParam (ids::eqBandSlope (b), (float) s.slope);
    setParam (ids::eqBandMode (b), (float) s.mode);
    setParam (ids::eqBandEnabled (b), s.enabled ? 1.0f : 0.0f);
}

void MUProcessor::applyMaskingCut (float hz, float db)
{
    EQBandSettings s;
    s.enabled = true; s.type = EQType::Bell; s.freqHz = hz; s.gainDb = 0.0f; s.q = 1.4f;
    // Masking cuts are dynamic: they only bite while this track is actually loud there.
    s.dynamic = true; s.thresholdDb = -30.0f; s.rangeDb = juce::jlimit (-6.0f, -1.0f, db);
    setBandSettings (pickBandFor (s), s);
    setParam (ids::eqEnabled, 1.0f);
}

// ---- A/B -----------------------------------------------------------------------------------------
std::vector<float> MUProcessor::captureSnapshot() const
{
    std::vector<float> v;
    v.reserve (abParams.size());
    for (auto* p : abParams) v.push_back (p->getValue());
    return v;
}

void MUProcessor::applySnapshot (const std::vector<float>& values)
{
    if (values.size() != abParams.size()) return;
    for (size_t i = 0; i < abParams.size(); ++i)
    {
        auto* p = abParams[i];
        if (std::abs (p->getValue() - values[i]) < 1.0e-6f) continue;
        p->beginChangeGesture();
        p->setValueNotifyingHost (values[i]);
        p->endChangeGesture();
    }
}

void MUProcessor::switchAB()
{
    abStates[(size_t) abSlot] = captureSnapshot();
    const int other = 1 - abSlot;
    if (abStates[(size_t) other].empty()) abStates[(size_t) other] = abStates[(size_t) abSlot];
    applySnapshot (abStates[(size_t) other]);
    abSlot = other;
}

void MUProcessor::copyToOtherAB() { abStates[(size_t) (1 - abSlot)] = captureSnapshot(); }

// ---- Mix Brain -----------------------------------------------------------------------------------
float MUProcessor::secondsHeard() const
{
    const double fs = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    return (float) (spectrum.getLongTermFrames() * (double) SpectrumAnalyzer::kHop / fs);
}

std::vector<Suggestion> MUProcessor::analyse (juce::String& status)
{
    std::vector<Suggestion> out;
    const float heard = secondsHeard();
    if (heard < 3.0f)
    {
        status = heard < 0.2f ? "Nothing has played yet. Start playback on the loudest part of the song, wait a few seconds, then analyse."
                              : "Only " + juce::String (heard, 1) + " s heard so far. Keep the song playing for a few more seconds and analyse again.";
        return out;
    }

    BandArray bands {};
    fillBandsDb (bands);
    const auto role = getEffectiveRole();
    out = analyseTone (bands, role);

    // Clashes with other tracks in the session that involve this one (the master sees all of them).
    const bool master = role == Role::Master;
    int clashes = 0;
    for (auto& c : bus->computeMasking())
    {
        if (! master && c.slotA != slot && c.slotB != slot) continue;
        if (clashes >= 3) break;
        const auto roleA = (Role) bus->getSlot (c.slotA).role.load();
        const auto roleB = (Role) bus->getSlot (c.slotB).role.load();
        out.insert (out.begin() + clashes, describeClash (c, roleA, roleB, c.slotB == slot));
        ++clashes;
    }
    if (out.size() > 6) out.resize (6);

    if (out.empty())
        status = "Nothing stands out. The tone of this track is in the normal range for " + roleNames()[(int) role].toLowerCase() + ", so leave the EQ alone unless your ears say otherwise.";
    else
        status = juce::String ((int) out.size()) + (out.size() == 1 ? " thing" : " things") + " worth a look, most important first. Read why, then apply the ones you agree with.";
    return out;
}

juce::String MUProcessor::applySuggestion (const Suggestion& s)
{
    switch (s.kind)
    {
        case Suggestion::Kind::EqBand:
        {
            const int b = pickBandFor (s.band);
            setBandSettings (b, s.band);
            setParam (ids::eqEnabled, 1.0f);
            spectrum.resetLongTerm();
            return "Band " + juce::String (b + 1) + " set: " + s.action;
        }
        case Suggestion::Kind::DeEsser:
        {
            setParam (ids::dsFreq, juce::jlimit (2000.0f, 12000.0f, s.freqHz));
            setParam (ids::dsRange, juce::jlimit (3.0f, 8.0f, std::abs (s.amountDb)));
            const float level = loudness.getShortTerm();
            if (level > -60.0f) setParam (ids::dsThreshold, juce::jlimit (-60.0f, 0.0f, level - 8.0f));
            setParam (ids::dsOn, 1.0f);
            spectrum.resetLongTerm();
            return "De-esser switched on (Dynamics tab): " + s.action;
        }
        case Suggestion::Kind::Saturation:
        {
            setParam (ids::satType, 2.0f);
            setParam (ids::satDrive, 6.0f);
            setParam (ids::satMix, 35.0f);
            setParam (ids::satOn, 1.0f);
            spectrum.resetLongTerm();
            return "Saturation switched on (Tone tab): " + s.action;
        }
        case Suggestion::Kind::Masking:
        {
            if (s.clash.slotB == slot) applyMaskingCut (s.clash.freqHz, s.clash.suggestedCutDb);
            else bus->applyMaskingClash (s.clash);
            return "Done: " + s.action;
        }
    }
    return {};
}

// ---- AI tutor ------------------------------------------------------------------------------------
juce::String MUProcessor::tutorPrompt (const juce::String& question)
{
    return tutor::corePrompt() + tutor::notesFor (question) + tutorTrackNotes();
}

// What the tutor is told about this track and the session around it. Everything here is measured, not guessed.
juce::String MUProcessor::tutorTrackNotes()
{
    juce::String s ("THIS TRACK\n");
    const auto role = getEffectiveRole();
    s << "Name: \"" << getDisplayName() << "\". Kind of track: " << roleNames()[(int) role] << ". ";
    if (! isPlaying() && loudness.getIntegrated() < -70.0f) s << "Nothing has been played through it yet, so there are no measurements. ";
    else
        s << "Loudness " << juce::String (loudness.getShortTerm(), 1) << " LUFS short-term, " << juce::String (loudness.getIntegrated(), 1) << " LUFS integrated, true peak "
          << juce::String (loudness.getTruePeakHold(), 1) << " dB, crest factor " << juce::String (loudness.getCrestFactorDb(), 1) << " dB. ";

    if (getParam (ids::compEnabled) > 0.5f)
        s << "Compressor on: threshold " << getParamText (ids::compThreshold) << ", ratio " << getParamText (ids::compRatio) << ", attack " << getParamText (ids::compAttack)
          << ", release " << getParamText (ids::compRelease) << ", reducing by " << juce::String (-comp.getGainReductionDb(), 1) << " dB right now. ";
    else s << "Compressor off. ";
    juce::StringArray bands;
    for (int b = 0; b < kNumEQBands; ++b)
    {
        const auto e = getBandSettings (b);
        if (! e.enabled) continue;
        bands.add (eqTypeNames()[(int) e.type] + " at " + juce::String (juce::roundToInt (e.freqHz)) + " Hz"
                   + (e.type == EQType::Bell || e.type == EQType::LowShelf || e.type == EQType::HighShelf ? " " + juce::String (e.dynamic ? e.rangeDb : e.gainDb, 1) + " dB" + (e.dynamic ? " dynamic" : "") : ""));
    }
    s << (bands.isEmpty() ? "EQ flat. " : "EQ bands: " + bands.joinIntoString ("; ") + ". ");
    juce::StringArray on;
    if (getParam (ids::gateOn) > 0.5f) on.add ("gate");
    if (getParam (ids::dsOn) > 0.5f) on.add ("de-esser");
    if (getParam (ids::smOn) > 0.5f) on.add ("Smooth");
    if (getParam (ids::satOn) > 0.5f) on.add ("saturation");
    if (getParam (ids::riderOn) > 0.5f) on.add ("level rider");
    if (getParam (ids::vsEnabled) > 0.5f) on.add ("vocal space");
    if (role == Role::Master && getParam (ids::peakGuard) > 0.5f) on.add ("limiter");
    s << "Also switched on: " << (on.isEmpty() ? juce::String ("nothing") : on.joinIntoString (", ")) << ". ";

    juce::String status;
    const auto found = analyse (status);
    s << "\nMix Brain suggestions right now (EQ tab, Analyse button): ";
    if (secondsHeard() < 3.0f) s << "none yet, because not enough audio has been played. ";
    else if (found.empty()) s << "none. Nothing stands out for this kind of track. ";
    else
    {
        int n = 0;
        for (auto& f : found) s << "(" << ++n << ") " << f.title << ". Its Apply button does this: " << f.action << ". ";
    }
    if (currentPreset.isNotEmpty()) s << "\nPreset loaded: " << currentPreset << ". ";
    if (getTotalNumOutputChannels() > 1 && secondsHeard() >= 3.0f)
    {
        const float c = stereo.getCorrelation();
        s << "Stereo correlation " << juce::String (c, 2) << (c < 0.0f ? " (below 0: parts of this track will vanish in mono). " : c < 0.4f ? " (very wide). " : " (safe in mono). ");
    }

    // the session around it, as the Mix tab sees it
    const auto tracks = bus->collectTracks();
    int playing = 0;
    for (auto& t : tracks) if (t.active) ++playing;
    s << "\n\nTHE WHOLE SESSION (Mix tab)\n";
    if (tracks.size() < 2) s << "This is the only track with the plugin on it, so there is nothing to balance it against yet. ";
    else
    {
        s << juce::String ((int) tracks.size()) << " tracks have the plugin on them, " << playing << " playing now: ";
        int listed = 0;
        for (auto& t : tracks)
        {
            if (listed++ >= 10) break;
            s << "\"" << t.name << "\" (" << roleNames()[(int) t.role] << (t.active ? ", " + juce::String (t.levelLufs, 1) + " LUFS" : juce::String (", silent")) << "); ";
        }
        const auto plan = bus->computePlan (bus->getTargets());
        juce::StringArray moves;
        for (auto& m : plan.moves)
            if (std::abs (m.deltaDb) >= 1.0f)
                moves.add ("\"" + m.name + "\" should come " + (m.deltaDb > 0.0f ? "up " : "down ") + juce::String (std::abs (m.deltaDb), 1) + " dB");
        if (! plan.usable) s << "Balance: not enough has been measured yet; play the loudest section. ";
        else if (moves.isEmpty()) s << "Balance: the levels are already close to where they should sit against the lead vocal. ";
        else s << "Balance (pressing Balance now on the Mix tab does all of this): " << moves.joinIntoString ("; ") << ". ";
        if (plan.hasMaster) s << "The whole mix measures " << juce::String (plan.measuredMasterLufs, 1) << " LUFS with a true peak of " << juce::String (plan.measuredMasterTruePeak, 1) << " dB. ";
        const auto clashes = bus->computeMasking();
        if (clashes.empty()) s << "Collisions: none. ";
        else
        {
            s << "Collisions (each has a Fix button on the Mix tab): ";
            int n = 0;
            for (auto& c : clashes)
            {
                if (n++ >= 4) break;
                s << "\"" << c.nameB << "\" is covering \"" << c.nameA << "\" around " << juce::String (juce::roundToInt (c.freqHz)) << " Hz; ";
            }
        }
    }
    return s;
}

// ---- Reference -----------------------------------------------------------------------------------
juce::String MUProcessor::captureReference()
{
    if (secondsHeard() < 5.0f) return "Play the reference song through this track for at least five seconds first.";
    BandArray b {};
    fillBandsDb (b);
    bus->setReference (b, getDisplayName());
    return "Reference captured from \"" + getDisplayName() + "\". Open the plugin on your own mix and press Match.";
}

bool MUProcessor::hasReference (juce::String& sourceName) const
{
    BandArray b {};
    return bus->getReference (b, sourceName);
}

bool MUProcessor::getReferenceBands (BandArray& out) const
{
    juce::String name;
    return bus->getReference (out, name);
}

juce::String MUProcessor::matchReference (float amount01)
{
    BandArray ref {}; juce::String name;
    if (! bus->getReference (ref, name)) return "No reference yet. Put the plugin on a track playing a finished song you like and press Capture.";
    if (secondsHeard() < 5.0f) return "Play your own mix for at least five seconds first so there is something to compare.";
    BandArray mine {};
    fillBandsDb (mine);
    const auto bands = mu::matchReference (mine, ref, amount01, 5);
    if (bands.empty()) return "Your tone is already within a decibel of the reference. Nothing to change.";
    for (int b = 1; b <= 5; ++b) setParam (ids::eqBandEnabled (b), 0.0f);
    juce::String s;
    int target = 1;
    for (auto& band : bands)
    {
        setBandSettings (target++, band);
        s << (s.isEmpty() ? "" : ", ") << (band.gainDb > 0.0f ? "+" : "") << juce::String (band.gainDb, 1) << " dB at "
          << (band.freqHz >= 1000.0f ? juce::String (band.freqHz / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (band.freqHz)) + " Hz");
    }
    setParam (ids::eqEnabled, 1.0f);
    spectrum.resetLongTerm();
    return "Matched towards \"" + name + "\": " + s + ". These are ordinary bands, so adjust or remove any you do not like.";
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

juce::String MUProcessor::autoCompressor()
{
    const float level = loudness.getShortTerm() > -70.0f ? loudness.getShortTerm() : loudness.getMomentary();
    if (level < -60.0f) return "Play the track first so the compressor can measure it.";
    const auto role = getEffectiveRole();
    float ratio = 2.5f, att = 10.0f, rel = 120.0f, knee = 6.0f, scHp = 20.0f;
    int style = 0;
    switch (role)
    {
        case Role::Vocal:        style = 1; ratio = 4.0f; att = 3.0f;  rel = 60.0f;  knee = 3.0f; break;
        case Role::BackingVocal: style = 2; ratio = 5.0f; att = 5.0f;  rel = 90.0f;  knee = 6.0f; break;
        case Role::Kick:         style = 1; ratio = 4.0f; att = 20.0f; rel = 60.0f;  knee = 4.0f; break;
        case Role::Bass808:      style = 2; ratio = 4.0f; att = 25.0f; rel = 160.0f; knee = 6.0f; break;
        case Role::Drums:        style = 3; ratio = 3.0f; att = 20.0f; rel = 120.0f; knee = 6.0f; scHp = 100.0f; break;
        case Role::Beat:         style = 3; ratio = 2.0f; att = 30.0f; rel = 200.0f; knee = 8.0f; scHp = 120.0f; break;
        case Role::Melody:       style = 2; ratio = 2.5f; att = 20.0f; rel = 150.0f; knee = 8.0f; break;
        case Role::Master:       style = 3; ratio = 1.5f; att = 30.0f; rel = 200.0f; knee = 10.0f; scHp = 100.0f; break;
        default: break;
    }
    setParam (ids::compStyle, (float) style);
    setParam (ids::compRatio, ratio);
    setParam (ids::compAttack, att);
    setParam (ids::compRelease, rel);
    setParam (ids::compKnee, knee);
    setParam (ids::compScHp, scHp);
    setParam (ids::compAutoMakeup, 1.0f);
    setParam (ids::compMix, 100.0f);
    setParam (ids::compEnabled, 1.0f);
    juce::String note;
    fitThresholds (role, note);
    static const char* styles[] = { "Clean", "Punch", "Smooth", "Glue" };
    return "Set for " + roleNames()[(int) role].toLowerCase() + ": " + styles[style] + ", " + juce::String (ratio, 1) + ":1, threshold " + getParamText (ids::compThreshold) + ".";
}

// Places the thresholds relative to how loud the track actually is, so a preset gives the amount of
// gain reduction it was designed for whatever level the track was recorded at.
void MUProcessor::fitThresholds (Role presetRole, juce::String& note)
{
    const float level = loudness.getShortTerm();
    if (level <= -60.0f)
    {
        if (getParam (ids::compEnabled) > 0.5f) note = " Play the track and load the preset again to fit its thresholds to your level.";
        return;
    }
    if (getParam (ids::compEnabled) > 0.5f)
    {
        const auto style = (CompStyle) (int) getParam (ids::compStyle);
        const float ratio = juce::jmax (1.1f, getParam (ids::compRatio));
        const bool vocal = presetRole == Role::Vocal || presetRole == Role::BackingVocal;
        float thr;
        if (style == CompStyle::Smooth || style == CompStyle::Glue)
        {
            // RMS detectors: aim for a typical amount of reduction on the average level.
            const float wanted = style == CompStyle::Glue ? (presetRole == Role::Master ? 1.5f : 2.5f) : (vocal ? 4.0f : 3.0f);
            thr = level + 3.0f - wanted / (1.0f - 1.0f / ratio);
        }
        else
            thr = level + (vocal ? 2.0f : 5.0f);   // peak detectors: the peaks sit well above the average
        setParam (ids::compThreshold, juce::jlimit (-60.0f, 0.0f, thr));
    }
    if (getParam (ids::dsOn) > 0.5f)    setParam (ids::dsThreshold, juce::jlimit (-60.0f, 0.0f, level - 8.0f));
    if (getParam (ids::gateOn) > 0.5f)  setParam (ids::gateThreshold, juce::jlimit (-80.0f, 0.0f, level - 30.0f));
    if (getParam (ids::riderOn) > 0.5f) setParam (ids::riderTarget, juce::jlimit (-40.0f, -6.0f, level - 1.0f));
    for (int b = 0; b < kNumEQBands; ++b)
        if (getParam (ids::eqBandEnabled (b)) > 0.5f && getParam (ids::eqBandDyn (b)) > 0.5f)
        {
            // dynamic bands listen to their own frequency region, which sits below the full-band level
            const float bandLevel = spectrum.hasLongTerm() ? spectrum.longTermBandDbAt (getParam (ids::eqBandFreq (b))) : level - 12.0f;
            setParam (ids::eqBandThr (b), juce::jlimit (-60.0f, 0.0f, bandLevel + 3.0f));
        }
    note = " Thresholds fitted to this track (" + juce::String (level, 1) + " LUFS).";
}

juce::String MUProcessor::applyChainPreset (int index)
{
    const auto& presets = chainPresets();
    if (index < 0 || index >= (int) presets.size()) return {};
    const auto& p = presets[(size_t) index];
    for (auto& pv : p.values) setParam (pv.id, pv.value);
    currentPreset = p.name;
    apvts.state.setProperty ("chainPresetName", currentPreset, nullptr);
    juce::String note;
    fitThresholds (p.role, note);
    spectrum.resetLongTerm();
    if (p.role == Role::Master) { masterTrimDb.store (0.0f); autoLevelHeld.store (false); }
    resetMeasurement();                 // readings taken with the old chain no longer describe this track
    return p.name + " loaded." + note;
}

void MUProcessor::applyMasterMode (int mode)
{
    setParam (ids::masterMode, (float) mode);
    switch (mode)
    {
        case 0: setParam (ids::targetLufs, -18.0f); setParam (ids::targetTruePeak, -6.0f); break;   // hand-off to a mastering engineer
        case 1: setParam (ids::targetLufs, -14.0f); setParam (ids::targetTruePeak, -1.0f); break;   // streaming
        case 2: setParam (ids::targetLufs, -9.0f);  setParam (ids::targetTruePeak, -2.0f); break;   // loud
        default: break;
    }
    if (mode <= 2)
    {
        setParam (ids::peakGuard, 1.0f);
        setParam (ids::masterAutoTrim, 1.0f);
        masterTrimDb.store (0.0f);            // find the level for the new target from scratch
        autoLevelHeld.store (false);
        loudness.resetIntegrated();
        loudness.resetPeakHold();
    }
}

void MUProcessor::resetEQ()
{
    for (int b = 0; b < kNumEQBands; ++b)
    {
        EQBandSettings s;
        s.type = kDefaultBands[b].type; s.freqHz = kDefaultBands[b].freq;
        setBandSettings (b, s);
    }
    setParam (ids::eqAutoGain, 0.0f);
}

void MUProcessor::resetTrim() { setParam (ids::trim, 0.0f); }
void MUProcessor::resetMeters() { resetMeasurement(); }

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
    s.slope = (int) bp.slope->load();
    s.mode = (ChannelMode) (int) bp.mode->load();
    return s;
}

float MUProcessor::eqBandMagnitudeDbAt (int band, float hz) const
{
    const double fs = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    return EQEngine::bandResponseDb (getBandSettings (band), hz, fs);
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
    state.setProperty ("chainPresetName", currentPreset, nullptr);
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
            currentTab.store (juce::jlimit (0, 5, (int) tree.getProperty ("uiTab", 0)));
            currentPreset = tree.getProperty ("chainPresetName", juce::String()).toString();
            abStates[0].clear(); abStates[1].clear(); abSlot = 0;
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
