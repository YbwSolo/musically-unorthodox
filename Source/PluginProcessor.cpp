#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
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
    pTameOn = p (ids::tameOn); pTameAmount = p (ids::tameAmount);
    pMbOn = p (ids::mbOn); pMbCrossLow = p (ids::mbCrossLow); pMbCrossHigh = p (ids::mbCrossHigh);
    pClipOn = p (ids::clipOn); pClipAmount = p (ids::clipAmount); pClipSoft = p (ids::clipSoft);
    for (int b = 0; b < 3; ++b)
    {
        pMbBandOn[(size_t) b] = p (ids::mbBandOn (b)); pMbThr[(size_t) b] = p (ids::mbThreshold (b)); pMbRatio[(size_t) b] = p (ids::mbRatio (b));
        pMbAtt[(size_t) b] = p (ids::mbAttack (b)); pMbRel[(size_t) b] = p (ids::mbRelease (b)); pMbGain[(size_t) b] = p (ids::mbMakeup (b));
    }
    pVsOn = p (ids::vsEnabled); pVsAmount = p (ids::vsAmount); pVsSpeed = p (ids::vsSpeed); pVsCentre = p (ids::vsCentre);
    for (int i = 0; i < 3; ++i) { pVsFreq[(size_t) i] = p (ids::vsFreq (i)); pVsCut[(size_t) i] = p (ids::vsCut (i)); pVsQ[(size_t) i] = p (ids::vsQ (i)); }
    pStWidth = p (ids::stWidth); pStMonoBass = p (ids::stMonoBass); pStMonoFreq = p (ids::stMonoFreq);
    pTuneOn = p (ids::tuneOn); pTuneKey = p (ids::tuneKey); pTuneScale = p (ids::tuneScale); pTuneSpeed = p (ids::tuneSpeed); pTuneAmount = p (ids::tuneAmount); pTuneVibrato = p (ids::tuneVibrato);
    pDblOn = p (ids::dblOn); pDblMix = p (ids::dblMix); pDblDetune = p (ids::dblDetune); pDblTiming = p (ids::dblTiming); pDblWidth = p (ids::dblWidth); pDblVoices = p (ids::dblVoices);
    pRvOn = p (ids::rvOn); pRvMix = p (ids::rvMix); pRvAuto = p (ids::rvAuto); pRvLength = p (ids::rvLength); pRvDecay = p (ids::rvDecay); pRvPre = p (ids::rvPre); pRvTone = p (ids::rvTone); pRvDuck = p (ids::rvDuck);
    pRvType = p (ids::rvType); pRvLowCut = p (ids::rvLowCut); pRvWidth = p (ids::rvWidth);
    pTuneMode = p (ids::tuneMode); pTuneHold = p (ids::tuneHold); pTuneFormant = p (ids::tuneFormant); pTuneNotes = p (ids::tuneNotes); pTrackGuard = p (ids::trackGuard);
    pDlOn = p (ids::dlOn); pDlMix = p (ids::dlMix); pDlSync = p (ids::dlSync); pDlNote = p (ids::dlNote); pDlTime = p (ids::dlTime); pDlFeedback = p (ids::dlFeedback);
    pDlTone = p (ids::dlTone); pDlWidth = p (ids::dlWidth); pDlDuck = p (ids::dlDuck);
    pHmOn = p (ids::hmOn); pHmVoicing = p (ids::hmVoicing); pHmMix = p (ids::hmMix); pHmWidth = p (ids::hmWidth);
    pTuneFlex = p (ids::tuneFlex); pTuneHumanise = p (ids::tuneHumanise); pTuneSlides = p (ids::tuneSlides); pHmHumanise = p (ids::hmHumanise);
    pRvSize = p (ids::rvSize); pRvDiffusion = p (ids::rvDiffusion); pRvModDepth = p (ids::rvModDepth); pRvModRate = p (ids::rvModRate);
    pRvLowDecay = p (ids::rvLowDecay); pRvEarly = p (ids::rvEarly); pRvFreeze = p (ids::rvFreeze); pRvShimmer = p (ids::rvShimmer);
    pDlStyle = p (ids::dlStyle); pDlPattern = p (ids::dlPattern); pDlDrive = p (ids::dlDrive); pDlWobble = p (ids::dlWobble); pDlLowCut = p (ids::dlLowCut);
    pFxOrder = p (ids::fxOrder);
    pPsMode = p (ids::psMode); pPsFine = p (ids::psFine); pPsDrive = p (ids::psDrive); pPsWidth = p (ids::psWidth);
    pHmMode = p (ids::hmMode); pHmLeading = p (ids::hmLeading); pHmChoir = p (ids::hmChoir); pHmChordCount = p (ids::hmChordCount); pHmChordLength = p (ids::hmChordLength);
    for (int v = 0; v < 4; ++v)
    {
        pHmVOn[(size_t) v] = p (ids::hmVoiceOn (v)); pHmVSteps[(size_t) v] = p (ids::hmVoiceSteps (v)); pHmVLevel[(size_t) v] = p (ids::hmVoiceLevel (v)); pHmVPan[(size_t) v] = p (ids::hmVoicePan (v));
        pHmVDelay[(size_t) v] = p (ids::hmVoiceDelay (v)); pHmVSize[(size_t) v] = p (ids::hmVoiceSize (v)); pHmVDetune[(size_t) v] = p (ids::hmVoiceDetune (v));
    }
    for (int slot = 0; slot < 16; ++slot) pHmChord[(size_t) slot] = p (ids::hmChord (slot));
    pPsOn = p (ids::psOn); pPsSemis = p (ids::psSemis); pPsCharacter = p (ids::psCharacter); pPsMix = p (ids::psMix);
    pPhOn = p (ids::phOn); pPhAmount = p (ids::phAmount); pPhTone = p (ids::phTone); pPhDrive = p (ids::phDrive);
    pLfOn = p (ids::lfOn); pLfCrush = p (ids::lfCrush); pLfWobble = p (ids::lfWobble); pLfMix = p (ids::lfMix);
    pMdOn = p (ids::mdOn); pMdType = p (ids::mdType); pMdRate = p (ids::mdRate); pMdDepth = p (ids::mdDepth); pMdMix = p (ids::mdMix);
    pChOn = p (ids::chOn); pChRate = p (ids::chRate); pChDepth = p (ids::chDepth); pChShape = p (ids::chShape);
    pPhMode = p (ids::phMode); pPhNoise = p (ids::phNoise);
    pLfDrive = p (ids::lfDrive); pLfNoise = p (ids::lfNoise); pLfNoiseType = p (ids::lfNoiseType); pLfWear = p (ids::lfWear); pLfTone = p (ids::lfTone);
    pMdShape = p (ids::mdShape); pMdSmooth = p (ids::mdSmooth); pMdSync = p (ids::mdSync); pMdNote = p (ids::mdNote);
    pChMode = p (ids::chMode); pChLength = p (ids::chLength); pChGate = p (ids::chGate); pChSwing = p (ids::chSwing);
    for (int i = 0; i < 16; ++i) { pMdPoint[(size_t) i] = p (ids::mdPoint (i)); pChStep[(size_t) i] = p (ids::chStep (i)); }
    tuner.setGraph (&graph);

    // A/B covers everything that shapes the sound; routing and session balance stay put.
    for (auto* param : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            const auto id = ranged->getParameterID();
            if (id == ids::role || id == ids::bypass || id == ids::trim || id == ids::autoBalance || id.startsWith ("roleOff_")) continue;
            if (id == ids::tuneKey || id == ids::tuneScale || id == ids::tuneNotes || id == ids::tuneMode) continue;      // the song's key is not an A/B choice
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
    // mono in, stereo out lets a mono vocal track have wide doubles and reverb
    if (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo()) return true;
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void MUProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int ch = juce::jmax (1, getTotalNumOutputChannels());
    loudness.prepare (sampleRate, samplesPerBlock, ch);
    mixMeter.prepare (sampleRate, samplesPerBlock, ch);
    inMeter.prepare (sampleRate, samplesPerBlock, ch);
    outMeter.prepare (sampleRate, samplesPerBlock, ch);
    inProbe.prepare (sampleRate, ch); outProbe.prepare (sampleRate, ch);
    multiband.prepare (sampleRate, samplesPerBlock, ch);
    clipper.prepare (sampleRate, samplesPerBlock, ch);
    for (auto& d : dryRing) d.assign (1 << 15, 0.0f);
    dryPos = 0;
    compareGain.reset (sampleRate, 0.03); compareGain.setCurrentAndTargetValue (1.0f);
    spectrum.prepare (sampleRate);
    preSpectrum.prepare (sampleRate);
    gate.prepare (sampleRate, samplesPerBlock, ch);
    eq.prepare (sampleRate, samplesPerBlock, ch);
    comp.prepare (sampleRate, samplesPerBlock, ch);
    deEsser.prepare (sampleRate, samplesPerBlock, ch);
    smoother.prepare (sampleRate, samplesPerBlock, ch);
    saturator.prepare (sampleRate, samplesPerBlock, ch);
    rider.prepare (sampleRate, samplesPerBlock, ch);
    tamer.prepare (sampleRate);
    vocalProbe.prepare (sampleRate);
    vocalSpace.prepare (sampleRate, samplesPerBlock, ch);
    stereo.prepare (sampleRate, samplesPerBlock, ch);
    maximizer.prepare (sampleRate, samplesPerBlock, ch);
    tuner.prepare (sampleRate, samplesPerBlock, ch);
    doubler.prepare (sampleRate, samplesPerBlock, ch);
    reverb.prepare (sampleRate, samplesPerBlock, ch);
    echo.prepare (sampleRate, samplesPerBlock, ch);
    pitchFx.prepare (sampleRate, samplesPerBlock, ch);
    phone.prepare (sampleRate, ch);
    lofi.prepare (sampleRate, ch);
    motion.prepare (sampleRate, ch);
    chop.prepare (sampleRate);
    guard.prepare (sampleRate);
    keyFinder.prepare (sampleRate);
    // the phone-speaker check: nothing below 350 Hz, little above 5 kHz
    checkHp1.highPass (350.0f, 0.5412f, sampleRate); checkHp2.highPass (350.0f, 1.3066f, sampleRate); checkLp.lowPass (5000.0f, 0.707f, sampleRate);
    checkHp1.clear(); checkHp2.clear(); checkLp.clear();
    deltaBuffer.setSize (ch, juce::jmax (64, samplesPerBlock), false, true, false);
    cleanBuffer.setSize (ch, juce::jmax (64, samplesPerBlock), false, true, false);
    trimGain.reset (sampleRate, 0.08);
    outGain.reset (sampleRate, 0.03);
    masterTrimGain.reset (sampleRate, 0.5);
    trimGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pTrim->load()));
    inputProbe.prepare (sampleRate, getTotalNumOutputChannels());
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
    // Tune adds 20 ms while it is on. In low latency mode the look-ahead stages are out of the path.
    const bool lowLatency = bus->isLowLatency();
    const int latency = saturator.getLatencySamples() + tuner.getLatencySamples() + (isMasterInstance() && ! lowLatency ? maximizer.getLatencySamples() + clipper.getLatencySamples() : 0);
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
    tamer.setEnabled (pTameOn->load() > 0.5f && ! isMasterInstance());
    tamer.setAmount (pTameAmount->load() * 0.01f);

    VocalSpace::Settings v;
    const auto role = getEffectiveRole();
    const bool vsAllowed = role != Role::Vocal && role != Role::BackingVocal && role != Role::Master;
    v.enabled = vsAllowed && pVsOn->load() > 0.5f;
    v.amount = pVsAmount->load() * 0.01f;
    v.speed = (int) pVsSpeed->load();
    v.centreOnly = pVsCentre->load() > 0.5f;
    for (int i = 0; i < 3; ++i) { v.freqHz[(size_t) i] = pVsFreq[(size_t) i]->load(); v.cutDb[(size_t) i] = pVsCut[(size_t) i]->load(); v.q[(size_t) i] = pVsQ[(size_t) i]->load(); }
    vocalSpace.setSettings (v);

    StereoSettings st;
    st.width = pStWidth->load() * 0.01f; st.monoBass = pStMonoBass->load() > 0.5f; st.monoBelowHz = pStMonoFreq->load();
    stereo.setSettings (st);

    const bool lowLatency = bus->isLowLatency();
    TunerSettings tn;
    tn.enabled = pTuneOn->load() > 0.5f; tn.key = (int) pTuneKey->load(); tn.scale = (int) pTuneScale->load(); tn.speedMs = pTuneSpeed->load();
    tn.amount = pTuneAmount->load() * 0.01f; tn.keepVibrato = pTuneVibrato->load() * 0.01f; tn.lowLatency = lowLatency;
    tn.mode = (int) pTuneMode->load(); tn.hold = pTuneHold->load() * 0.01f; tn.formantSemis = pTuneFormant->load(); tn.noteMask = (int) pTuneNotes->load();
    tn.flexCents = pTuneFlex->load(); tn.humanise = pTuneHumanise->load() * 0.01f; tn.keepSlides = pTuneSlides->load() * 0.01f;
    tuner.setSettings (tn);

    DoublerSettings db;
    db.enabled = pDblOn->load() > 0.5f; db.mix = pDblMix->load() * 0.01f; db.detuneCents = pDblDetune->load(); db.timing = pDblTiming->load() * 0.01f;
    db.width = pDblWidth->load() * 0.01f; db.voices = (int) pDblVoices->load();
    doubler.setSettings (db);

    ReverbSettings rv;
    rv.enabled = pRvOn->load() > 0.5f; rv.mix = pRvMix->load() * 0.01f; rv.autoTime = pRvAuto->load() > 0.5f; rv.length = (int) pRvLength->load();
    rv.decaySeconds = pRvDecay->load(); rv.preDelayMs = pRvPre->load(); rv.tone = pRvTone->load() * 0.01f; rv.duck = pRvDuck->load() * 0.01f;
    rv.type = (int) pRvType->load(); rv.lowCutHz = pRvLowCut->load(); rv.width = pRvWidth->load() * 0.01f;
    rv.size = pRvSize->load() * 0.01f; rv.diffusion = pRvDiffusion->load() * 0.01f; rv.modDepth = pRvModDepth->load() * 0.01f; rv.modRate = pRvModRate->load();
    rv.lowDecay = pRvLowDecay->load() * 0.01f; rv.early = pRvEarly->load() * 0.01f; rv.freeze = pRvFreeze->load() > 0.5f; rv.shimmer = pRvShimmer->load() * 0.01f;
    rv.bpm = hostBpm.load (std::memory_order_relaxed);
    reverb.setSettings (rv);

    EchoSettings dl;
    dl.enabled = pDlOn->load() > 0.5f; dl.mix = pDlMix->load() * 0.01f; dl.sync = pDlSync->load() > 0.5f; dl.note = (int) pDlNote->load(); dl.timeMs = pDlTime->load();
    dl.feedback = pDlFeedback->load() * 0.01f; dl.tone = pDlTone->load() * 0.01f; dl.width = pDlWidth->load() * 0.01f; dl.duck = pDlDuck->load() * 0.01f; dl.bpm = rv.bpm;
    dl.style = (int) pDlStyle->load(); dl.pattern = (int) pDlPattern->load(); dl.drive = pDlDrive->load() * 0.01f; dl.wobble = pDlWobble->load() * 0.01f; dl.lowCutHz = pDlLowCut->load();
    echo.setSettings (dl);

    PitchFXSettings pf;
    pf.shiftOn = pPsOn->load() > 0.5f; pf.semitones = pPsSemis->load(); pf.character = pPsCharacter->load() * 0.01f; pf.shiftMix = pPsMix->load() * 0.01f;
    pf.harmonyOn = pHmOn->load() > 0.5f; pf.voicing = (int) pHmVoicing->load(); pf.harmonyMix = pHmMix->load() * 0.01f; pf.harmonyWidth = pHmWidth->load() * 0.01f;
    pf.key = tn.key; pf.scale = tn.scale; pf.noteMask = tn.noteMask; pf.humanise = pHmHumanise->load() * 0.01f;
    pf.shiftMode = (int) pPsMode->load(); pf.fineCents = pPsFine->load(); pf.shiftDrive = pPsDrive->load() * 0.01f; pf.shiftWidth = pPsWidth->load() * 0.01f;
    pf.harmonyMode = (int) pHmMode->load(); pf.leading = pHmLeading->load() > 0.5f; pf.choir = pHmChoir->load() * 0.01f;
    for (int v = 0; v < 4; ++v)
    {
        auto& voice = pf.voice[(size_t) v];
        voice.on = pHmVOn[(size_t) v]->load() > 0.5f; voice.steps = juce::roundToInt (pHmVSteps[(size_t) v]->load()); voice.level = pHmVLevel[(size_t) v]->load() * 0.01f;
        voice.pan = pHmVPan[(size_t) v]->load() * 0.01f; voice.delayMs = pHmVDelay[(size_t) v]->load(); voice.size = pHmVSize[(size_t) v]->load() * 0.01f; voice.detuneCents = pHmVDetune[(size_t) v]->load();
    }
    pitchFx.setSettings (pf);

    PhoneSettings ph;
    ph.enabled = pPhOn->load() > 0.5f; ph.amount = pPhAmount->load() * 0.01f; ph.tone = pPhTone->load() * 0.01f; ph.drive = pPhDrive->load() * 0.01f;
    ph.mode = (int) pPhMode->load(); ph.noise = pPhNoise->load() * 0.01f;
    phone.setSettings (ph);
    LoFiSettings lf;
    lf.enabled = pLfOn->load() > 0.5f; lf.crush = pLfCrush->load() * 0.01f; lf.wobble = pLfWobble->load() * 0.01f; lf.mix = pLfMix->load() * 0.01f;
    lf.drive = pLfDrive->load() * 0.01f; lf.noise = pLfNoise->load() * 0.01f; lf.noiseType = (int) pLfNoiseType->load(); lf.wear = pLfWear->load() * 0.01f; lf.tone = pLfTone->load() * 0.01f;
    lofi.setSettings (lf);
    MotionSettings md;
    md.enabled = pMdOn->load() > 0.5f; md.type = (int) pMdType->load(); md.rateHz = pMdRate->load(); md.depth = pMdDepth->load() * 0.01f; md.mix = pMdMix->load() * 0.01f;
    md.shape = (int) pMdShape->load(); md.smooth = pMdSmooth->load() * 0.01f; md.sync = pMdSync->load() > 0.5f; md.note = (int) pMdNote->load(); md.bpm = rv.bpm;
    for (int i = 0; i < 16; ++i) md.curve[(size_t) i] = pMdPoint[(size_t) i]->load() * 0.01f;
    motion.setSettings (md);
    ChopSettings cs;
    cs.enabled = pChOn->load() > 0.5f; cs.rate = (int) pChRate->load(); cs.depth = pChDepth->load() * 0.01f; cs.shape = pChShape->load() * 0.01f; cs.bpm = rv.bpm;
    cs.mode = (int) pChMode->load(); cs.length = juce::roundToInt (pChLength->load()); cs.gate = pChGate->load() * 0.01f; cs.swing = pChSwing->load() * 0.01f;
    for (int i = 0; i < 16; ++i) cs.steps[(size_t) i] = pChStep[(size_t) i]->load() * 0.01f;
    chop.setSettings (cs);

    guard.setEnabled (role != Role::Master && pTrackGuard->load() > 0.5f);

    maximizer.setActive (role == Role::Master && ! lowLatency);
    maximizer.setEnabled (role == Role::Master && ! lowLatency && pPeakGuard->load() > 0.5f);
    maximizer.setGainDb (pMaxGain->load());
    {
        // the master's own stages: three-band compression after the EQ, a soft clipper in front of the limiter
        MultibandSettings mb;
        mb.enabled = role == Role::Master && pMbOn->load() > 0.5f;
        mb.crossLowHz = pMbCrossLow->load(); mb.crossHighHz = pMbCrossHigh->load();
        for (int b = 0; b < 3; ++b)
        {
            auto& band = mb.band[(size_t) b];
            band.on = pMbBandOn[(size_t) b]->load() > 0.5f; band.thresholdDb = pMbThr[(size_t) b]->load(); band.ratio = pMbRatio[(size_t) b]->load();
            band.attackMs = pMbAtt[(size_t) b]->load(); band.releaseMs = pMbRel[(size_t) b]->load(); band.makeupDb = pMbGain[(size_t) b]->load();
        }
        multiband.setSettings (mb);
        clipper.setActive (role == Role::Master && ! lowLatency);
        clipper.setEnabled (role == Role::Master && ! lowLatency && pClipOn->load() > 0.5f);
        // Amount: at 0 % the clipper starts 6 dB above the limiter's ceiling (it rarely touches anything); at 100 % it
        // starts at the ceiling, so all the peak control is rounding and the limiter has almost nothing left to do
        clipper.setThresholdDb (pTargetTp->load() + 6.0f * (1.0f - pClipAmount->load() * 0.01f));
        clipper.setSoftness (pClipSoft->load() * 0.01f);
    }
    maximizer.setCeilingDb (pTargetTp->load());
}

void MUProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
    {
        if (getTotalNumInputChannels() == 1) buffer.copyFrom (c, 0, buffer, 0, 0, n);      // mono in, stereo out: start from the same sound on both sides
        else buffer.clear (c, 0, n);
    }
    if (n == 0) return;

    // the song's tempo and where the song is, for everything that keeps time with it
    bool playing = false, haveTime = false, haveBeats = false;
    double seconds = 0.0, quarterNotes = 0.0;
    if (auto* head = getPlayHead())
        if (auto position = head->getPosition())
        {
            if (auto bpm = position->getBpm())
                if (*bpm > 20.0) { hostBpm.store (*bpm, std::memory_order_relaxed); hostTempoKnown.store (true, std::memory_order_relaxed); }
            playing = position->getIsPlaying();
            if (auto t = position->getTimeInSeconds()) { seconds = *t; haveTime = true; }
            else if (auto samples = position->getTimeInSamples()) { seconds = (double) *samples / juce::jmax (1.0, getSampleRate()); haveTime = true; }
            if (auto q = position->getPpqPosition()) { quarterNotes = *q; haveBeats = true; }
        }
    hostPlaying.store (playing && haveTime, std::memory_order_relaxed);
    if (haveTime) hostSeconds.store (seconds, std::memory_order_relaxed);
    tuner.setTimeline (playing && haveTime, seconds);
    chop.setTimeline (playing && haveBeats, quarterNotes);
    {
        // Harmony in Chords mode: the chord for this moment. Keys held on a MIDI keyboard come first (where the
        // host sends MIDI to an effect); otherwise it is the chord the song has reached in the grid.
        for (const auto meta : midi)
        {
            const auto message = meta.getMessage();
            if (message.isNoteOn()) midiHeld[(size_t) message.getNoteNumber()] = 1;
            else if (message.isNoteOff()) midiHeld[(size_t) message.getNoteNumber()] = 0;
            else if (message.isAllNotesOff() || message.isAllSoundOff()) midiHeld.fill (0);
        }
        int held = 0;
        for (int note = 0; note < 128; ++note) if (midiHeld[(size_t) note] != 0) held |= 1 << (note % 12);
        midiChordNow.store (held, std::memory_order_relaxed);
        static const double slotBeats[4] = { 8.0, 4.0, 2.0, 1.0 };
        const int count = juce::jlimit (1, 16, juce::roundToInt (pHmChordCount->load()));
        int slot = chordSlotNow.load (std::memory_order_relaxed);
        if (playing && haveBeats)
        {
            slot = (int) std::floor (quarterNotes / slotBeats[juce::jlimit (0, 3, (int) pHmChordLength->load())]) % count;
            if (slot < 0) slot += count;
        }
        slot = juce::jlimit (0, count - 1, slot);
        chordSlotNow.store (slot, std::memory_order_relaxed);
        pitchFx.setChord (held != 0 ? held : juce::roundToInt (pHmChord[(size_t) slot]->load()) & 0xFFF);
    }
    motion.setTimeline (playing && haveBeats, quarterNotes);

    pullParameters();
    const bool bypassed = pBypass->load() > 0.5f;
    const bool master = getEffectiveRole() == Role::Master;
    if (editorsOpen.load (std::memory_order_relaxed) > 0 || inProbe.isRunning()) preSpectrum.pushBlock (buffer);
    if (master)
    {
        // the mix as it arrives: measured while a mastering pass is listening, and kept for the comparison
        if (inProbe.isRunning()) { inMeter.process (buffer); inProbe.process (buffer, inMeter.getShortTerm(), inMeter.getMomentary()); }
        const int size = (int) dryRing[0].size();
        for (int c = 0; c < juce::jmin (2, numCh); ++c)
        {
            const float* in = buffer.getReadPointer (c); auto& ring = dryRing[(size_t) c];
            int pos = dryPos;
            for (int i = 0; i < n; ++i) { ring[(size_t) pos] = in[i]; pos = pos + 1 == size ? 0 : pos + 1; }
        }
        dryPos = (dryPos + n) % size;
    }
    keyFinder.push (buffer.getReadPointer (0), numCh > 1 ? buffer.getReadPointer (1) : nullptr, n);

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

        // The balance trim is this track's fader: it comes after all the processing, so balancing the song never
        // changes how hard the compressor, de-esser, gate or saturation are being driven. (Songs saved by earlier
        // builds had it in front; they keep that until they are next balanced, so they open sounding the same.)
        trimGain.setTargetValue (juce::Decibels::decibelsToGain (pTrim->load()));
        const bool trimFirst = trimBeforeChain.load (std::memory_order_relaxed);
        if (trimFirst) applySmoothedGain (trimGain);
        inputProbe.process (buffer);
        tuner.process (buffer);
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
            multiband.process (buffer);                  // master only; everywhere else it is out of the path
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
            tamer.process (buffer);                      // the loudest syllables, after the level has been evened out
            vocalSpace.setKeyLevel (bus->getVocalKeyLevel());
            vocalSpace.process (buffer);
            // The doubler and the harmony and shift voices all need to hear the clean, single voice to follow its
            // pitch, so they work side by side on the same sound and their results are added together.
            const bool pitchVoices = pPsOn->load() > 0.5f || pHmOn->load() > 0.5f;
            if (pitchVoices && n <= cleanBuffer.getNumSamples() && numCh <= cleanBuffer.getNumChannels())
            {
                for (int c = 0; c < numCh; ++c) cleanBuffer.copyFrom (c, 0, buffer, c, 0, n);
                doubler.process (buffer);                                                   // clean + doubles
                for (int c = 0; c < numCh; ++c) buffer.addFrom (c, 0, cleanBuffer, c, 0, n, -1.0f);     // doubles only
                juce::AudioBuffer<float> voices (cleanBuffer.getArrayOfWritePointers(), numCh, n);
                pitchFx.process (voices);                                                   // the voice (shifted or not) + harmonies
                for (int c = 0; c < numCh; ++c) buffer.addFrom (c, 0, cleanBuffer, c, 0, n);
            }
            else
            {
                pitchFx.process (buffer);       // off: lets it reset
                doubler.process (buffer);
            }
            // The six in-line tools, in the order chosen on the FX tab. A change of order takes one block to fade
            // the track out and the next to fade it back in, with the swap in the silence between.
            const int wantOrder = juce::jlimit (0, fxorder::kOrders - 1, juce::roundToInt (pFxOrder->load()));
            for (int tool : fxOrderList)
                switch (tool)
                {
                    case fxorder::Phone:  phone.process (buffer); break;
                    case fxorder::Lofi:   lofi.process (buffer); break;
                    case fxorder::Motion: motion.process (buffer); break;
                    case fxorder::Chop:   chop.process (buffer); break;
                    case fxorder::Delay:  echo.process (buffer); break;
                    case fxorder::Reverb: reverb.process (buffer); break;
                    default: break;
                }
            if (wantOrder != fxOrderNow || fxOrderGain < 1.0f)
            {
                const float step = (float) n / (float) (0.008 * juce::jmax (8000.0, getSampleRate()));
                const float to = wantOrder != fxOrderNow ? juce::jmax (0.0f, fxOrderGain - step) : juce::jmin (1.0f, fxOrderGain + step);
                buffer.applyGainRamp (0, n, fxOrderGain, to);
                fxOrderGain = to;
                if (wantOrder != fxOrderNow && fxOrderGain <= 0.0f) { fxOrderNow = wantOrder; fxOrderList = fxorder::decode (wantOrder); }
            }
            stereo.process (buffer);
            vocalProbe.process (buffer);                 // only while Finish is listening
            outGain.setTargetValue (juce::Decibels::decibelsToGain (pOut->load()));
            applySmoothedGain (outGain);
        }

        if (! trimFirst) applySmoothedGain (trimGain);
        guard.process (buffer);                        // tracks only: catches the odd peak before it leaves
        if (master) mixMeter.process (buffer);         // the mix as it arrives, before any mastering gain
        masterTrimGain.setTargetValue (juce::Decibels::decibelsToGain (master && ! bus->isLowLatency() ? masterTrimDb.load() : 0.0f));
        applySmoothedGain (masterTrimGain);
        clipper.process (buffer);
        maximizer.process (buffer);
    }
    else
    {
        // keep the latency-bearing stages running so bypass does not shift timing
        tuner.process (buffer, true);
        saturator.setSettings ({});
        saturator.process (buffer);
        if (master) mixMeter.process (buffer);
        clipper.setEnabled (false);
        clipper.process (buffer);
        maximizer.setEnabled (false);
        maximizer.process (buffer);
    }

    loudness.process (buffer);
    spectrum.pushBlock (buffer);
    if (master && outProbe.isRunning()) { outMeter.process (buffer); outProbe.process (buffer, outMeter.getShortTerm(), outMeter.getMomentary()); }
    if (master && gateOn.load (std::memory_order_relaxed) && inProbe.isRunning() && outProbe.isRunning())
    {
        const float in = inMeter.getMomentary(), out = outMeter.getMomentary();
        if (in > gateFloorLufs.load (std::memory_order_relaxed) && out > -70.0f)
        {
            const double dt = (double) buffer.getNumSamples() / juce::jmax (1.0, getSampleRate());
            gateIn.store (gateIn.load (std::memory_order_relaxed) + dt * std::pow (10.0, 0.1 * (double) in), std::memory_order_relaxed);
            gateOut.store (gateOut.load (std::memory_order_relaxed) + dt * std::pow (10.0, 0.1 * (double) out), std::memory_order_relaxed);
            gateSeconds.store (gateSeconds.load (std::memory_order_relaxed) + dt, std::memory_order_relaxed);
        }
    }
    samplesProcessed.fetch_add ((uint64_t) n, std::memory_order_relaxed);
    // The session balance works on the mix itself, so the master reports its level before the mastering stage.
    const auto& shared = master ? mixMeter : loudness;
    bus->publish (slot, shared.getMomentary(), shared.getShortTerm(), shared.getIntegrated(),
                  shared.getTruePeakHold(), shared.getCrestFactorDb(), shared.isActive(), getEffectiveRole());

    // Listening check, master only, after everything has been measured: hear the mix as it would sound in mono
    // or on a phone speaker. Never during a bounce, so it cannot end up in the exported song.
    const int check = master && ! isNonRealtime() ? listenCheck.load (std::memory_order_relaxed) : 0;
    // Level-matched comparison, master only and never during a bounce: the mix as it arrived (late by the same
    // amount as the master, so switching does not jump in time), or the master turned down to the mix's loudness.
    {
        const int compare = master && ! isNonRealtime() ? masterCompare.load (std::memory_order_relaxed) : 0;
        if (compare == 2)
        {
            const int size = (int) dryRing[0].size(), late = juce::jlimit (0, size - n - 1, juce::jmax (0, reportedLatency));
            for (int c = 0; c < juce::jmin (2, numCh); ++c)
            {
                auto* out = buffer.getWritePointer (c); const auto& ring = dryRing[(size_t) c];
                int pos = (dryPos - n - late + 2 * size) % size;
                for (int i = 0; i < n; ++i) { out[i] = ring[(size_t) pos]; pos = pos + 1 == size ? 0 : pos + 1; }
            }
        }
        compareGain.setTargetValue (compare == 1 ? juce::Decibels::decibelsToGain (compareGainDb.load (std::memory_order_relaxed)) : 1.0f);
        if (compare != 2) applySmoothedGain (compareGain);
    }
    if (check != checkWas) { checkWas = check; checkHp1.clear(); checkHp2.clear(); checkLp.clear(); }
    if (check != 0 && numCh > 1)
    {
        auto* l = buffer.getWritePointer (0); auto* r = buffer.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            float m = 0.5f * (l[i] + r[i]);
            if (check == (int) ListenCheck::Phone) m = checkLp.run (0, checkHp2.run (0, checkHp1.run (0, m)));
            l[i] = r[i] = m;
        }
    }
}

void MUProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (buffer.getNumSamples() == 0) return;
    maximizer.setActive (getEffectiveRole() == Role::Master);
    maximizer.setEnabled (false);
    clipper.setActive (getEffectiveRole() == Role::Master);
    clipper.setEnabled (false);
    tuner.process (buffer, true);
    saturator.setSettings ({});
    saturator.process (buffer);
    clipper.process (buffer);
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
    masterTick();
    finishTick();
    noteEqForMeasurement();
    updateLatency();
    lufsHistory.push (loudness.getShortTerm());

    RoleFeatures f;
    f.valid = spectrum.hasLongTerm() && loudness.isActive();
    f.lowEnergyRatio = spectrum.lowEnergyRatio();
    f.highEnergyRatio = spectrum.highEnergyRatio();
    f.centroidHz = spectrum.centroidHz();
    f.crestFactorDb = loudness.getCrestFactorDb();
    f.mono = getTotalNumInputChannels() == 1;
    f.voiceTools = pTuneOn->load() > 0.5f || pHmOn->load() > 0.5f;
    Role named;
    if (f.valid || roleFromName (trackName, named))
    {
        detected = guessRole (f);
        detectedRoleAtomic.store ((int) detected.role);
    }

    // a capture ends by itself when the song stops
    if (graph.isCapturing() && captureSeen && ! hostPlaying.load()) stopCapture();
    if (graph.isCapturing() && hostPlaying.load()) captureSeen = true;

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
                // A level set by Analyse and master came from the whole song. It is not moved by whichever passage
                // happens to be playing (a quiet verse would push it up, and the chorus would then come out too
                // loud); only the guard against crushing the mix still acts. Changing the target by hand hands
                // the level back to the steering below.
                if (masterLevelSet.load() && std::abs (pTargetLufs->load() - masterLevelTarget.load()) > 0.05f) masterLevelSet.store (false);
                if (masterLevelSet.load())
                {
                    if (worst < -8.0f) masterTrimDb.store (juce::jlimit (-24.0f, 24.0f, masterTrimDb.load() + juce::jmax (-3.0f, worst + 8.0f)));
                    lastAutoTrimStep = now;
                    return;
                }
                // Once it has stopped pushing it stays stopped until the limiter has clearly eased off (under 4.5 dB),
                // so the level does not creep up and down as louder and quieter passages go by.
                bool held = autoLevelHeld.load();
                if (error > 0.0f)
                {
                    if (worst < -6.0f) held = true;
                    else if (worst > -4.5f) held = false;
                    if (held) error = worst < -8.0f ? juce::jmax (-3.0f, worst + 8.0f) : 0.0f;
                }
                else held = false;
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
            { masterTrimDb.store (0.0f); masterLevelSet.store (false); }
    }
    else if (std::abs (masterTrimDb.load()) > 0.001f)
        { masterTrimDb.store (0.0f); masterLevelSet.store (false); }
}

// The name of the track says more than its sound does: someone who names a track "rap" or "808" has told us.
RoleGuess MUProcessor::guessRole (const RoleFeatures& f) const
{
    Role named;
    if (roleFromName (trackName, named))
    {
        // only the song's output can be the master, and a mono track cannot be a stereo beat
        if (! (named == Role::Beat && f.mono)) return { named, 0.9f, "from the track's name" };
    }
    return detectRole (f);
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

void MUProcessor::setTrimDb (float db)
{
    if (trimBeforeChain.load()) moveTrimAfterChain();
    setParam (ids::trim, juce::jlimit (-24.0f, 24.0f, db));
}

// A song from an earlier build has its trim in front of the processing. Moving it behind without changing the
// sound means every level-dependent setting has to move by the same amount the other way.
void MUProcessor::moveTrimAfterChain()
{
    const float t = getTrimDb();
    auto shift = [this, t] (const juce::String& id, float lo, float hi) { setParam (id, juce::jlimit (lo, hi, getParam (id) - t)); };
    if (std::abs (t) > 0.05f)
    {
        shift (ids::compThreshold, -60.0f, 0.0f);
        shift (ids::dsThreshold, -60.0f, 0.0f);
        shift (ids::gateThreshold, -80.0f, 0.0f);
        shift (ids::riderTarget, -40.0f, -6.0f);
        for (int b = 0; b < kNumEQBands; ++b) shift (ids::eqBandThr (b), -60.0f, 0.0f);
    }
    trimBeforeChain.store (false);
}

float MUProcessor::toolLevel() const
{
    const float in = inputProbe.getLevel();
    return in > -70.0f ? in : loudness.getShortTerm() - faderDb();
}

// What comes after the processing: the level-dependent tools hear the track without it.
float MUProcessor::faderDb() const
{
    return (trimBeforeChain.load() ? 0.0f : getTrimDb()) + getParam (ids::outputGain);
}

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
    // Masking cuts are dynamic: they only bite while this track is actually loud there. The threshold is
    // placed from how loud this track is, so the cut is working at the track's normal level and deepens as it
    // gets louder; a fixed threshold would sit above a quiet track and never act.
    const float level = loudness.getShortTerm() > -70.0f ? loudness.getShortTerm() : loudness.getIntegrated();
    s.dynamic = true;
    s.thresholdDb = level > -70.0f ? juce::jlimit (-60.0f, -12.0f, level - 16.0f) : -36.0f;
    s.rangeDb = juce::jlimit (-6.0f, -1.5f, db);
    // reuse a cut that is already there rather than stacking a second one beside it
    int band = -1;
    for (int b = 0; b < kNumEQBands; ++b)
    {
        const auto e = getBandSettings (b);
        if (e.enabled && e.type == EQType::Bell && e.dynamic && std::abs (std::log2 (e.freqHz / hz)) < 0.4f) { band = b; break; }
    }
    setBandSettings (band >= 0 ? band : pickBandFor (s), s);
    setParam (ids::eqEnabled, 1.0f);
}

float MUProcessor::cutDbNear (float hz) const
{
    float deepest = 0.0f;
    // room already made for the vocal counts as a cut in place
    if (getParam (ids::vsEnabled) > 0.5f)
        for (int i = 0; i < 3; ++i)
            if (std::abs (std::log2 (getParam (ids::vsFreq (i)) / juce::jmax (1.0f, hz))) <= 0.5f)
                deepest = juce::jmin (deepest, getParam (ids::vsCut (i)) * getParam (ids::vsAmount) * 0.01f);
    if (getParam (ids::eqEnabled) < 0.5f) return deepest;
    for (int b = 0; b < kNumEQBands; ++b)
    {
        const auto e = getBandSettings (b);
        if (! e.enabled || e.type != EQType::Bell || std::abs (std::log2 (e.freqHz / juce::jmax (1.0f, hz))) > 0.4f) continue;
        deepest = juce::jmin (deepest, e.dynamic ? e.rangeDb : e.gainDb);
    }
    return deepest;
}

SongKey MUProcessor::songKey() const
{
    // The music knows the key better than a single voice does, so ask the session first: a track name that states
    // the key, or the master, the beat or an instrument that has been heard.
    auto k = bus->findSongKey();
    if (k.found) return k;
    const auto role = getEffectiveRole();
    if (role == Role::Vocal || role == Role::BackingVocal)
    {
        const auto g = tuner.guessKey();
        if (g.secondsHeard >= 4.0f) { k.found = true; k.key = g.key; k.minor = g.minor; k.confidence = g.confidence; k.source = getDisplayName(); }
    }
    else
    {
        const auto g = keyFinder.estimate();
        if (g.secondsHeard >= 6.0f) { k.found = true; k.key = g.key; k.minor = g.minor; k.confidence = g.confidence; k.source = getDisplayName(); }
    }
    return k;
}

juce::String MUProcessor::findKey()
{
    const auto k = songKey();
    if (! k.found)
        return "Nothing to go on yet. Play the song for ten seconds with the plugin on the beat or on the Stereo Out, then press Find key again. On a vocal alone, switch Tune on and sing a verse first.";
    setSongKey (k.key, k.minor);
    const bool own = k.source == getDisplayName();
    juce::String s = "Set to " + k.text();
    if (k.fromName) s << ", read from the name of \"" << k.source << "\".";
    else if (own && (getEffectiveRole() == Role::Vocal || getEffectiveRole() == Role::BackingVocal)) s << ", worked out from the notes sung on this track. A beat or the master would give a surer answer.";
    else s << ", heard on \"" << k.source << "\".";
    if (k.note.isNotEmpty()) s << " " << k.note;
    else if (! k.fromName && k.confidence < 0.6f) s << " The match is loose, so check it by ear.";
    return s;
}

void MUProcessor::setSongKey (int key, bool minor)
{
    setParam (ids::tuneKey, (float) juce::jlimit (0, 11, key));
    setParam (ids::tuneScale, minor ? 2.0f : 1.0f);
}

juce::String MUProcessor::sendKeyToSession()
{
    const int key = (int) getParam (ids::tuneKey), scale = (int) getParam (ids::tuneScale);
    if (scale != 1 && scale != 2)
        return "Choose Major or Minor first (or press Find key). Chromatic and the other scales are not a key that can be shared.";
    const int total = bus->getNumInstances();
    const int tuned = bus->sendKeyToAll (key, scale == 2);
    return Tuner::keyNames()[key] + (scale == 2 ? " minor" : " major") + " sent to all " + juce::String (total) + (total == 1 ? " instance" : " instances") + " in this song. "
         + (tuned == 0 ? juce::String ("None of them has Tune switched on yet; the key is waiting for when one does.")
                       : juce::String (tuned) + (tuned == 1 ? " of them has" : " of them have") + " Tune on and now " + (tuned == 1 ? "follows" : "follow") + " it.");
}

// ---- Peaks ----------------------------------------------------------------------------------------
float MUProcessor::getPeakOverDb() const
{
    if (isMasterInstance()) return 0.0f;
    const float peak = guard.getInputPeakDb();
    return peak > -90.0f ? juce::jmax (0.0f, peak + 1.0f) : 0.0f;
}

juce::String MUProcessor::tamePeaks()
{
    const float over = getPeakOverDb();
    juce::String done;
    if (getParam (ids::compEnabled) < 0.5f)
    {
        const auto note = autoCompressor();
        if (getParam (ids::compEnabled) < 0.5f) return {};       // nothing has played yet
        done = "Compressor switched on. " + note;
    }
    else
        // Already compressing. Squeezing harder is what flattens a voice and thickens the low mids, so the
        // compressor is left alone: the peak guard catches the odd peak, and if the track is simply too hot
        // the balance turns the song down around it.
        done = "The compressor is already working and was left as it is; the peak guard is on.";
    juce::ignoreUnused (over);
    setParam (ids::trackGuard, 1.0f);
    resetMeasurement();
    return done;
}

// ---- Reverb chosen for the track ------------------------------------------------------------------
ReverbFacts MUProcessor::reverbFacts() const
{
    ReverbFacts f;
    f.role = getEffectiveRole();
    f.bpm = hostBpm.load();
    f.measured = secondsHeard() >= 3.0f;
    f.crestDb = loudness.getCrestFactorDb();
    f.highShare = spectrum.highEnergyRatio();
    // long notes: the graph has seen notes of half a second or more, or the voice is being tuned slowly on purpose
    int longNotes = 0, notes = 0;
    {
        const juce::ScopedLock sl (graphLock);
        for (auto& n : graph.getNotes()) { ++notes; if (n.end - n.start >= 45) ++longNotes; }
    }
    f.sustainedNotes = (notes >= 6 && longNotes * 4 >= notes) || (f.measured && f.crestDb < 11.0f && (f.role == Role::Vocal));
    return f;
}

juce::String MUProcessor::fitReverb()
{
    const auto a = adviseReverb (reverbFacts());
    if (! a.use)
    {
        setParam (ids::rvOn, 0.0f);
        return a.reason;
    }
    setParam (ids::rvType, (float) a.type); setParam (ids::rvAuto, 1.0f); setParam (ids::rvLength, (float) a.length);
    setParam (ids::rvMix, a.mix); setParam (ids::rvTone, a.tone); setParam (ids::rvDuck, a.duck); setParam (ids::rvLowCut, a.lowCutHz); setParam (ids::rvWidth, a.width);
    setParam (ids::rvOn, 1.0f);
    apvts.state.setProperty ("preset_reverb", "Fitted to this track", nullptr);
    return a.reason;
}

// ---- Tune graph -----------------------------------------------------------------------------------
void MUProcessor::startCapture()
{
    graph.allocate();
    setParam (ids::tuneOn, 1.0f);
    setParam (ids::tuneMode, 1.0f);
    captureSeen = false;
    captureFrom = -1;
    graph.setCapturing (true);
}

void MUProcessor::stopCapture()
{
    if (! graph.isCapturing()) return;
    graph.setCapturing (false);
    captureSeen = false;
    const juce::ScopedLock sl (graphLock);
    const int from = graph.passFirst(), to = graph.passLast();
    if (to >= from) graph.segment (from, to + 1);
}

juce::String MUProcessor::correctAllNotes (float straighten01)
{
    const int key = (int) getParam (ids::tuneKey), scale = (int) getParam (ids::tuneScale), mask = (int) getParam (ids::tuneNotes);
    const juce::ScopedLock sl (graphLock);
    int moved = 0;
    for (auto& n : graph.getNotes())
    {
        const float before = n.pitch;
        n.pitch = (float) Tuner::nearestAllowed (n.sung, key, scale, mask);
        n.straighten = juce::jmax (n.straighten, juce::jlimit (0.0f, 1.0f, straighten01));
        if (std::abs (n.pitch - before) > 0.005f) ++moved;
    }
    graph.rasterise();
    if (graph.getNotes().empty()) return "No notes yet. Press Capture and play the part of the song you want to tune.";
    return juce::String ((int) graph.getNotes().size()) + " notes put on the nearest note of " + Tuner::keyNames()[key] + " " + Tuner::scaleNames()[scale].toLowerCase()
         + ". Play it back; drag any note that landed on the wrong pitch.";
}

void MUProcessor::resetAllNotes()
{
    const juce::ScopedLock sl (graphLock);
    for (auto& n : graph.getNotes()) { n.pitch = n.sung; n.straighten = 0.0f; }
    graph.rasterise();
}

void MUProcessor::clearGraph()
{
    graph.setCapturing (false);
    const juce::ScopedLock sl (graphLock);
    graph.clear();
}

// ---- Presets for one tool, and complete sounds ----------------------------------------------------
juce::String MUProcessor::getModulePresetName (const juce::String& module) const
{
    return apvts.state.getProperty ("preset_" + module, juce::String()).toString();
}

juce::String MUProcessor::applyModulePreset (int index)
{
    const auto& all = modulePresets();
    if (index < 0 || index >= (int) all.size()) return {};
    const auto& p = all[(size_t) index];
    bool rootOnly = false;
    for (auto& v : p.values)
    {
        if (v.id == kRootOnly) { rootOnly = true; continue; }
        setParam (v.id, v.value);
    }
    if (p.module == "tune")
    {
        // The robot allows a single note. Any other Tune preset arriving after it gives the scale back.
        const int mask = (int) getParam (ids::tuneNotes);
        const bool single = (int) getParam (ids::tuneScale) == Tuner::kCustomScale && mask != 0 && (mask & (mask - 1)) == 0;
        if (rootOnly)
        {
            if (! single) apvts.state.setProperty ("scaleBeforeRobot", (int) getParam (ids::tuneScale), nullptr);
            setParam (ids::tuneNotes, (float) (1 << (int) getParam (ids::tuneKey)));
            setParam (ids::tuneScale, (float) Tuner::kCustomScale);
        }
        else if (single) setParam (ids::tuneScale, (float) juce::jlimit (0, 4, (int) apvts.state.getProperty ("scaleBeforeRobot", 2)));
    }
    apvts.state.setProperty ("preset_" + p.module, p.name, nullptr);
    rememberLoaded (p.module, scopeParams (p.module));
    return p.name + ": " + p.blurb;
}

juce::String MUProcessor::applySound (int index)
{
    const auto& all = soundPresets();
    if (index < 0 || index >= (int) all.size()) return {};
    const auto& sound = all[(size_t) index];
    for (auto& m : fxModules()) { setParam (m.onParam, 0.0f); apvts.state.removeProperty ("preset_" + m.id, nullptr); }
    setParam (ids::fxOrder, 0.0f);          // the sounds were made for the standard order
    for (auto& v : sound.values)
    {
        if (v.id == kRootOnly)
        {
            apvts.state.setProperty ("scaleBeforeRobot", (int) getParam (ids::tuneScale), nullptr);
            setParam (ids::tuneNotes, (float) (1 << (int) getParam (ids::tuneKey)));
            setParam (ids::tuneScale, (float) Tuner::kCustomScale);
            continue;
        }
        setParam (v.id, v.value);
    }
    apvts.state.setProperty ("soundName", sound.name, nullptr);
    return sound.name + ": " + sound.blurb;
}

// ---- Bounce check ---------------------------------------------------------------------------------
std::vector<CheckItem> MUProcessor::bounceCheck()
{
    std::vector<CheckItem> out;
    auto add = [&out] (CheckItem::State st, const juce::String& title, const juce::String& detail) { out.push_back ({ st, title, detail }); };
    const auto tracks = bus->collectTracks();
    const auto targets = isMasterInstance() ? currentTargets() : bus->getTargets();
    const TrackInfo* master = nullptr;
    int playing = 0;
    for (auto& t : tracks) { if (t.role == Role::Master) master = &t; else if (t.active) ++playing; }

    if (master == nullptr)
        add (CheckItem::Problem, "No master", "Put the plugin on the Stereo Out and set its role to Master. Without it the loudness and peaks of the finished mix cannot be measured.");
    else if (master->levelLufs < -70.0f)
        add (CheckItem::Info, "Nothing measured yet", "Play the song from start to finish, or at least its loudest section, then look again.");
    else
    {
        // the finished level is read after the master's own limiter when this instance is the master
        const float loud = isMasterInstance() ? loudness.getIntegrated() : master->levelLufs;
        const float want = isMasterInstance() ? getParam (ids::targetLufs) : targets.masterLufs;
        const float diff = loud - want;
        if (std::abs (diff) <= 1.0f) add (CheckItem::Good, "Loudness", juce::String (loud, 1) + " LUFS, on the target of " + juce::String (want, 1) + ".");
        else add (CheckItem::Warn, "Loudness", juce::String (loud, 1) + " LUFS is " + juce::String (std::abs (diff), 1) + (diff > 0.0f ? " above" : " below") + " the target of " + juce::String (want, 1)
                  + ". " + (isMasterInstance() ? "Let the Master tab's auto level settle, or pick the target you want there." : "Open the plugin on the Stereo Out and check the Master tab."));

        const float peak = isMasterInstance() ? loudness.getTruePeakHold() : master->truePeakDb;
        const float ceiling = isMasterInstance() ? getParam (ids::targetTruePeak) : targets.masterTruePeak;
        if (peak > -0.05f) add (CheckItem::Problem, "Peaks", "The mix reached " + juce::String (peak, 1) + " dB true peak: it is clipping. Switch the limiter on in the Master tab, or bring the tracks down with Learn and balance.");
        else if (peak > ceiling + 0.3f) add (CheckItem::Warn, "Peaks", "Highest peak " + juce::String (peak, 1) + " dB, above the ceiling of " + juce::String (ceiling, 1) + ".");
        else add (CheckItem::Good, "Peaks", "Highest peak " + juce::String (peak, 1) + " dB true peak, under the ceiling.");

        const float corr = bus->getMasterCorrelation();
        if (corr < 0.0f) add (CheckItem::Problem, "Mono", "Stereo correlation " + juce::String (corr, 2) + ": parts of the mix cancel out in mono. Narrow the widest tracks (Tone tab, Stereo width) and check with the Mono listening check.");
        else if (corr < 0.3f) add (CheckItem::Warn, "Mono", "Stereo correlation " + juce::String (corr, 2) + ": very wide. Listen with the Mono check on the Master tab to make sure nothing important disappears.");
        else add (CheckItem::Good, "Mono", "The mix holds up in mono (correlation " + juce::String (corr, 2) + ").");
    }

    const auto hot = bus->findHotPeaks();
    if (hot.empty()) { if (playing > 0) add (CheckItem::Good, "Track peaks", "No track is hitting the top."); }
    else
    {
        juce::StringArray names;
        for (auto& h : hot) names.add ("\"" + h.name + "\" (" + juce::String (h.overDb, 1) + " dB over)");
        add (CheckItem::Warn, "Track peaks", names.joinIntoString (", ") + (hot.size() == 1 ? " is" : " are") + " leaning on the peak guard. Press Fix beside each one on this page.");
    }

    const auto plan = bus->computePlan (targets);
    if (plan.usable && playing >= 2)
    {
        juce::StringArray moves;
        for (auto& m : plan.moves) if (std::abs (m.deltaDb) >= 1.5f) moves.add ("\"" + m.name + "\" " + (m.deltaDb > 0.0f ? "up " : "down ") + juce::String (std::abs (m.deltaDb), 1) + " dB");
        if (moves.isEmpty()) add (CheckItem::Good, "Balance", "Every track is within 1.5 dB of where its role should sit.");
        else add (CheckItem::Warn, "Balance", "Off balance: " + moves.joinIntoString (", ") + ". Press Balance now, or leave it if you set these by ear.");
    }
    const auto clashes = bus->computeMasking();
    if (playing >= 2)
    {
        if (clashes.empty()) add (CheckItem::Good, "Collisions", "No track is covering another.");
        else add (CheckItem::Warn, "Collisions", juce::String ((int) clashes.size()) + (clashes.size() == 1 ? " place" : " places") + " where one track covers another. Each has a Fix button below.");
    }

    if (bus->isLowLatency()) add (CheckItem::Problem, "Low latency is on", "Recording mode switches the master limiter off and loosens Tune. Turn it off in Settings before you bounce.");
    if (getListenCheck() != ListenCheck::Off) add (CheckItem::Info, "Listening check is on", "It is left out of a bounce automatically, but switch it off so you are hearing the real mix.");
    return out;
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

void MUProcessor::startListening()   { spectrum.startListen(); }
void MUProcessor::cancelListening()  { spectrum.stopListen(); }

std::vector<Suggestion> MUProcessor::analyse (juce::String& status)
{
    std::vector<Suggestion> out;
    // A listening pass, if one has just been made, is what the analysis is based on: a known stretch of sound,
    // silence left out, with how each band behaved over it. Without one it falls back on the running average.
    BandArray bands {}, swing {};
    const float listened = spectrum.getListenSeconds();
    const bool fromPass = listened >= 3.0f && spectrum.listenBands (bands, swing);
    spectrum.stopListen();
    const float heard = fromPass ? listened : secondsHeard();
    if (heard < 3.0f)
    {
        status = heard < 0.2f ? "Nothing has played yet. Start playback on the loudest part of the song, wait a few seconds, then analyse."
                              : "Only " + juce::String (heard, 1) + " s heard so far. Keep the song playing for a few more seconds and analyse again.";
        return out;
    }

    if (! fromPass) fillBandsDb (bands);
    const auto role = getEffectiveRole();
    const auto source = getEqSource();
    const int passes = juce::jlimit (1, kAllEqPasses, (int) getParam (ids::eqPasses));
    const bool detailed = getParam (ids::eqDepth) > 0.5f;
    out = analyseTone (bands, role, fromPass ? &swing : nullptr, source, passes, detailed ? 0.6f : 1.0f);
    std::vector<float> heardNotes;
    // Narrow peaks, at the analyser's own resolution (the ones that can be seen on the display): a peak standing
    // clear of the spectrum around it over the last few seconds. Not on a bass, a kick or a whole mix, where a
    // peak is a note. On a voice or an instrument that holds notes, the tall narrow peaks ARE the notes and their
    // harmonics, so the notes are found first (the lines low down) and anything that is one of them, or a whole
    // multiple of one, is left out: cutting it would be cutting the tune. The same test is put to the ringing
    // found by the broad analysis, where one band can hold a single harmonic of the note that is sung most.
    {
        const Role fam = eqSourceRole (source, role);
        const bool pitched = fam == Role::Vocal || fam == Role::BackingVocal || fam == Role::Melody;
        const std::vector<float> notes = pitched ? spectrum.findNoteLines (70.0f, 1500.0f, 6.0f) : std::vector<float>();
        heardNotes = notes;
        const float binHz = spectrum.getBinHz();
        if (! notes.empty())
            out.erase (std::remove_if (out.begin(), out.end(), [&] (const Suggestion& s)
            {
                if (! s.title.startsWith ("Ringing")) return false;
                for (float f0 : notes) for (int k = 1; k <= 8; ++k) if (std::abs (std::log2 ((float) k * f0 / s.band.freqHz)) < 0.17f) return true;
                return false;
            }), out.end());
        if ((passes & 1) != 0 && (pitched || fam == Role::Drums))
        {
            auto peaks = spectrum.findResonances (fam == Role::Drums ? 200.0f : 700.0f, 9000.0f, detailed ? 6.0f : 8.0f);
            auto harmonic = [&] (float f) { return SpectrumAnalyzer::isHarmonicOf (f, notes, binHz); };
            int added = 0;
            for (auto& pk : peaks)
            {
                if (added >= (detailed ? 3 : 1) || harmonic (pk.freqHz)) continue;
                // one that the broad analysis already placed to the nearest third of an octave is replaced by the exact one
                out.erase (std::remove_if (out.begin(), out.end(), [&pk] (const Suggestion& s) { return s.title.startsWith ("Ringing") && std::abs (std::log2 (s.band.freqHz / pk.freqHz)) < 0.3f; }), out.end());
                bool covered = false;
                for (auto& s : out) if (s.kind == Suggestion::Kind::EqBand && s.stage != EqStage::Tonal && std::abs (std::log2 (s.band.freqHz / pk.freqHz)) < 0.2f) covered = true;
                if (covered) continue;
                Suggestion s; s.stage = EqStage::Surgical; s.freqHz = pk.freqHz; s.amountDb = pk.prominenceDb;
                const juce::String at = pk.freqHz >= 1000.0f ? juce::String (pk.freqHz / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (pk.freqHz)) + " Hz";
                s.title = "Narrow peak at " + at;
                s.why = "On the display there is a narrow peak here, " + juce::String (pk.prominenceDb, 1) + " dB clear of the spectrum around it over the last few seconds. It is not one of the notes that were held while listening, nor a harmonic of one, so it is likely a resonance (the room, the microphone, the instrument) and a narrow cut takes it out. That is a likelihood, not a certainty: solo the band and sweep it, and if the peak moves with the tune, leave it.";
                s.band.enabled = true; s.band.type = EQType::Bell; s.band.freqHz = pk.freqHz; s.band.q = juce::jlimit (3.0f, 8.0f, pk.q);
                s.band.gainDb = -juce::jlimit (2.0f, 4.5f, 0.5f * pk.prominenceDb);
                s.action = "Narrow cut " + juce::String (s.band.gainDb, 1) + " dB at " + at;
                // after the other surgical moves, before the tonal ones
                auto where = std::find_if (out.begin(), out.end(), [] (const Suggestion& x) { return x.kind == Suggestion::Kind::EqBand && x.stage != EqStage::Surgical; });
                out.insert (where, s);
                ++added;
            }
        }
    }

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
        out[(size_t) clashes].stage = EqStage::Dynamic;
        ++clashes;
    }
    // A beat with no stems cannot be remixed around the vocal, but it can step back where it covers the vocal
    // while the vocal is singing. If that has not been set up for this track yet, it is the first thing offered.
    const Role family = eqSourceRole (source, role);
    if ((passes & 4) != 0 && family != Role::Vocal && family != Role::BackingVocal && ! master)
    {
        const auto plan = planVocalSpace();
        // not offered again once Vocal space is on and already dipping in those places
        bool alreadyThere = plan.found && getParam (ids::vsEnabled) > 0.5f;
        for (int i = 0; i < plan.count && alreadyThere; ++i) alreadyThere = cutDbNear (plan.freqHz[(size_t) i]) <= -1.0f;
        if (plan.found && ! alreadyThere)
        {
            Suggestion s; s.kind = Suggestion::Kind::VocalRoom; s.stage = EqStage::Dynamic; s.room = plan; s.freqHz = plan.freqHz[0]; s.amountDb = plan.cutDb[0];
            s.title = "Make room for the vocal";
            s.why = "Measured against the lead vocal in this session, this track is as loud as the vocal or louder in " + juce::String (plan.count) + (plan.count == 1 ? " place" : " places")
                  + " where the vocal is strong. With no stems to turn down, the fix is to let this track dip there, only while the vocal is singing, and come back when it stops. That is Vocal space (Tone tab), set to where these two actually overlap.";
            s.action = "Dip at " + plan.text + " while the vocal sings";
            out.insert (out.begin(), s);
        }
    }

    const juce::String what = source == EqSource::TrackKind ? roleNames()[(int) role].toLowerCase() : eqSourceNames()[(int) source].toLowerCase();
    const juce::String basis = fromPass ? "Listened to " + juce::String (listened, 0) + " s of this track as: " + what + ". " : juce::String();
    juce::StringArray jobs;
    if (passes & 1) jobs.add ("surgical"); if (passes & 2) jobs.add ("tonal"); if (passes & 4) jobs.add ("dynamic");
    // What the tall narrow peaks on the display are, said in so many words: they look like the obvious thing to cut.
    juce::String aboutNotes;
    if (! heardNotes.empty())
    {
        std::sort (heardNotes.begin(), heardNotes.end());
        juce::StringArray named;
        std::vector<float> roots;
        for (float f : heardNotes)
            if (named.size() < 3 && ! SpectrumAnalyzer::isHarmonicOf (f, roots, spectrum.getBinHz(), 8)) { roots.push_back (f); named.add (juce::String (juce::roundToInt (f))); }
        juce::String list = named[0];
        for (int i = 1; i < named.size(); ++i) list += (i == named.size() - 1 ? " and " : ", ") + named[i];
        aboutNotes = " The tall narrow peaks on the display at " + list + " Hz and at whole multiples of " + (named.size() == 1 ? "that" : "those")
                   + " are the notes being " + (eqSourceRole (source, role) == Role::Melody ? "played" : "sung") + ": they are the tune, not faults, and are never suggested as cuts.";
    }
    if (out.empty())
        status = basis + "Nothing stands out (" + jobs.joinIntoString (", ") + "). The tone of this track is in the normal range for " + what + ", so leave the EQ alone unless your ears say otherwise." + aboutNotes;
    else
        status = basis + juce::String ((int) out.size()) + (out.size() == 1 ? " thing" : " things") + " worth a look, in the order an engineer would do them: surgical cuts first, then tonal lifts, then dynamic. Read why, then apply the ones you agree with." + aboutNotes;
    return out;
}

juce::String MUProcessor::applySuggestion (const Suggestion& s)
{
    switch (s.kind)
    {
        case Suggestion::Kind::EqBand:
        {
            bool added = false;
            const int b = mergeEqBand (s.band, added);
            if (b < 0) return "Left as it is: the EQ is already doing as much as it should here (about 10 dB at most in one place, and no band is replaced to make room). If it still sounds wrong, free a band on the EQ tab or look at the recording itself: mic position, distance, a pop shield.";
            setParam (ids::eqEnabled, 1.0f);
            restartSpectrumAverage();
            if (! added) return "Band " + juce::String (b + 1) + " set: " + s.action;
            // The suggestion was measured with the EQ already working, so it is what is still missing: it goes on
            // top of the band that is already doing that job instead of replacing it or stacking a second one.
            const auto now = getBandSettings (b);
            const float amount = now.dynamic ? now.rangeDb : now.gainDb;
            return "Band " + juce::String (b + 1) + " moved to " + (amount >= 0.0f ? "+" : "") + juce::String (amount, 1) + " dB at "
                 + (now.freqHz >= 1000.0f ? juce::String (now.freqHz / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (now.freqHz)) + " Hz")
                 + " (" + s.action.substring (0, 1).toLowerCase() + s.action.substring (1) + ", added to what was there)";
        }
        case Suggestion::Kind::DeEsser:
        {
            setParam (ids::dsFreq, juce::jlimit (2000.0f, 12000.0f, s.freqHz));
            setParam (ids::dsRange, juce::jlimit (3.0f, 8.0f, std::abs (s.amountDb)));
            const float level = toolLevel();
            if (level > -60.0f) setParam (ids::dsThreshold, juce::jlimit (-60.0f, 0.0f, level - 8.0f));
            setParam (ids::dsOn, 1.0f);
            restartSpectrumAverage();
            return "De-esser switched on (Dynamics tab): " + s.action;
        }
        case Suggestion::Kind::Saturation:
        {
            setParam (ids::satType, 2.0f);
            setParam (ids::satDrive, 6.0f);
            setParam (ids::satMix, 35.0f);
            setParam (ids::satOn, 1.0f);
            restartSpectrumAverage();
            return "Saturation switched on (Tone tab): " + s.action;
        }
        case Suggestion::Kind::Masking:
        {
            if (s.clash.slotB == slot) applyMaskingCut (s.clash.freqHz, s.clash.suggestedCutDb);
            else bus->applyMaskingClash (s.clash);
            return "Done: " + s.action;
        }
        case Suggestion::Kind::VocalRoom:
            return fitVocalSpace();
    }
    return {};
}

VocalRoomPlan MUProcessor::planVocalSpace() const
{
    VocalRoomPlan none;
    const auto tracks = bus->collectTracks();
    const TrackInfo* vocal = nullptr; const TrackInfo* me = nullptr;
    for (auto& t : tracks)
    {
        if (t.slot == slot) me = &t;
        else if (t.role == Role::Vocal && t.hasBands && (vocal == nullptr || t.levelLufs > vocal->levelLufs)) vocal = &t;
    }
    if (vocal == nullptr) { none.text = "There is no lead vocal in this session yet (a track with the plugin on it, set to Lead vocal, that has played). Vocal space needs one to follow."; return none; }
    BandArray mine {};
    if (me != nullptr && me->hasBands) mine = me->bandsDb;
    else if (! fillBandsDb (mine)) { none.text = "This track has not played yet. Play the song with the vocal in it and try again."; return none; }
    return planVocalRoom (vocal->bandsDb, mine);
}

juce::String MUProcessor::fitVocalSpace()
{
    const auto role = getEffectiveRole();
    if (role == Role::Vocal || role == Role::BackingVocal || role == Role::Master)
        return "Vocal space is for the tracks around the vocal (the beat, instruments, FX), not for a vocal or the master.";
    const auto plan = planVocalSpace();
    if (! plan.found) return plan.text;
    for (int i = 0; i < 3; ++i)
    {
        setParam (ids::vsFreq (i), plan.freqHz[(size_t) i]);
        setParam (ids::vsCut (i), i < plan.count ? plan.cutDb[(size_t) i] : 0.0f);
        setParam (ids::vsQ (i), plan.q[(size_t) i]);
    }
    setParam (ids::vsAmount, 100.0f);
    setParam (ids::vsEnabled, 1.0f);
    return "Room made for the vocal at " + plan.text + ", only while the vocal is singing. Amount (Tone tab) scales all of it; Centre only leaves the sides of the track alone.";
}

// ---- AI tutor ------------------------------------------------------------------------------------
juce::String MUProcessor::tutorPrompt (const juce::String& question)
{
    return tutor::corePrompt() + tutor::notesFor (question, tutorFacts()) + tutorTrackNotes();
}

// The comparisons a small model gets wrong are made here, and handed over as plain sentences.
juce::StringPairArray MUProcessor::tutorFacts()
{
    juce::StringPairArray facts;
    const auto tracks = bus->collectTracks();
    const auto plan = bus->computePlan (bus->getTargets());

    juce::String balance;
    if (tracks.size() < 2) balance = "Only one track has the plugin on it so far, so there is nothing to balance yet: put the plugin on the other tracks too.";
    else if (! plan.usable) balance = "The balance has not been measured yet: play the loudest section of the song for a few seconds first.";
    else
    {
        juce::StringArray moves;
        for (auto& m : plan.moves)
            if (std::abs (m.deltaDb) >= 1.0f)
                moves.add ("\"" + m.name + "\" should come " + (m.deltaDb > 0.0f ? "up " : "down ") + juce::String (std::abs (m.deltaDb), 1) + " dB");
        balance = moves.isEmpty() ? juce::String ("Measured just now: every track is within 1 dB of where it should sit, so the mix is balanced.")
                                  : "Measured just now, the mix is not balanced yet: " + moves.joinIntoString (", and ") + ".";
    }
    facts.set ("balance", balance);

    juce::String width;
    if (getTotalNumOutputChannels() < 2) width = "This track is mono, so it cannot be too wide.";
    else if (secondsHeard() < 3.0f) width = "The width of this track has not been measured yet: play it for a few seconds.";
    else
    {
        const float c = stereo.getCorrelation();
        width = "Measured on this track: stereo correlation is " + juce::String (c, 2) + ", which means "
              + (c < 0.0f ? "it is too wide and parts of it will vanish in mono, so bring Stereo width down."
                 : c < 0.4f ? "it is very wide but still survives in mono; check it in mono by ear."
                            : "it is not too wide and it works in mono.");
    }
    facts.set ("width", width);

    juce::String loud;
    if (! plan.hasMaster) loud = "The loudness of the whole mix is not being measured yet: put the plugin on the stereo output and set its role to Master.";
    else
    {
        const float l = plan.measuredMasterLufs;
        loud = "Measured: the whole mix is at " + juce::String (l, 1) + " LUFS, which is "
             + (l < -16.0f ? "quieter than the -14 LUFS that streaming services play at. That is normal for a mix that has not been mastered yet, and it is not what squashes a mix."
                : l <= -12.0f ? "about the level streaming services play at, so it does not need to be any louder."
                              : "louder than the -14 LUFS that streaming services play at, so they will turn it down. Pushing this hard into the limiter is what makes a mix sound squashed.");
    }
    facts.set ("mixloudness", loud);
    return facts;
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
    if (getParam (ids::tuneOn) > 0.5f)
        on.add ("Tune (" + Tuner::keyNames()[(int) getParam (ids::tuneKey)] + " " + Tuner::scaleNames()[(int) getParam (ids::tuneScale)].toLowerCase() + ", speed " + getParamText (ids::tuneSpeed) + ")");
    if (getParam (ids::dblOn) > 0.5f) on.add (getParam (ids::dblVoices) > 0.5f ? "doubler (stack)" : "doubler");
    if (getParam (ids::dlOn) > 0.5f) on.add ("delay (" + juce::String (juce::roundToInt (echo.currentTimeMs())) + " ms)");
    if (getParam (ids::rvOn) > 0.5f) on.add (Reverb::typeNames()[(int) getParam (ids::rvType)].toLowerCase() + " reverb (" + juce::String (reverb.currentDecaySeconds(), 1) + " s)");
    if (getParam (ids::hmOn) > 0.5f) on.add ("harmony (" + PitchFX::voicingNames()[(int) getParam (ids::hmVoicing)].toLowerCase() + ")");
    if (getParam (ids::psOn) > 0.5f) on.add ("shift (" + getParamText (ids::psSemis) + ")");
    if (getParam (ids::phOn) > 0.5f) on.add ("phone effect");
    if (getParam (ids::lfOn) > 0.5f) on.add ("lo-fi");
    if (getParam (ids::mdOn) > 0.5f) on.add (juce::StringArray { "chorus", "flanger", "phaser" }[(int) getParam (ids::mdType)]);
    if (getParam (ids::chOn) > 0.5f) on.add ("chop");
    if (role == Role::Master && getParam (ids::peakGuard) > 0.5f) on.add ("limiter");
    s << "Also switched on: " << (on.isEmpty() ? juce::String ("nothing") : on.joinIntoString (", ")) << ". ";

    juce::String status;
    const auto found = analyse (status);
    s << "\nSuggestions right now (EQ tab, Analyse button): ";
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
    restartSpectrumAverage();
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
        // Voices get the two-stage Vocal style: a 3:1 leveller slow enough to let consonants through, with a fast
        // catcher behind it for the peaks. The detector ignores everything under 110 Hz, so plosives and
        // close-mic boom do not push the whole voice down.
        case Role::Vocal:        style = 4; ratio = 3.0f; att = 12.0f; rel = 110.0f; knee = 8.0f; scHp = 110.0f; break;
        case Role::BackingVocal: style = 4; ratio = 4.0f; att = 8.0f;  rel = 120.0f; knee = 8.0f; scHp = 120.0f; break;
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
    return "Set for " + roleNames()[(int) role].toLowerCase() + ": " + Compressor::styleNames()[style] + ", " + juce::String (ratio, 1) + ":1, threshold " + getParamText (ids::compThreshold) + ".";
}

// Places the thresholds relative to how loud the track actually is, so a preset gives the amount of
// gain reduction it was designed for whatever level the track was recorded at.
void MUProcessor::fitThresholds (Role presetRole, juce::String& note)
{
    if (loudness.getShortTerm() <= -60.0f)
    {
        if (getParam (ids::compEnabled) > 0.5f) note = " Play the track and load the preset again to fit its thresholds to your level.";
        return;
    }
    // the level the tools themselves hear: the track as it comes in, before any of them has touched it
    const float post = faderDb();
    const float level = toolLevel();
    if (getParam (ids::compEnabled) > 0.5f)
    {
        const auto style = (CompStyle) (int) getParam (ids::compStyle);
        const float ratio = juce::jmax (1.1f, getParam (ids::compRatio));
        const bool vocal = presetRole == Role::Vocal || presetRole == Role::BackingVocal;
        float thr;
        if (style == CompStyle::Vocal)
            // Measured on real voices: with the threshold 2 dB under the three-second loudness, the leveller
            // takes about 4 dB off the loud-to-soft range and the catcher 2 to 3 dB off the peaks, which evens
            // a voice out without flattening it.
            thr = level - 2.0f;
        else if (style == CompStyle::Smooth || style == CompStyle::Glue)
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
            const float bandLevel = spectrum.hasLongTerm() ? spectrum.longTermBandDbAt (getParam (ids::eqBandFreq (b))) - post : level - 12.0f;
            setParam (ids::eqBandThr (b), juce::jlimit (-60.0f, 0.0f, bandLevel + 3.0f));
        }
    note = " Thresholds fitted to this track (" + juce::String (level, 1) + " LUFS).";
}

juce::String MUProcessor::applyChainPreset (int index)
{
    const auto& presets = chainPresets();
    if (index < 0 || index >= (int) presets.size()) return {};
    const auto& p = presets[(size_t) index];
    setParam (ids::fxOrder, 0.0f);          // the built-in chains were made for the standard order
    for (auto& pv : p.values) setParam (pv.id, pv.value);
    for (auto* tab : { "eq", "dynamics", "tone", "master" }) apvts.state.removeProperty ("preset_" + juce::String (tab), nullptr);       // a whole chain replaces whatever a tab had loaded
    currentPreset = p.name;
    apvts.state.setProperty ("chainPresetName", currentPreset, nullptr);
    juce::String note;
    fitThresholds (p.role, note);
    // A vocal chain's EQ is a starting shape. It is then fitted to the voice on this track, from what has been
    // heard of it, so a dark recording gets the lift it needs and a bright one is not made brighter.
    if (p.fitsTone && (p.role == Role::Vocal || p.role == Role::BackingVocal))
    {
        const auto fitted = fitTone();
        if (fitted.startsWith ("EQ fitted") || fitted.startsWith ("The tone")) note << " " << fitted;
    }
    restartSpectrumAverage();
    if (p.role == Role::Master) { masterTrimDb.store (0.0f); autoLevelHeld.store (false); masterLevelSet.store (false); }
    resetMeasurement();                 // readings taken with the old chain no longer describe this track
    rememberLoaded ("chain", scopeParams ("chain"));
    return p.name + " loaded." + note;
}

// ---- Tone fitted to the voice ---------------------------------------------------------------------
// The long-term spectrum is measured at the output, so it has the EQ in it. To know what the voice itself sounds
// like, the EQ that was in force during the measurement is remembered: set outright whenever the average starts
// again, and otherwise following the EQ at the same pace as the average does.
void MUProcessor::restartSpectrumAverage()
{
    spectrum.resetLongTerm();
    const auto centres = analysisBandCentres();
    for (int b = 0; b < kAnalysisBands; ++b) measuredThroughEq[(size_t) b] = eqResponseDbAt (centres[(size_t) b]);
}

void MUProcessor::noteEqForMeasurement()
{
    const auto centres = analysisBandCentres();
    for (int b = 0; b < kAnalysisBands; ++b)
        measuredThroughEq[(size_t) b] += (eqResponseDbAt (centres[(size_t) b]) - measuredThroughEq[(size_t) b]) * 0.02f;      // timer runs every 100 ms: about 5 s
}

float MUProcessor::eqResponseDbAt (float hz) const
{
    if (getParam (ids::eqEnabled) < 0.5f) return 0.0f;
    const double fs = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    float total = 0.0f;
    for (int b = 0; b < kNumEQBands; ++b)
    {
        auto band = getBandSettings (b);
        if (! band.enabled) continue;
        if (band.dynamic) band.gainDb = band.rangeDb * 0.4f;         // a dynamic band works part of the time; count it for that much
        band.dynamic = false;
        total += EQEngine::bandResponseDb (band, hz, fs);
    }
    return total;
}

bool MUProcessor::isEqUntouched() const
{
    if (getParam (ids::eqEnabled) < 0.5f) return true;
    for (int b = 0; b < kNumEQBands; ++b) if (getParam (ids::eqBandEnabled (b)) > 0.5f) return false;
    return true;
}

// Puts an EQ move into the EQ. If a band of the same kind is already working the same area the move is added to
// it; otherwise it takes a free band. Either way the whole EQ is kept inside what a mix engineer would call a big
// move, not a broken one: nothing automatic takes the EQ past 10 dB up or down at the place it is working on.
// Returns the band used, or -1 if the EQ is already doing as much as it should there.
int MUProcessor::mergeEqBand (EQBandSettings band, bool& addedToExisting)
{
    addedToExisting = false;
    const bool shelf = band.type == EQType::HighShelf || band.type == EQType::LowShelf;
    if (! band.dynamic && band.type != EQType::LowCut && band.type != EQType::HighCut && std::abs (band.gainDb) > 0.01f)
    {
        // where this move does its work: a shelf an octave beyond its corner, anything else at its centre
        const float at = band.type == EQType::HighShelf ? juce::jmin (16000.0f, band.freqHz * 2.0f) : band.type == EQType::LowShelf ? juce::jmax (30.0f, band.freqHz * 0.5f) : band.freqHz;
        const float already = eqResponseDbAt (at);
        const float room = band.gainDb > 0.0f ? juce::jmax (0.0f, 10.0f - already) : juce::jmin (0.0f, -10.0f - already);
        band.gainDb = band.gainDb > 0.0f ? juce::jmin (band.gainDb, room) : juce::jmax (band.gainDb, room);
        if (std::abs (band.gainDb) < 0.5f) return -1;
    }
    int into = -1;
    for (int b = 0; b < kNumEQBands && into < 0; ++b)
    {
        const auto have = getBandSettings (b);
        if (have.enabled && have.type == band.type && have.dynamic == band.dynamic && band.type != EQType::LowCut && band.type != EQType::HighCut
            // (two narrow cuts a third of an octave apart are two different things: how near counts as "the same
            // place" shrinks with the width of the narrower band)
            && std::abs (std::log2 (have.freqHz / juce::jmax (20.0f, band.freqHz))) < (shelf ? 1.3f : juce::jlimit (0.12f, 0.6f, 0.7f / juce::jmax (have.q, band.q)))) into = b;
    }
    if (into < 0)
    {
        into = pickBandFor (band);
        const auto there = getBandSettings (into);
        // A free band, or the slot that kind of filter always uses: it goes in as it is.
        if (! there.enabled || band.type == EQType::LowCut || band.type == EQType::HighCut) { setBandSettings (into, band); return into; }
        // Every band is in use. The move is never allowed to wipe out a band that is doing a different job (it
        // used to replace the nearest one): it joins a band of its own kind if there is one, or is left out.
        if (there.type != band.type || there.dynamic != band.dynamic) return -1;
    }
    const auto have = getBandSettings (into);
    const float most = shelf ? 9.0f : 8.0f;
    if (band.dynamic) band.rangeDb = juce::jlimit (-9.0f, most, have.rangeDb + band.rangeDb);
    else              band.gainDb = juce::jlimit (-9.0f, most, have.gainDb + band.gainDb);
    band.freqHz = std::sqrt (have.freqHz * band.freqHz);
    band.q = juce::jmin (have.q, band.q);
    if (band.dynamic) band.thresholdDb = have.thresholdDb;
    setBandSettings (into, band);
    addedToExisting = true;
    return into;
}

// A raw vocal is not a finished vocal with a small problem: across five real sessions the recordings were 6 to
// 17 dB short of a finished voice somewhere between 2 and 8 kHz, and a fixed "+2 dB of presence" left them dull.
// So the EQ is fitted to the voice that is there. previousEq is what the EQ was doing while the measurement was
// taken (the measurement is of the output), so that can be taken back out.
juce::String MUProcessor::fitTone()
{
    const auto role = getEffectiveRole();
    if (role != Role::Vocal && role != Role::BackingVocal) return {};
    BandArray bands {};
    if (secondsHeard() < 3.0f || ! fillBandsDb (bands)) return "Play the track for a few seconds first, so its tone can be measured.";
    const auto centres = analysisBandCentres();
    // what the voice will sound like through the EQ as it is set now: the measurement, less the EQ that was in
    // force while it was taken, plus the EQ in force now
    for (int b = 0; b < kAnalysisBands; ++b) bands[(size_t) b] += eqResponseDbAt (centres[(size_t) b]) - measuredThroughEq[(size_t) b];

    juce::StringArray did;
    bool lifted = false;
    for (auto& s : analyseTone (bands, role))
    {
        if (s.kind == Suggestion::Kind::DeEsser) { applySuggestion (s); did.add ("de-esser on"); continue; }
        if (s.kind != Suggestion::Kind::EqBand) continue;
        // The fit never adds weight below 500 Hz by itself. Whether a voice needs more body is a judgement to make
        // by ear with the beat playing (Analyse will still suggest it); getting it wrong is how a mix turns muddy.
        if (! s.band.dynamic && s.band.gainDb > 0.0f && s.band.freqHz < 500.0f) continue;
        bool added = false;
        const int used = mergeEqBand (s.band, added);
        if (used < 0) continue;
        const auto band = getBandSettings (used);
        if (! band.dynamic && band.gainDb > 3.5f && band.freqHz > 1500.0f) lifted = true;
        did.add (s.action.substring (0, 1).toLowerCase() + s.action.substring (1));
    }
    if (getParam (ids::eqBandEnabled (0)) < 0.5f)
    {
        // every vocal gets a low cut: nothing a voice needs is under 80 Hz, and what is there makes the compressor work for nothing
        EQBandSettings cutLows; cutLows.enabled = true; cutLows.type = EQType::LowCut; cutLows.freqHz = role == Role::BackingVocal ? 120.0f : 85.0f; cutLows.q = 0.71f; cutLows.slope = 1;
        setBandSettings (0, cutLows);
        did.add ("low cut at " + juce::String (juce::roundToInt (cutLows.freqHz)) + " Hz");
    }
    if (did.isEmpty()) return "The tone of this voice is already where a finished vocal sits, so the EQ was left alone.";
    setParam (ids::eqEnabled, 1.0f);
    // Brightening a voice brightens its S sounds too. The de-esser sits after the EQ; make sure it is watching.
    if (lifted && getParam (ids::dsOn) < 0.5f)
    {
        setParam (ids::dsFreq, 6500.0f); setParam (ids::dsRange, 5.0f);
        if (toolLevel() > -60.0f) setParam (ids::dsThreshold, juce::jlimit (-60.0f, 0.0f, toolLevel() - 8.0f));
        setParam (ids::dsOn, 1.0f);
        did.add ("de-esser on to keep the S sounds in check");
    }
    restartSpectrumAverage();
    return "EQ fitted to this voice: " + did.joinIntoString (", ") + ". It is all on the EQ tab, to adjust or undo.";
}

juce::String MUProcessor::fitToneIfUntouched()
{
    const auto role = getEffectiveRole();
    if ((role != Role::Vocal && role != Role::BackingVocal) || ! isEqUntouched() || secondsHeard() < 3.0f) return {};
    const auto said = fitTone();
    return said.startsWith ("EQ fitted") ? said : juce::String();
}

// ---- Harmony: the chord grid ----------------------------------------------------------------------
namespace
{
const int kMajorScale[7] = { 0, 2, 4, 5, 7, 9, 11 }, kMinorScale[7] = { 0, 2, 3, 5, 7, 8, 10 };
// each progression is four chords, given as the step of the scale each one is built on (0 = the key note)
const int kMajorProgressions[5][4] = { { 0, 4, 5, 3 }, { 0, 5, 3, 4 }, { 5, 3, 0, 4 }, { 0, 3, 4, 3 }, { 1, 4, 0, 0 } };
const int kMinorProgressions[5][4] = { { 0, 5, 2, 6 }, { 0, 6, 5, 6 }, { 0, 3, 5, 4 }, { 5, 6, 0, 0 }, { 0, 3, 0, 4 } };
}

juce::StringArray MUProcessor::chordProgressionNames (bool minor)
{
    return minor ? juce::StringArray { "i - VI - III - VII (the trap and pop minor loop)", "i - VII - VI - VII", "i - iv - VI - v", "VI - VII - i - i", "i - iv - i - v" }
                 : juce::StringArray { "I - V - vi - IV (the four-chord song)", "I - vi - IV - V", "vi - IV - I - V", "I - IV - V - IV", "ii - V - I - I" };
}

juce::String MUProcessor::chordName (int mask)
{
    static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    mask &= 0xFFF;
    for (int root = 0; root < 12; ++root)
    {
        auto has = [&] (int semis) { return ((mask >> ((root + semis) % 12)) & 1) != 0; };
        auto only = [&] (int a, int b) { return mask == ((1 << root) | (1 << ((root + a) % 12)) | (1 << ((root + b) % 12))); };
        if (! has (0)) continue;
        if (only (4, 7)) return names[root];
        if (only (3, 7)) return juce::String (names[root]) + "m";
        if (only (3, 6)) return juce::String (names[root]) + "dim";
    }
    return {};
}

juce::String MUProcessor::fillChords (int progression)
{
    const int scale = (int) getParam (ids::tuneScale), key = juce::jlimit (0, 11, (int) getParam (ids::tuneKey));
    if (scale != 1 && scale != 2 && scale != 3 && scale != 4)
        return "Choose Major or Minor on the Tune tab first (Find key on the Mix tab does it for you), so the chords can be built from the key.";
    const bool minor = scale == 2 || scale == 4;
    const int* notes = minor ? kMinorScale : kMajorScale;
    const int* chosen = (minor ? kMinorProgressions : kMajorProgressions)[juce::jlimit (0, 4, progression)];
    juce::StringArray said;
    for (int slot = 0; slot < 16; ++slot)
    {
        int mask = 0;
        if (slot < 4)
            for (int third = 0; third < 3; ++third) mask |= 1 << ((key + notes[(chosen[slot] + 2 * third) % 7]) % 12);       // three notes of the scale, every other one
        setParam (ids::hmChord (slot), (float) mask);
        if (slot < 4) said.add (chordName (mask));
    }
    setParam (ids::hmChordCount, 4.0f);
    setParam (ids::hmMode, 2.0f);
    return "Chords set: " + said.joinIntoString (", ") + ", one after another. Harmony is in Chords mode: the voices now sing notes of these chords. Click notes in the grid to change them.";
}

juce::String MUProcessor::clearChords()
{
    for (int slot = 0; slot < 16; ++slot) setParam (ids::hmChord (slot), 0.0f);
    return "Chords cleared. With no chord in a slot the voices go back to their intervals in the key.";
}

// ---- Presets of your own --------------------------------------------------------------------------
// Which controls a preset holds. The whole chain is everything that shapes the sound (the same set A/B switches:
// not the role, the bypass, the balance trim or the song's key). A tool's preset holds that tool's controls.
std::vector<juce::String> MUProcessor::scopeParams (const juce::String& scope) const
{
    std::vector<juce::String> ids;
    if (scope == "chain") { for (auto* p : abParams) ids.push_back (p->getParameterID()); return ids; }
    // A tab's preset holds everything on that tab that shapes the sound, and nothing that belongs to the song
    // (the role, the balance trim, the output level).
    if (scope == "eq")
    {
        ids = { ids::eqEnabled, ids::eqAutoGain };
        for (int b = 0; b < kNumEQBands; ++b)
            for (auto& id : { ids::eqBandEnabled (b), ids::eqBandType (b), ids::eqBandFreq (b), ids::eqBandGain (b), ids::eqBandQ (b), ids::eqBandDyn (b),
                              ids::eqBandThr (b), ids::eqBandRange (b), ids::eqBandSlope (b), ids::eqBandMode (b) }) ids.push_back (id);
        return ids;
    }
    if (scope == "dynamics")
        return { ids::gateOn, ids::gateThreshold, ids::gateRange, ids::gateAttack, ids::gateRelease, ids::gateHold,
                 ids::compEnabled, ids::compThreshold, ids::compRatio, ids::compAttack, ids::compRelease, ids::compKnee, ids::compMakeup, ids::compAutoMakeup, ids::compMix, ids::compStyle, ids::compScHp,
                 ids::dsOn, ids::dsFreq, ids::dsThreshold, ids::dsRange, ids::dsMode };
    if (scope == "tone")
        return { ids::smOn, ids::smDepth, ids::smFocus, ids::smSpeed, ids::satOn, ids::satDrive, ids::satMix, ids::satType,
                 ids::riderOn, ids::riderTarget, ids::riderRange, ids::riderSpeed, ids::stWidth, ids::stMonoBass, ids::stMonoFreq, ids::vsEnabled, ids::vsAmount, ids::vsSpeed,
                 ids::vsFreq (0), ids::vsFreq (1), ids::vsFreq (2), ids::vsCut (0), ids::vsCut (1), ids::vsCut (2), ids::vsQ (0), ids::vsQ (1), ids::vsQ (2), ids::vsCentre };
    if (scope == "master")
    {
        std::vector<juce::String> m { ids::masterMode, ids::targetLufs, ids::targetTruePeak, ids::maxGain, ids::peakGuard, ids::mstStyle, ids::mstIntensity,
                                      ids::mbOn, ids::mbCrossLow, ids::mbCrossHigh, ids::clipOn, ids::clipAmount, ids::clipSoft };
        for (int b = 0; b < 3; ++b)
            for (auto& id : { ids::mbBandOn (b), ids::mbThreshold (b), ids::mbRatio (b), ids::mbAttack (b), ids::mbRelease (b), ids::mbMakeup (b) }) m.push_back (id);
        return m;
    }
    for (auto& preset : modulePresets())
        if (preset.module == scope)
            for (auto& v : preset.values)
                if (v.id != kRootOnly && std::find (ids.begin(), ids.end(), v.id) == ids.end()) ids.push_back (v.id);
    // a tool's own preset holds every control of the tool, including the ones no built-in preset touches
    auto also = [&ids] (const juce::String& id) { if (std::find (ids.begin(), ids.end(), id) == ids.end()) ids.push_back (id); };
    if (scope == "harmony")
    {
        for (auto& id : { ids::hmMode, ids::hmLeading, ids::hmChoir, ids::hmChordCount, ids::hmChordLength }) also (id);
        for (int v = 0; v < 4; ++v)
            for (auto& id : { ids::hmVoiceOn (v), ids::hmVoiceSteps (v), ids::hmVoiceLevel (v), ids::hmVoicePan (v), ids::hmVoiceDelay (v), ids::hmVoiceSize (v), ids::hmVoiceDetune (v) }) also (id);
        for (int slot = 0; slot < 16; ++slot) also (ids::hmChord (slot));
    }
    if (scope == "shift") for (auto& id : { ids::psMode, ids::psFine, ids::psDrive, ids::psWidth }) also (id);
    return ids;
}

bool MUProcessor::isFactoryName (const juce::String& scope, const juce::String& name)
{
    if (scope == "chain") return findChainPreset (name) >= 0;
    for (auto& p : modulePresets()) if (p.module == scope && p.name == name) return true;
    return false;
}

juce::String MUProcessor::scopePresetName (const juce::String& scope) const
{
    return scope == "chain" ? currentPreset : getModulePresetName (scope);
}

void MUProcessor::rememberLoaded (const juce::String& scope, const std::vector<juce::String>& ids)
{
    auto& kept = loadedPreset[scope];
    kept.clear();
    for (auto& id : ids) if (apvts.getParameter (id) != nullptr) kept.push_back ({ id, getParam (id) });
}

bool MUProcessor::presetEdited (const juce::String& scope) const
{
    const auto found = loadedPreset.find (scope);
    if (found == loadedPreset.end() || scopePresetName (scope).isEmpty()) return false;
    for (auto& v : found->second)
        if (auto* p = apvts.getParameter (v.id))
            if (std::abs (p->convertTo0to1 (getParam (v.id)) - p->convertTo0to1 (v.value)) > 0.002f) return true;
    return false;
}

juce::String MUProcessor::saveUserPreset (const juce::String& scope, const juce::String& typedName, bool replaceExisting, bool* ok)
{
    if (ok != nullptr) *ok = false;
    UserPreset p;
    p.scope = scope; p.name = UserPresets::cleanName (typedName); p.pluginVersion = MU_VERSION_STRING;
    if (p.name.isEmpty()) return "Give the preset a name first.";
    if (isFactoryName (scope, p.name)) return "\"" + p.name + "\" is the name of a built-in preset. Choose a name of your own.";
    const auto ids = scopeParams (scope);
    for (auto& id : ids) if (apvts.getParameter (id) != nullptr) p.values.push_back ({ id, getParam (id) });
    // anything with thresholds in it remembers how loud the track was, so they can follow a quieter or louder track later
    if ((scope == "chain" || scope == "eq" || scope == "dynamics" || scope == "tone") && loudness.getShortTerm() > -60.0f) p.level = toolLevel();
    const auto result = UserPresets::save (p, replaceExisting);
    if (result.failed()) return result.getErrorMessage();
    if (scope == "chain") { currentPreset = p.name; apvts.state.setProperty ("chainPresetName", currentPreset, nullptr); }
    else apvts.state.setProperty ("preset_" + scope, p.name, nullptr);
    rememberLoaded (scope, ids);
    if (ok != nullptr) *ok = true;
    return "Saved \"" + p.name + "\" to your presets. It is a file of its own, so it is there in every song.";
}

juce::String MUProcessor::applyUserPreset (const UserPreset& preset)
{
    const auto allowed = scopeParams (preset.scope);
    if (allowed.empty()) return "That preset is for a tool this version of the plugin does not have.";
    // a chain saved before the FX order could be changed was made with the standard order
    if (preset.scope == "chain") setParam (ids::fxOrder, 0.0f);
    int set = 0, skipped = preset.skipped;
    for (auto& v : preset.values)
    {
        auto* param = apvts.getParameter (v.id);
        // a preset can only move the controls of what it says it is for
        if (param == nullptr || std::find (allowed.begin(), allowed.end(), v.id) == allowed.end()) { ++skipped; continue; }
        const auto range = param->getNormalisableRange();
        setParam (v.id, juce::jlimit (range.start, range.end, v.value));
        ++set;
    }
    if (set == 0) return "Nothing in that preset matches this version of the plugin.";
    juce::String note;
    // Thresholds were set for the track the preset was saved on. If this one is louder or quieter, move the ones
    // this preset holds by the difference, so the compressor, de-esser, gate and rider work as hard as they did there.
    if (preset.level > -200.0f && loudness.getShortTerm() > -60.0f)
    {
        const float shift = toolLevel() - preset.level;
        if (std::abs (shift) > 1.0f)
        {
            int moved = 0;
            auto move = [&] (const juce::String& id, float lo, float hi)
            {
                if (std::find (allowed.begin(), allowed.end(), id) == allowed.end()) return;
                setParam (id, juce::jlimit (lo, hi, getParam (id) + shift)); ++moved;
            };
            move (ids::compThreshold, -60.0f, 0.0f); move (ids::dsThreshold, -60.0f, 0.0f); move (ids::gateThreshold, -80.0f, 0.0f); move (ids::riderTarget, -40.0f, -6.0f);
            for (int b = 0; b < kNumEQBands; ++b) move (ids::eqBandThr (b), -60.0f, 0.0f);
            if (moved > 0)
                note = " This track is " + juce::String (std::abs (shift), 1) + " dB " + (shift > 0.0f ? "louder" : "quieter") + " than the one it was saved on, so the thresholds were moved by that much.";
        }
    }
    if (preset.scope == "chain")
    {
        currentPreset = preset.name;
        apvts.state.setProperty ("chainPresetName", currentPreset, nullptr);
        for (auto* tab : { "eq", "dynamics", "tone", "master" }) apvts.state.removeProperty ("preset_" + juce::String (tab), nullptr);
        restartSpectrumAverage();
        resetMeasurement();
    }
    else
    {
        apvts.state.setProperty ("preset_" + preset.scope, preset.name, nullptr);
        if (preset.scope == "eq") restartSpectrumAverage();
    }
    rememberLoaded (preset.scope, allowed);
    if (skipped > 0) note << " " << skipped << (skipped == 1 ? " setting in it is" : " settings in it are") << " for controls this version does not have and " << (skipped == 1 ? "was" : "were") << " left out.";
    return "\"" + preset.name + "\" loaded (one of your presets)." + note;
}

juce::String MUProcessor::loadUserPreset (const juce::String& scope, const juce::String& name)
{
    UserPreset p;
    if (! UserPresets::find (scope, name, p)) return "That preset is no longer in your presets folder.";
    return applyUserPreset (p);
}

juce::String MUProcessor::deleteUserPreset (const juce::String& scope, const juce::String& name)
{
    UserPreset p;
    if (! UserPresets::find (scope, name, p)) return "That preset is already gone.";
    const auto result = UserPresets::remove (p);
    if (result.failed()) return result.getErrorMessage();
    if (scopePresetName (scope) == name)
    {
        if (scope == "chain") { currentPreset.clear(); apvts.state.setProperty ("chainPresetName", currentPreset, nullptr); }
        else apvts.state.removeProperty ("preset_" + scope, nullptr);
        loadedPreset.erase (scope);
    }
    return "\"" + name + "\" deleted. Your settings were not changed.";
}

juce::String MUProcessor::renameUserPreset (const juce::String& scope, const juce::String& name, const juce::String& newName)
{
    UserPreset p;
    if (! UserPresets::find (scope, name, p)) return "That preset is no longer in your presets folder.";
    const auto clean = UserPresets::cleanName (newName);
    if (isFactoryName (scope, clean)) return "\"" + clean + "\" is the name of a built-in preset. Choose a name of your own.";
    const auto result = UserPresets::rename (p, clean);
    if (result.failed()) return result.getErrorMessage();
    if (scopePresetName (scope) == name)
    {
        if (scope == "chain") { currentPreset = p.name; apvts.state.setProperty ("chainPresetName", currentPreset, nullptr); }
        else apvts.state.setProperty ("preset_" + scope, p.name, nullptr);
    }
    return "Renamed to \"" + p.name + "\".";
}

juce::String MUProcessor::importUserPreset (const juce::File& file, bool load)
{
    UserPreset p;
    const auto result = UserPresets::importFile (file, p);
    if (result.failed()) return result.getErrorMessage();
    if (scopeParams (p.scope).empty()) { UserPresets::remove (p); return "That preset is for a tool this version of the plugin does not have."; }
    const juce::String what = p.scope == "chain" ? juce::String ("a whole-chain preset") : "a preset for " + p.scope;
    return "Imported \"" + p.name + "\", " + what + ". " + (load ? applyUserPreset (p) : juce::String ("It is in your presets now."));
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
        autoLevelHeld.store (false); masterLevelSet.store (false);
        loudness.resetIntegrated();
        loudness.resetPeakHold();
    }
}

// ---- Mastering assistant -------------------------------------------------------------------------
MasterFacts MUProcessor::collectFacts (const MasterProbe& probe, const LoudnessMeter& meter, const SpectrumAnalyzer& analyser) const
{
    MasterFacts f;
    f.probe = probe.read();
    f.integratedLufs = meter.getIntegrated();
    f.truePeakDb = meter.getTruePeakHold();
    f.hasBands = analyser.listenBands (f.bandsDb, f.swingDb);
    return f;
}

float MUProcessor::masteringSeconds() const
{
    const auto stage = getMasterStage();
    return stage == MasterStage::Listening ? inProbe.secondsHeard() : stage == MasterStage::Checking ? outProbe.secondsHeard() : 0.0f;
}

juce::String MUProcessor::startMastering()
{
    if (! isMasterInstance()) return "Mastering works on the plugin that sits on the Stereo Out. Set this one to Master (Settings, or the button on this tab) and try again.";
    referencePass = false;
    outProbe.stop();
    inMeter.reset(); inMeter.resetIntegrated(); inMeter.resetPeakHold();
    preSpectrum.startListen (true);
    inProbe.start();
    masterStage.store ((int) MasterStage::Listening);
    return "Listening to the mix. Play the song from the start, all the way through if you can: at least " + juce::String ((int) kMasterMinSeconds) + " seconds of the loudest part. Then press Master it.";
}

void MUProcessor::cancelMastering()
{
    inProbe.stop(); outProbe.stop(); preSpectrum.stopListen();
    referencePass = false;
    masterStage.store ((int) (masterText ("processing").isEmpty() ? MasterStage::Idle : MasterStage::Done));
}

juce::String MUProcessor::finishMastering()
{
    if (getMasterStage() != MasterStage::Listening || referencePass) return "Press Analyse and master first, and play the song.";
    const float heard = inProbe.secondsHeard();
    if (heard < kMasterMinSeconds) return "Only " + juce::String (heard, 0) + " s of sound heard so far. Keep the song playing: a decision needs at least " + juce::String ((int) kMasterMinSeconds) + " s.";
    inMeter.readIntegrated();
    const auto facts = collectFacts (inProbe, inMeter, preSpectrum);
    inProbe.stop(); preSpectrum.stopListen();
    if (! facts.enough()) { masterStage.store ((int) MasterStage::Idle); return "The mix could not be measured (too little sound, or too quiet). Play the song and try again."; }
    mixFacts = facts;
    masterUndo = captureSnapshot();
    return restyleMaster();
}

juce::String MUProcessor::restyleMaster()
{
    if (! mixFacts.enough()) return "Nothing has been measured yet. Press Analyse and master and play the song.";
    const auto style = (MasterStyle) juce::jlimit (0, (int) MasterStyle::Count - 1, (int) getParam (ids::mstStyle));
    masterPlan = planMaster (mixFacts, style, getParam (ids::mstIntensity) * 0.01f, refFacts.enough() ? &refFacts : nullptr);
    applyMasterPlan (masterPlan);
    masterFacts = {};
    storeMasterText();
    settleStarted = (double) samplesProcessed.load() / juce::jmax (1.0, getSampleRate());
    settlePhase = 0; inProbe.stop(); outProbe.stop();
    masterStage.store ((int) MasterStage::Settling);
    return "Mastered as " + masterStyleNames()[(int) style] + " from " + juce::String (mixFacts.probe.seconds, 0) + " s of the song: " + juce::String (masterPlan.processing.size())
         + " processors in use. Keep it playing: the level settles, then the master is measured for the report.";
}

void MUProcessor::applyMasterPlan (const MasterPlan& plan)
{
    // EQ: the corrective, tonal and dynamic bands, in that order, from band 1
    resetEQ();
    for (int b = 0; b < kNumEQBands && b < (int) plan.eq.size(); ++b) setBandSettings (b, plan.eq[(size_t) b]);
    setParam (ids::eqEnabled, plan.eq.empty() ? 0.0f : 1.0f);
    // glue
    setParam (ids::compEnabled, plan.glue ? 1.0f : 0.0f);
    if (plan.glue)
    {
        setParam (ids::compStyle, 3.0f); setParam (ids::compRatio, plan.glueRatio); setParam (ids::compThreshold, plan.glueThresholdDb);
        setParam (ids::compAttack, 30.0f); setParam (ids::compRelease, 200.0f); setParam (ids::compKnee, 10.0f); setParam (ids::compScHp, 100.0f);
        setParam (ids::compAutoMakeup, 1.0f); setParam (ids::compMix, 100.0f);
    }
    // three bands
    setParam (ids::mbOn, plan.multiband.enabled ? 1.0f : 0.0f);
    setParam (ids::mbCrossLow, plan.multiband.crossLowHz); setParam (ids::mbCrossHigh, plan.multiband.crossHighHz);
    for (int b = 0; b < 3; ++b)
    {
        const auto& band = plan.multiband.band[(size_t) b];
        setParam (ids::mbBandOn (b), band.on ? 1.0f : 0.0f);
        if (band.on) { setParam (ids::mbThreshold (b), band.thresholdDb); setParam (ids::mbRatio (b), band.ratio); setParam (ids::mbAttack (b), band.attackMs); setParam (ids::mbRelease (b), band.releaseMs); setParam (ids::mbMakeup (b), band.makeupDb); }
    }
    // saturation, stereo, clipper
    setParam (ids::satOn, plan.saturate ? 1.0f : 0.0f);
    if (plan.saturate) { setParam (ids::satType, (float) plan.satType); setParam (ids::satDrive, plan.satDriveDb); setParam (ids::satMix, plan.satMix * 100.0f); }
    setParam (ids::stMonoBass, plan.monoBass ? 1.0f : 0.0f);
    if (plan.monoBass) setParam (ids::stMonoFreq, plan.monoBelowHz);
    setParam (ids::stWidth, plan.width * 100.0f);
    setParam (ids::clipOn, plan.clip ? 1.0f : 0.0f);
    setParam (ids::clipAmount, plan.clipAmount * 100.0f); setParam (ids::clipSoft, plan.clipSoftness * 100.0f);
    // loudness: the limiter on, the level found by the auto level, starting from the gain the measurement says is needed
    setParam (ids::masterMode, 3.0f);
    setParam (ids::targetLufs, plan.targetLufs); setParam (ids::targetTruePeak, plan.ceilingDb);
    setParam (ids::maxGain, 0.0f);
    setParam (ids::peakGuard, 1.0f); setParam (ids::masterAutoTrim, 1.0f);
    masterTrimDb.store (juce::jlimit (-12.0f, 12.0f, plan.targetLufs - mixFacts.integratedLufs));
    autoLevelHeld.store (false);
    masterLevelTarget.store (plan.targetLufs); masterLevelSet.store (true);
    loudness.resetIntegrated(); loudness.resetPeakHold();
    restartSpectrumAverage();
}

// The words of the last decision travel with the project, so they are still there when it is opened again.
void MUProcessor::storeMasterText()
{
    apvts.state.setProperty ("mstDetected", masterPlan.detected.joinIntoString ("\n"), nullptr);
    apvts.state.setProperty ("mstProcessing", masterPlan.processing.joinIntoString ("\n"), nullptr);
    apvts.state.setProperty ("mstLeft", masterPlan.leftAlone.joinIntoString ("\n"), nullptr);
    apvts.state.setProperty ("mstNotes", masterPlan.notes.joinIntoString ("\n"), nullptr);
    apvts.state.setProperty ("mstReport", masterFacts.enough() ? masterReport (mixFacts, masterFacts, masterPlan).joinIntoString ("\n") : juce::String(), nullptr);
}

juce::StringArray MUProcessor::masterText (const juce::String& part) const
{
    const char* key = part == "detected" ? "mstDetected" : part == "processing" ? "mstProcessing" : part == "left" ? "mstLeft" : part == "notes" ? "mstNotes" : "mstReport";
    juce::StringArray lines;
    lines.addLines (apvts.state.getProperty (key, juce::String()).toString());
    lines.removeEmptyStrings();
    return lines;
}

juce::String MUProcessor::undoMaster()
{
    if (masterUndo.empty()) return "There is nothing to undo.";
    applySnapshot (masterUndo);
    masterUndo.clear();
    inProbe.stop(); outProbe.stop();
    for (auto* key : { "mstDetected", "mstProcessing", "mstLeft", "mstNotes", "mstReport" }) apvts.state.removeProperty (key, nullptr);
    masterStage.store ((int) MasterStage::Idle);
    masterTrimDb.store (0.0f); autoLevelHeld.store (false); masterLevelSet.store (false);
    loudness.resetIntegrated(); loudness.resetPeakHold();
    return "Back to how everything was before Analyse and master.";
}

// ---- Finish ----------------------------------------------------------------------------------------------
// One button for a vocal or a two-track beat. It listens to twelve seconds of the track as it leaves the plugin,
// compares what it hears with what was measured on the reference records (analysis/VocalReference.h), changes
// only what is outside what those records do, listens again, corrects once or twice, and says what it did.
// Everything it sets is an ordinary control. It is rules worked from measurements, not a trained model.
namespace
{
    juce::String oneDp (float v) { return juce::String (v, 1); }
}

// Sets the de-esser when the loudest esses stand further above the vowels than on any of the records. The threshold
// comes from the loudest ess the de-esser itself has heard, so it is in the de-esser's own units.
bool MUProcessor::fitDeEsserTo (float essDb)
{
    const auto ref = vocalReference();
    const float top = deEsser.getLoudestEssDb();
    if (essDb <= ref.essMost + 0.5f || top < -90.0f) return false;
    const float need = juce::jlimit (2.0f, 6.0f, essDb - ref.essDb);
    if (getParam (ids::dsOn) < 0.5f) setParam (ids::dsFreq, 6500.0f);
    setParam (ids::dsRange, juce::jlimit (3.0f, 8.0f, need + 1.0f));
    setParam (ids::dsThreshold, juce::jlimit (-60.0f, 0.0f, top - 3.0f - need / 1.5f));
    setParam (ids::dsOn, 1.0f);
    return true;
}

juce::String MUProcessor::startFinish()
{
    const Role role = getEffectiveRole();
    const bool voice = role == Role::Vocal || role == Role::BackingVocal;
    if (! voice && role != Role::Beat)
        return "Finish works on a lead or backing vocal and on a two-track beat. This track is set as " + roleNames()[(int) role] + ".";
    finishBeat = ! voice; finishDid.clear(); finishSetReverb = finishSetTamer = finishSetWidth = finishSetEss = finishReverbWasOff = finishSetDelay = false; finishRound = 0;
    deEsser.resetLoudestEss();
    vocalProbe.start();
    finishStage.store ((int) FinishStage::Listening);
    return voice ? "Listening. Keep the song playing where the vocal is going: twelve seconds of it are needed."
                 : "Listening. Keep the song playing: twelve seconds of the beat are needed.";
}

void MUProcessor::cancelFinish()
{
    vocalProbe.stop();
    finishStage.store ((int) FinishStage::Idle);
}

void MUProcessor::finishTick()
{
    const auto stage = getFinishStage();
    if (stage == FinishStage::Listening)
    {
        const auto f = vocalProbe.read();
        if (f.seconds < kFinishListenSeconds) return;
        finishBefore = f;
        if (finishBeat) applyBeatFinish (f); else applyVocalFinish (f);
        vocalProbe.start();
        finishStage.store ((int) FinishStage::Checking);
    }
    else if (stage == FinishStage::Checking)
    {
        const auto f = vocalProbe.read();
        if (f.seconds < kFinishCheckSeconds) return;
        endFinish (f);
    }
}

void MUProcessor::applyVocalFinish (const VocalFacts& f)
{
    const auto ref = vocalReference();
    // 1. The loudest syllables. (Ten or twelve seconds hold few of them, so a short listen reads a little low:
    //    it acts from just above the middle one of the records, gently, and the second listen decides the rest.)
    if (f.peakSpreadDb > ref.peakSpreadDb + 0.4f)
    {
        setParam (ids::tameAmount, 100.0f * juce::jlimit (0.15f, 0.9f, (f.peakSpreadDb - ref.peakSpreadDb) / 5.5f));
        setParam (ids::tameOn, 1.0f);
        finishSetTamer = true;
    }
    // 2. Esses that stand out more than on any of the records.
    finishSetEss = fitDeEsserTo (f.essDb);
    // 3. Space and width: a vocal with less at its sides than any of the records is dry and in the middle. On the
    //    records what is left a quarter to half a second after a phrase is only about 21 dB under the phrase,
    //    which a short reverb cannot do by itself: that is an echo as well. So the space is a short reverb, as
    //    the reverb advice chooses it, plus a quiet quarter-note echo out at the sides, and it is the echo that is
    //    turned up or down to reach the width of the records.
    if (f.sideDb < ref.sideNarrowest - 2.0f)
    {
        if (getParam (ids::rvOn) < 0.5f)
        {
            fitReverb();
            setParam (ids::rvDuck, juce::jmin (getParam (ids::rvDuck), 50.0f));
            finishReverbWasOff = getParam (ids::rvOn) > 0.5f;
        }
        if (getParam (ids::dlOn) < 0.5f)
        {
            setParam (ids::dlSync, 1.0f); setParam (ids::dlNote, 1.0f); setParam (ids::dlPattern, 0.0f); setParam (ids::dlStyle, 0.0f);
            setParam (ids::dlFeedback, 22.0f); setParam (ids::dlWidth, 100.0f); setParam (ids::dlDuck, 55.0f); setParam (ids::dlTone, 40.0f);
            setParam (ids::dlLowCut, 250.0f); setParam (ids::dlMix, 8.0f);
            setParam (ids::dlOn, 1.0f);
            finishSetDelay = true;
        }
        finishSetReverb = true;
        if (! isStereoOut()) finishDid.add ("This track is mono in the host, so the echo and the reverb cannot reach the sides. Put the plugin on a stereo track for the width.");
    }
    // 4. The level against the rest of the song, for a lead vocal with other tracks to measure it against.
    if (getEffectiveRole() == Role::Vocal)
    {
        float mine = -100.0f; double rest = 0.0; int others = 0;
        for (auto& t : bus->collectTracks())
        {
            if (! t.active || t.levelLufs < -60.0f) continue;
            if (t.slot == slot) { mine = t.levelLufs; continue; }
            if (t.role == Role::Master || t.role == Role::Vocal || t.role == Role::BackingVocal) continue;
            rest += std::pow (10.0, 0.1 * (double) t.levelLufs); ++others;
        }
        if (others > 0 && mine > -60.0f)
        {
            const float over = (float) (10.0 * std::log10 (rest)) - mine;
            if (! acceptsBalance())
                finishDid.add ("Level: the rest of the song is " + oneDp (over) + " LU over the vocal (" + oneDp (ref.beatOverVocalLeast) + " to " + oneDp (ref.beatOverVocalMost) + " on the records). This track is locked on the Mix tab, so its level was left.");
            else if (over < ref.beatOverVocalLeast - 0.3f || over > ref.beatOverVocalMost + 0.3f)
            {
                const float move = juce::jlimit (-6.0f, 6.0f, over - ref.beatOverVocalLu);
                setTrimDb (getTrimDb() + move);
                finishDid.add ("Level: the rest of the song was " + oneDp (std::abs (over)) + (over >= 0.0f ? " LU over" : " LU under") + " the vocal (" + oneDp (ref.beatOverVocalLeast) + " to " + oneDp (ref.beatOverVocalMost)
                               + " LU over on the records). The vocal was turned " + (move > 0.0f ? "up" : "down") + " " + oneDp (std::abs (move)) + " dB.");
            }
        }
        else finishDid.add ("Level: no other track with the plugin on it was heard, so the vocal's level against the beat was not checked.");
    }
}

void MUProcessor::applyBeatFinish (const VocalFacts& f)
{
    const auto ref = vocalReference();
    if (! isStereoOut() || f.bandSideDb < -35.0f)
        finishDid.add ("This beat is mono: there is nothing at its sides to turn up, and widening cannot invent it. Ask for the stereo bounce of the beat.");
    else if (f.bandSideDb < ref.beatBandSideNarrowest - 3.0f)
    {
        const float lift = juce::jmin (6.0f, ref.beatBandSideNarrowest - f.bandSideDb);
        setParam (ids::stWidth, juce::jmax (getParam (ids::stWidth), juce::jlimit (100.0f, 200.0f, 100.0f * std::pow (10.0f, lift / 20.0f))));
        setParam (ids::stMonoBass, 1.0f);
        finishSetWidth = true;
    }
}

void MUProcessor::endFinish (const VocalFacts& after)
{
    const auto ref = vocalReference();
    // One or two corrections from what was heard after the change, then a second listen.
    bool again = false;
    if (! finishBeat)
    {
        if (! finishSetTamer && after.peakSpreadDb > ref.peakSpreadMost + 0.4f && getParam (ids::tameOn) < 0.5f) { setParam (ids::tameAmount, 30.0f); setParam (ids::tameOn, 1.0f); finishSetTamer = true; again = true; }
        else if (finishSetTamer && after.peakSpreadDb > ref.peakSpreadMost + 0.4f && getParam (ids::tameAmount) < 99.0f) { setParam (ids::tameAmount, juce::jmin (100.0f, getParam (ids::tameAmount) + 20.0f)); again = true; }
        else if (finishSetTamer && after.peakSpreadDb < ref.peakSpreadLeast - 0.4f && getParam (ids::tameAmount) > 12.0f) { setParam (ids::tameAmount, juce::jmax (10.0f, getParam (ids::tameAmount) - 15.0f)); again = true; }
        // the space: towards the middle one of the records, not just inside their range
        if (finishSetReverb && isStereoOut())
        {
            const float step = std::pow (10.0f, juce::jlimit (-6.0f, 6.0f, ref.sideDb - after.sideDb) / 20.0f);
            const bool low = after.sideDb < ref.sideDb - 2.0f, high = after.sideDb > ref.sideDb + 3.0f;
            const float echo = getParam (ids::dlMix), verb = getParam (ids::rvMix);
            if (finishSetDelay && ((low && echo < 17.5f) || (high && echo > 3.0f))) { setParam (ids::dlMix, juce::jlimit (2.0f, 18.0f, echo * step)); again = true; }
            else if ((low && verb < 23.5f) || (high && verb > 5.0f))                { setParam (ids::rvMix, juce::jlimit (4.0f, 24.0f, verb * step)); again = true; }
        }
        // holding the loud syllables leaves the esses standing higher against them: looked at again here
        if (! finishSetEss && fitDeEsserTo (after.essDb)) { finishSetEss = true; again = true; }
    }
    else if (finishSetWidth && isStereoOut() && stereo.getCorrelation() < 0.2f && getParam (ids::stWidth) > 110.0f)
    {
        // wide enough that the two sides hardly agree: it would lose level in mono, so half of the widening is taken back
        setParam (ids::stWidth, 100.0f + 0.5f * (getParam (ids::stWidth) - 100.0f)); again = true;
    }
    if (again && finishRound < 3)
    {
        ++finishRound;
        vocalProbe.start();
        return;                                   // still Checking: listen once more
    }
    vocalProbe.stop();

    juce::StringArray text;
    const auto& b = finishBefore;
    auto side = [] (float db) { return db < -60.0f ? juce::String ("nothing at the sides") : juce::String (-db, 1) + " dB below the middle"; };
    if (! finishBeat)
    {
        text.add ("Measured against the vocals of the five reference records, on " + juce::String (juce::roundToInt (b.seconds)) + " s of this vocal.");
        text.add ("Loudest syllables above the usual level: " + oneDp (b.peakSpreadDb) + " dB, now " + oneDp (after.peakSpreadDb) + " (records " + oneDp (ref.peakSpreadLeast) + " to " + oneDp (ref.peakSpreadMost) + ").");
        text.add ("Sides of the vocal: " + side (b.sideDb) + ", now " + side (after.sideDb) + " (records " + oneDp (-ref.sideWidest) + " to " + oneDp (-ref.sideNarrowest) + ").");
        if (b.essDb > -99.0f && after.essDb > -99.0f)
            text.add ("Loudest esses against the vowels: " + oneDp (b.essDb) + " dB, now " + oneDp (after.essDb) + " (records up to " + oneDp (ref.essMost) + ").");
    }
    else
    {
        text.add ("Measured against the instrumentals of the five reference records, on " + juce::String (juce::roundToInt (b.seconds)) + " s of this beat.");
        text.add ("Sides between 800 Hz and 5 kHz: " + side (b.bandSideDb) + ", now " + side (after.bandSideDb) + " (records " + oneDp (-ref.beatBandSideDb) + ", the narrowest " + oneDp (-ref.beatBandSideNarrowest) + ").");
        if (after.bandSideDb > -35.0f && after.bandSideDb < ref.beatBandSideNarrowest - 3.0f)
            text.add ("It is still narrower than the records: a width control only turns up what is already at the sides. The rest is in how the beat was made.");
    }
    // what was set, as it stands now (the corrections included)
    if (finishSetTamer) finishDid.insert (0, "Syllable peaks switched on at " + juce::String (juce::roundToInt (getParam (ids::tameAmount))) + " % (Tone tab, with the level rider).");
    if (finishSetEss)   finishDid.add ("De-esser set to take up to " + oneDp (getParam (ids::dsRange)) + " dB off the loudest esses (Dynamics tab).");
    if (finishSetReverb && finishReverbWasOff) finishDid.add ("Reverb switched on and fitted to this track, at " + juce::String (juce::roundToInt (getParam (ids::rvMix))) + " % (FX tab).");
    else if (finishSetReverb && ! finishSetDelay) finishDid.add ("Reverb set to " + juce::String (juce::roundToInt (getParam (ids::rvMix))) + " % (FX tab).");
    if (finishSetDelay) finishDid.add ("Quarter-note echo switched on out at the sides, at " + juce::String (juce::roundToInt (getParam (ids::dlMix))) + " % (FX tab).");
    if (finishSetWidth) finishDid.add ("Width set to " + juce::String (juce::roundToInt (getParam (ids::stWidth))) + " % with the low end kept in the middle (Tone tab).");
    if (finishDid.isEmpty()) text.add (finishBeat ? "Nothing was changed: this beat is already as wide as the records." : "Nothing was changed: this vocal is already inside what the records do.");
    else { text.add ("Changed:"); for (auto& d : finishDid) text.add ("- " + d); }
    apvts.state.setProperty ("finishText", text.joinIntoString ("\n"), nullptr);
    finishStage.store ((int) FinishStage::Done);
}

// How much louder the master came out than the mix went in over the measurement that has just run: from the loud
// passages if enough of them were heard, otherwise from everything that was. -100 when nothing usable was heard.
float MUProcessor::masterGainHeard() const
{
    const double in = gateIn.load(), out = gateOut.load();
    if (gateSeconds.load() >= 8.0 && in > 0.0 && out > 0.0) return (float) (10.0 * std::log10 (out / in));
    const float i = inMeter.getIntegrated(), o = outMeter.getIntegrated();
    return i > -60.0f && o > -60.0f ? o - i : -100.0f;
}

void MUProcessor::masterTick()
{
    const auto stage = getMasterStage();
    if (stage == MasterStage::Listening) inMeter.readIntegrated();
    const double now = (double) samplesProcessed.load() / juce::jmax (1.0, getSampleRate());
    if (stage == MasterStage::Settling && isMasterInstance())
    {
        // The level starts at the gain the measurement of the whole song says is needed. What the EQ, the
        // compression and the limiter then add or take away is measured here: the mix going in and the master
        // coming out at the same moments. Only the loud passages are counted (within 10 LU of the whole song's
        // loudness), because those are what the loudness of the whole song is made of and where the compressors
        // and the limiter work: measured on a quiet intro, the master looks louder than it will be. The level is
        // corrected by the difference, once here and once more after the longer check, and then left alone.
        auto measureBoth = [this]
        {
            inMeter.reset(); inMeter.resetIntegrated(); inMeter.resetPeakHold();
            outMeter.reset(); outMeter.resetIntegrated(); outMeter.resetPeakHold();
            gateIn.store (0.0); gateOut.store (0.0); gateSeconds.store (0.0);
            gateFloorLufs.store (mixFacts.enough() ? mixFacts.integratedLufs - 10.0f : -70.0f);
            inProbe.start(); outProbe.start();
            gateOn.store (true);
        };
        const double waited = now - settleStarted;
        if (settlePhase == 0 && waited > 3.0) { measureBoth(); settlePhase = 1; }
        else if (settlePhase == 1)
        {
            inMeter.readIntegrated(); outMeter.readIntegrated();
            const bool enoughLoud = gateSeconds.load() >= 12.0, givenUp = outProbe.secondsHeard() >= 45.0f;
            if (enoughLoud || givenUp)
            {
                gateOn.store (false);
                const float gain = masterGainHeard();
                float worst = 0.0f;
                for (int i = 0; i < 250; ++i) worst = juce::jmin (worst, maximizer.reductionHistory().get (i));
                if (gain > -90.0f && mixFacts.enough())
                {
                    float fix = juce::jlimit (-4.0f, 4.0f, (masterLevelTarget.load() - mixFacts.integratedLufs) - gain);
                    if (fix > 0.0f && worst < -8.0f) fix = 0.0f;          // never chased by crushing the mix
                    if (std::abs (fix) > 0.2f) masterTrimDb.store (juce::jlimit (-24.0f, 24.0f, masterTrimDb.load() + fix));
                }
                inProbe.stop(); outProbe.stop();
                settlePhase = 2; settleStarted = now;
            }
        }
        else if (settlePhase == 2 && waited > 2.0)
        {
            measureBoth();
            spectrum.startListen (true);
            settlePhase = 0;
            masterStage.store ((int) MasterStage::Checking);
        }
    }
    if (stage == MasterStage::Checking)
    {
        outMeter.readIntegrated(); inMeter.readIntegrated();
        if (outProbe.secondsHeard() >= kMasterCheckSeconds)
        {
            gateOn.store (false);
            masterFacts = collectFacts (outProbe, outMeter, spectrum);
            // The passage that was checked is not the whole song. What it shows for certain is how much louder the
            // master is than the mix, so the loudness in the report is the whole song's own loudness plus that.
            // If that is still off the target the level is corrected a second time, by the remainder.
            if (const float gain = masterGainHeard(); gain > -90.0f && mixFacts.enough())
            {
                masterFacts.integratedLufs = mixFacts.integratedLufs + gain;
                float worst = 0.0f;
                for (int i = 0; i < 250; ++i) worst = juce::jmin (worst, maximizer.reductionHistory().get (i));
                float fix = juce::jlimit (-2.0f, 2.0f, masterLevelTarget.load() - masterFacts.integratedLufs);
                if (fix > 0.0f && worst < -8.0f) fix = 0.0f;
                if (std::abs (fix) > 0.3f && masterLevelSet.load())
                {
                    masterTrimDb.store (juce::jlimit (-24.0f, 24.0f, masterTrimDb.load() + fix));
                    masterFacts.integratedLufs += fix;
                }
            }
            inProbe.stop();
            outProbe.stop(); spectrum.stopListen();
            storeMasterText();
            masterStage.store ((int) MasterStage::Done);
        }
    }
    // the comparison: how far the master has to come down to be as loud as the mix was
    const float mixLufs = mixFacts.enough() ? mixFacts.integratedLufs : mixMeter.getIntegrated();
    const float nowLufs = masterFacts.enough() ? masterFacts.integratedLufs : loudness.getIntegrated();
    compareGainDb.store (mixLufs > -70.0f && nowLufs > -70.0f ? juce::jlimit (-24.0f, 0.0f, mixLufs - nowLufs) : 0.0f);
}

juce::String MUProcessor::startReferencePass()
{
    if (! isMasterInstance()) return "The reference is played through the plugin on the Stereo Out.";
    if (getMasterStage() == MasterStage::Listening && ! referencePass) return "The mix is being listened to. Finish that first.";
    referencePass = true;
    inMeter.reset(); inMeter.resetIntegrated(); inMeter.resetPeakHold();
    preSpectrum.startListen (true);
    inProbe.start();
    masterStage.store ((int) MasterStage::Listening);
    return "Listening to the reference. Solo the reference track so only it reaches the Stereo Out, play at least " + juce::String ((int) kMasterMinSeconds) + " s of it, then press Use as reference.";
}

juce::String MUProcessor::finishReferencePass()
{
    if (! referencePass) return "Press Play a reference first.";
    const float heard = inProbe.secondsHeard();
    if (heard < kMasterMinSeconds) return "Only " + juce::String (heard, 0) + " s of the reference heard so far. Keep it playing.";
    inMeter.readIntegrated();
    const auto facts = collectFacts (inProbe, inMeter, preSpectrum);
    inProbe.stop(); preSpectrum.stopListen();
    referencePass = false;
    masterStage.store ((int) (masterText ("processing").isEmpty() ? MasterStage::Idle : MasterStage::Done));
    if (! facts.enough()) return "The reference could not be measured. Play it louder or for longer.";
    refFacts = facts; refName = "the track you played";
    return "Reference measured: " + juce::String (facts.integratedLufs, 1) + " LUFS, true peak " + juce::String (facts.truePeakDb, 1) + " dBTP, loudness range " + juce::String (facts.probe.loudnessRangeLu, 1)
         + " LU. Un-solo it, then press Analyse and master (or Decide again) to lean the master towards it.";
}

juce::String MUProcessor::loadReferenceFile (const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr) return "That file could not be read. WAV, AIFF and FLAC always work; MP3 and M4A depend on the computer.";
    const auto length = (int) juce::jmin ((juce::int64) (reader->sampleRate * 600.0), reader->lengthInSamples);        // ten minutes at most
    if (length < (int) (reader->sampleRate * (double) kMasterMinSeconds)) return "That file is shorter than " + juce::String ((int) kMasterMinSeconds) + " seconds: too little to measure.";
    juce::AudioBuffer<float> audio (2, length);
    reader->read (&audio, 0, length, 0, true, true);
    if (reader->numChannels < 2) audio.copyFrom (1, 0, audio, 0, 0, length);
    const auto facts = measureMix (audio, reader->sampleRate);
    if (! facts.enough()) return "The reference could not be measured (it may be silent).";
    refFacts = facts; refName = file.getFileName();
    return "Reference \"" + refName + "\": " + juce::String (facts.integratedLufs, 1) + " LUFS, true peak " + juce::String (facts.truePeakDb, 1) + " dBTP, loudness range " + juce::String (facts.probe.loudnessRangeLu, 1)
         + " LU. Press Analyse and master (or Decide again) to lean the master towards it.";
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
    state.setProperty ("uiTabs", 8, nullptr);
    state.setProperty ("trimAfter", ! trimBeforeChain.load(), nullptr);
    // the level the master has arrived at, so that a project that is opened again (or bounced straight away)
    // starts at that level and does not have to find it again over the first seconds of the song
    if (isMasterInstance() && std::abs (masterTrimDb.load()) > 0.001f)
    {
        state.setProperty ("masterLevelDb", masterTrimDb.load(), nullptr);
        if (masterLevelSet.load()) state.setProperty ("masterLevelFor", masterLevelTarget.load(), nullptr); else state.removeProperty ("masterLevelFor", nullptr);
    }
    else { state.removeProperty ("masterLevelDb", nullptr); state.removeProperty ("masterLevelFor", nullptr); }
    {
        const juce::ScopedLock sl (graphLock);
        const auto notes = graph.toString();
        if (notes.isNotEmpty()) state.setProperty ("tuneGraph", notes, nullptr); else state.removeProperty ("tuneGraph", nullptr);
    }
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
            // A song saved before these controls existed was tuned without them: keep it that way.
            bool hasSlides = false;
            for (int i = 0; i < tree.getNumChildren(); ++i) hasSlides = hasSlides || tree.getChild (i).getProperty ("id").toString() == ids::tuneSlides;
            if (! hasSlides) { setParam (ids::tuneFlex, 0.0f); setParam (ids::tuneHumanise, 0.0f); setParam (ids::tuneSlides, 0.0f); setParam (ids::hmHumanise, 0.0f); }
            // builds before 12 applied the balance trim in front of the processing
            trimBeforeChain.store (! (bool) tree.getProperty ("trimAfter", false) && std::abs (getTrimDb()) > 0.05f && ! isMasterInstance());
            // songs saved before Tune had its own tab counted the tabs differently
            int savedTab = (int) tree.getProperty ("uiTab", 0);
            if ((int) tree.getProperty ("uiTabs", 7) < 8 && savedTab >= 4) ++savedTab;
            currentTab.store (juce::jlimit (0, 8, savedTab));
            {
                const juce::ScopedLock sl (graphLock);
                const auto notes = tree.getProperty ("tuneGraph", juce::String()).toString();
                if (notes.isNotEmpty()) graph.fromString (notes); else graph.clear();
            }
            currentPreset = tree.getProperty ("chainPresetName", juce::String()).toString();
            abStates[0].clear(); abStates[1].clear(); abSlot = 0;
            masterTrimDb.store (juce::jlimit (-24.0f, 24.0f, (float) tree.getProperty ("masterLevelDb", 0.0)));
            masterLevelSet.store (tree.hasProperty ("masterLevelFor"));
            masterLevelTarget.store ((float) tree.getProperty ("masterLevelFor", -14.0));
            masterTrimGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (masterTrimDb.load()));
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
