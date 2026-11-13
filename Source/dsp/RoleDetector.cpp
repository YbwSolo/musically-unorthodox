#include "RoleDetector.h"

namespace mu
{
RoleGuess detectRole (const RoleFeatures& f) noexcept
{
    if (! f.valid) return { Role::Beat, 0.0f, "listening" };

    const float low = f.lowEnergyRatio, high = f.highEnergyRatio, cen = f.centroidHz, crest = f.crestFactorDb;

    if (low > 0.55f && cen < 350.0f)
    {
        if (crest > 13.0f) return { Role::Kick, juce::jlimit (0.4f, 0.95f, low), "sub-heavy, punchy transients" };
        return { Role::Bass808, juce::jlimit (0.4f, 0.95f, low), "sub-heavy, sustained" };
    }

    if (low < 0.12f && cen > 700.0f && cen < 3200.0f && crest > 7.0f && crest < 17.0f && high < 0.35f)
        return { Role::Vocal, 0.6f + juce::jmin (0.3f, (0.12f - low) * 2.0f), "mid-focused, speech-like dynamics" };

    if (high > 0.35f && low < 0.08f)
        return { Role::FX, 0.55f, "bright, little low end" };

    if (crest > 15.0f && low > 0.12f && low < 0.5f)
        return { Role::Drums, 0.5f, "transient-heavy with low end" };

    if (low < 0.2f && cen > 300.0f && cen < 2000.0f && crest < 10.0f)
        return { Role::Melody, 0.45f, "sustained mid content" };

    if (low > 0.18f && low < 0.55f && cen > 150.0f && cen < 1800.0f)
        return { Role::Beat, 0.5f, "full-range balance" };

    return { Role::Beat, 0.25f, "unclear - set the role manually" };
}
}
