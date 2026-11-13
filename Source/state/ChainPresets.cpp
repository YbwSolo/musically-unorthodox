#include "ChainPresets.h"
#include "ParameterLayout.h"

namespace mu
{
namespace
{
    struct BandSpec { bool on; EQType type; float hz; float gain; float q; bool dyn = false; float thr = -30.0f; float range = -6.0f; };

    void band (std::vector<ParamValue>& v, int b, const BandSpec& s)
    {
        v.push_back ({ ids::eqBandEnabled (b), s.on ? 1.0f : 0.0f });
        v.push_back ({ ids::eqBandType (b), (float) s.type });
        v.push_back ({ ids::eqBandFreq (b), s.hz });
        v.push_back ({ ids::eqBandGain (b), s.gain });
        v.push_back ({ ids::eqBandQ (b), s.q });
        v.push_back ({ ids::eqBandDyn (b), s.dyn ? 1.0f : 0.0f });
        v.push_back ({ ids::eqBandThr (b), s.thr });
        v.push_back ({ ids::eqBandRange (b), s.range });
    }

    void gate (std::vector<ParamValue>& v, bool on, float thr, float range, float att, float rel, float hold)
    {
        v.push_back ({ ids::gateOn, on ? 1.0f : 0.0f }); v.push_back ({ ids::gateThreshold, thr }); v.push_back ({ ids::gateRange, range });
        v.push_back ({ ids::gateAttack, att }); v.push_back ({ ids::gateRelease, rel }); v.push_back ({ ids::gateHold, hold });
    }

    void comp (std::vector<ParamValue>& v, bool on, float thr, float ratio, float att, float rel, float knee, float mix = 100.0f)
    {
        v.push_back ({ ids::compEnabled, on ? 1.0f : 0.0f }); v.push_back ({ ids::compThreshold, thr }); v.push_back ({ ids::compRatio, ratio });
        v.push_back ({ ids::compAttack, att }); v.push_back ({ ids::compRelease, rel }); v.push_back ({ ids::compKnee, knee });
        v.push_back ({ ids::compAutoMakeup, 1.0f }); v.push_back ({ ids::compMakeup, 0.0f }); v.push_back ({ ids::compMix, mix });
    }

    void deess (std::vector<ParamValue>& v, bool on, float hz, float thr, float range, int mode = 0)
    {
        v.push_back ({ ids::dsOn, on ? 1.0f : 0.0f }); v.push_back ({ ids::dsFreq, hz }); v.push_back ({ ids::dsThreshold, thr });
        v.push_back ({ ids::dsRange, range }); v.push_back ({ ids::dsMode, (float) mode });
    }

    void sat (std::vector<ParamValue>& v, bool on, float drive, float mix, int type)
    {
        v.push_back ({ ids::satOn, on ? 1.0f : 0.0f }); v.push_back ({ ids::satDrive, drive }); v.push_back ({ ids::satMix, mix }); v.push_back ({ ids::satType, (float) type });
    }

    void rider (std::vector<ParamValue>& v, bool on, float target, float range, int speed)
    {
        v.push_back ({ ids::riderOn, on ? 1.0f : 0.0f }); v.push_back ({ ids::riderTarget, target }); v.push_back ({ ids::riderRange, range }); v.push_back ({ ids::riderSpeed, (float) speed });
    }

    void space (std::vector<ParamValue>& v, bool on, float amount, int speed = 1)
    {
        v.push_back ({ ids::vsEnabled, on ? 1.0f : 0.0f }); v.push_back ({ ids::vsAmount, amount }); v.push_back ({ ids::vsSpeed, (float) speed });
    }

    void common (std::vector<ParamValue>& v, Role role)
    {
        v.push_back ({ ids::role, (float) role });
        v.push_back ({ ids::eqEnabled, 1.0f });
        v.push_back ({ ids::outputGain, 0.0f });
        v.push_back ({ ids::peakGuard, 0.0f });
        v.push_back ({ ids::masterAutoTrim, 0.0f });
    }

    std::vector<ChainPreset> build()
    {
        std::vector<ChainPreset> out;

        {   // 0: clean
            ChainPreset p { "Clean (everything off)", "Metering and session balance only. Start here if you just want levels.", Role::Auto, {} };
            auto& v = p.values;
            common (v, Role::Auto);
            gate (v, false, -50, 20, 1.5f, 120, 40);
            for (int b = 0; b < kNumEQBands; ++b) band (v, b, { false, kDefaultBands[b].type, kDefaultBands[b].freq, 0.0f, 0.71f });
            comp (v, false, -18, 3, 10, 120, 6);
            deess (v, false, 6000, -30, 6);
            sat (v, false, 6, 40, 0);
            rider (v, false, -20, 6, 1);
            space (v, false, 50);
            out.push_back (p);
        }
        {   // 1: lead vocal - gate, HP, mud control, presence, air, 76-style comp, de-ess, tape, rider
            ChainPreset p { "MU Lead Vocal", "The studio vocal chain: tight gate, high-pass, dynamic mud and harsh control, presence and air, fast 4:1 compression, de-esser, a touch of tape and a level rider.", Role::Vocal, {} };
            auto& v = p.values;
            common (v, Role::Vocal);
            gate (v, true, -48, 18, 1.0f, 150, 60);
            band (v, 0, { true, EQType::LowCut, 75, 0, 0.71f });
            band (v, 1, { true, EQType::Bell, 230, -2.0f, 1.6f, true, -26, -4 });
            band (v, 2, { true, EQType::Bell, 2200, 2.0f, 0.8f });
            band (v, 3, { true, EQType::Bell, 4200, -1.0f, 1.4f, true, -24, -4 });
            band (v, 4, { true, EQType::HighShelf, 10000, 2.0f, 0.71f });
            band (v, 5, { false, EQType::Bell, 350, 0, 1.0f });
            comp (v, true, -22, 4, 3, 80, 3);
            deess (v, true, 5500, -30, 6, 0);
            sat (v, true, 4, 35, 0);
            rider (v, true, -19, 6, 1);
            space (v, false, 50);
            out.push_back (p);
        }
        {   // 2: backing / adlib
            ChainPreset p { "MU Backing / Adlib", "Thinner and more controlled so it sits behind the lead: higher high-pass, harder compression, more de-ess, brighter top.", Role::BackingVocal, {} };
            auto& v = p.values;
            common (v, Role::BackingVocal);
            gate (v, true, -46, 20, 1.0f, 120, 50);
            band (v, 0, { true, EQType::LowCut, 130, 0, 0.71f });
            band (v, 1, { true, EQType::Bell, 300, -3.0f, 1.4f, true, -28, -4 });
            band (v, 2, { true, EQType::Bell, 1800, -1.5f, 1.2f });
            band (v, 3, { true, EQType::Bell, 4500, -1.5f, 1.6f, true, -26, -4 });
            band (v, 4, { true, EQType::HighShelf, 9000, 3.0f, 0.71f });
            band (v, 5, { false, EQType::Bell, 350, 0, 1.0f });
            comp (v, true, -24, 6, 2, 70, 2);
            deess (v, true, 6000, -32, 8, 0);
            sat (v, true, 3, 25, 0);
            rider (v, true, -24, 4, 1);
            space (v, false, 50);
            out.push_back (p);
        }
        {   // 3: 808 / bass
            ChainPreset p { "MU 808 / Bass", "Sub kept clean, mid mud pulled, harmonics added so it reads on phones, slow compression for a consistent low end, ducks for the vocal.", Role::Bass808, {} };
            auto& v = p.values;
            common (v, Role::Bass808);
            gate (v, false, -60, 20, 1.5f, 120, 40);
            band (v, 0, { true, EQType::LowCut, 26, 0, 0.71f });
            band (v, 1, { true, EQType::Bell, 55, 1.5f, 1.0f });
            band (v, 2, { true, EQType::Bell, 260, -2.0f, 1.2f, true, -24, -3 });
            band (v, 3, { false, EQType::Bell, 1000, 0, 1.0f });
            band (v, 4, { false, EQType::Bell, 3000, 0, 1.0f });
            band (v, 5, { true, EQType::HighCut, 7000, 0, 0.71f });
            comp (v, true, -20, 4, 25, 160, 6);
            deess (v, false, 6000, -30, 6);
            sat (v, true, 7, 40, 2);
            rider (v, false, -20, 6, 1);
            space (v, true, 40, 1);
            out.push_back (p);
        }
        {   // 4: kick
            ChainPreset p { "MU Kick", "Weight at 60, box cut at 400, click at 3k, punchy compression with a slow attack so the transient gets through.", Role::Kick, {} };
            auto& v = p.values;
            common (v, Role::Kick);
            gate (v, false, -60, 20, 1.5f, 120, 40);
            band (v, 0, { true, EQType::LowCut, 30, 0, 0.71f });
            band (v, 1, { true, EQType::Bell, 60, 2.0f, 1.2f });
            band (v, 2, { true, EQType::Bell, 400, -3.0f, 1.5f });
            band (v, 3, { true, EQType::Bell, 3000, 2.0f, 1.2f });
            band (v, 4, { false, EQType::Bell, 8000, 0, 1.0f });
            band (v, 5, { false, EQType::HighCut, 12000, 0, 0.71f });
            comp (v, true, -16, 4, 20, 60, 4);
            deess (v, false, 6000, -30, 6);
            sat (v, true, 3, 25, 2);
            rider (v, false, -20, 6, 1);
            space (v, true, 30, 2);
            out.push_back (p);
        }
        {   // 5: drums / percussion bus
            ChainPreset p { "MU Drums", "Low-mid control, a little top, glue compression, gets out of the way of the vocal in the presence region.", Role::Drums, {} };
            auto& v = p.values;
            common (v, Role::Drums);
            gate (v, false, -60, 20, 1.5f, 120, 40);
            band (v, 0, { true, EQType::LowCut, 40, 0, 0.71f });
            band (v, 1, { true, EQType::Bell, 300, -2.0f, 1.2f, true, -24, -3 });
            band (v, 2, { false, EQType::Bell, 1000, 0, 1.0f });
            band (v, 3, { true, EQType::Bell, 3500, -1.0f, 1.4f, true, -22, -3 });
            band (v, 4, { true, EQType::HighShelf, 8000, 1.5f, 0.71f });
            band (v, 5, { false, EQType::Bell, 12000, 0, 1.0f });
            comp (v, true, -18, 3, 15, 100, 6);
            deess (v, false, 6000, -30, 6);
            sat (v, true, 3, 20, 0);
            rider (v, false, -20, 6, 1);
            space (v, true, 50, 1);
            out.push_back (p);
        }
        {   // 6: beat (2-track)
            ChainPreset p { "MU Beat (2-track)", "For a bought beat or 2-track instrumental: gentle mud and harsh control, light glue, and Vocal Space so the vocal sits on top without turning the beat down.", Role::Beat, {} };
            auto& v = p.values;
            common (v, Role::Beat);
            gate (v, false, -60, 20, 1.5f, 120, 40);
            band (v, 0, { true, EQType::LowCut, 28, 0, 0.71f });
            band (v, 1, { true, EQType::Bell, 250, -1.5f, 1.2f, true, -20, -3 });
            band (v, 2, { false, EQType::Bell, 1000, 0, 1.0f });
            band (v, 3, { true, EQType::Bell, 2800, -1.0f, 1.2f, true, -20, -3 });
            band (v, 4, { false, EQType::HighShelf, 10000, 0, 0.71f });
            band (v, 5, { false, EQType::Bell, 12000, 0, 1.0f });
            comp (v, true, -16, 2, 30, 200, 8, 70);
            deess (v, false, 6000, -30, 6);
            sat (v, false, 4, 30, 0);
            rider (v, false, -20, 6, 1);
            space (v, true, 60, 1);
            out.push_back (p);
        }
        {   // 7: melody / keys
            ChainPreset p { "MU Melody / Keys", "Clears the low end out of the vocal's way, tames low-mid build-up, light compression.", Role::Melody, {} };
            auto& v = p.values;
            common (v, Role::Melody);
            gate (v, false, -60, 20, 1.5f, 120, 40);
            band (v, 0, { true, EQType::LowCut, 90, 0, 0.71f });
            band (v, 1, { true, EQType::Bell, 350, -2.0f, 1.2f, true, -26, -3 });
            band (v, 2, { true, EQType::Bell, 2500, -1.5f, 1.0f, true, -24, -3 });
            band (v, 3, { false, EQType::Bell, 5000, 0, 1.0f });
            band (v, 4, { true, EQType::HighShelf, 9000, 1.0f, 0.71f });
            band (v, 5, { false, EQType::Bell, 12000, 0, 1.0f });
            comp (v, true, -20, 2.5f, 20, 150, 6);
            deess (v, false, 6000, -30, 6);
            sat (v, false, 4, 30, 0);
            rider (v, false, -20, 6, 1);
            space (v, true, 55, 1);
            out.push_back (p);
        }
        {   // 8: master prep
            ChainPreset p { "MU Master Prep", "For the master bus instance: no tone changes, auto trim to the target loudness and the true-peak guard switched on.", Role::Master, {} };
            auto& v = p.values;
            common (v, Role::Master);
            gate (v, false, -60, 20, 1.5f, 120, 40);
            for (int b = 0; b < kNumEQBands; ++b) band (v, b, { false, kDefaultBands[b].type, kDefaultBands[b].freq, 0.0f, 0.71f });
            comp (v, false, -18, 1.5f, 30, 300, 10);
            deess (v, false, 6000, -30, 6);
            sat (v, false, 2, 20, 0);
            rider (v, false, -20, 6, 1);
            space (v, false, 50);
            v.push_back ({ ids::peakGuard, 1.0f });
            v.push_back ({ ids::masterAutoTrim, 1.0f });
            out.push_back (p);
        }
        return out;
    }
}

const std::vector<ChainPreset>& chainPresets()
{
    static const std::vector<ChainPreset> presets = build();
    return presets;
}

juce::StringArray chainPresetNames()
{
    juce::StringArray names;
    for (auto& p : chainPresets()) names.add (p.name);
    return names;
}
}
