#include "ChainPage.h"
#include "Theme.h"

namespace mu
{
ChainPage::ChainPage (MUProcessor& p) : proc (p)
{
    for (auto* panel : { &dsPanel, &satPanel, &riderPanel, &vsPanel }) addAndMakeVisible (panel);
    dsPanel.bindToggle (proc.apvts, ids::dsOn);
    satPanel.bindToggle (proc.apvts, ids::satOn);
    riderPanel.bindToggle (proc.apvts, ids::riderOn);
    vsPanel.bindToggle (proc.apvts, ids::vsEnabled);

    auto mk = [this] (std::unique_ptr<Knob>& k, ModulePanel& panel, const juce::String& id, const juce::String& title, const juce::String& tip)
    {
        k = std::make_unique<Knob> (proc.apvts, id, title, tip);
        panel.addAndMakeVisible (*k);
    };
    auto combo = [this] (juce::ComboBox& box, ModulePanel& panel, const juce::String& id, const juce::StringArray& items, std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& att)
    {
        box.addItemList (items, 1);
        panel.addAndMakeVisible (box);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, box);
    };
    auto info = [this] (juce::Label& l, ModulePanel& panel, const juce::String& text)
    {
        l.setFont (theme::body (11.5f));
        l.setColour (juce::Label::textColourId, theme::textDim);
        l.setJustificationType (juce::Justification::topLeft);
        l.setText (text, juce::dontSendNotification);
        panel.addAndMakeVisible (l);
    };

    mk (dsFreq, dsPanel, ids::dsFreq, "Frequency", "Where the esses live. 5-7 kHz for most voices, higher for a bright female vocal.");
    mk (dsThr, dsPanel, ids::dsThreshold, "Threshold", "Lower it until the meter only moves on the esses.");
    mk (dsRange, dsPanel, ids::dsRange, "Range", "Maximum amount it pulls down. 4-8 dB is plenty.");
    combo (dsMode, dsPanel, ids::dsMode, { "Split band", "Wide band" }, dsModeAtt);
    dsMeter.setLabel ("DE-ESS");
    dsPanel.addAndMakeVisible (dsMeter);
    info (dsInfo, dsPanel, "Split band only turns down the top end during an ess. Wide band ducks the whole vocal, which can sound more natural on harsh takes.");

    mk (satDrive, satPanel, ids::satDrive, "Drive", "How hard the signal pushes into the curve. Level-compensated, so listen for density, not volume.");
    mk (satMix, satPanel, ids::satMix, "Mix", "Blend of saturated and clean. 30-40 % adds presence without obvious distortion.");
    combo (satType, satPanel, ids::satType, { "Tape", "Tube", "Warm" }, satTypeAtt);
    info (satInfo, satPanel, "Tape: smooth, glues vocals. Tube: adds even harmonics, more forward. Warm: gentle, good on 808s so they read on small speakers.");

    mk (riderTarget, riderPanel, ids::riderTarget, "Target", "The average level the rider keeps the track at. Apply a chain preset after playing the track and this is set for you.");
    mk (riderRange, riderPanel, ids::riderRange, "Range", "How far it is allowed to move either way.");
    combo (riderSpeed, riderPanel, ids::riderSpeed, { "Slow", "Normal", "Fast" }, riderSpeedAtt);
    riderGain.setFont (theme::headline (22.0f));
    riderGain.setJustificationType (juce::Justification::centred);
    riderPanel.addAndMakeVisible (riderGain);

    mk (vsAmount, vsPanel, ids::vsAmount, "Amount", "How much room the beat makes for the vocal.");
    combo (vsSpeed, vsPanel, ids::vsSpeed, { "Smooth", "Normal", "Fast" }, vsSpeedAtt);
    info (vsInfo, vsPanel, "Only works on beat / instrument instances. Dips 260 Hz, 2.6 kHz and 7 kHz while the Lead vocal instance is singing, so the vocal sits on top without the beat getting quieter overall.");

    orderInfo.setFont (theme::body (11.5f));
    orderInfo.setColour (juce::Label::textColourId, theme::textMute);
    orderInfo.setText ("Signal order:  TRIM  >  GATE  >  EQ  >  COMPRESSOR  >  DE-ESSER  >  SATURATION  >  RIDER  >  VOCAL SPACE  >  OUTPUT", juce::dontSendNotification);
    addAndMakeVisible (orderInfo);
}

void ChainPage::refresh()
{
    dsMeter.setReduction (proc.getDeEsser().getGainReductionDb());
    const float rg = proc.getRider().getCurrentGainDb();
    riderGain.setText (theme::dbString (rg), juce::dontSendNotification);
    riderGain.setColour (juce::Label::textColourId, std::abs (rg) > 0.1f ? theme::yellow : theme::textMute);
    const auto role = proc.getEffectiveRole();
    const bool vsAllowed = role != Role::Vocal && role != Role::BackingVocal && role != Role::Master;
    vsPanel.setHint (vsAllowed ? "ducks the beat under the vocal" : "not used on vocal or master instances");
    vsAmount->setEnabled (vsAllowed);
    vsSpeed.setEnabled (vsAllowed);
    if (auto* t = vsPanel.getToggle()) t->setEnabled (vsAllowed);
}

void ChainPage::paint (juce::Graphics& g) { g.fillAll (theme::bg); }

void ChainPage::resized()
{
    auto r = getLocalBounds().reduced (18, 16);
    orderInfo.setBounds (r.removeFromBottom (18));
    r.removeFromBottom (8);
    const int gap = 12;
    const int rowH = (r.getHeight() - gap) / 2;
    auto top = r.removeFromTop (rowH);
    r.removeFromTop (gap);
    auto bottom = r;
    const int colW = (top.getWidth() - gap) / 2;
    dsPanel.setBounds (top.removeFromLeft (colW));
    top.removeFromLeft (gap);
    satPanel.setBounds (top);
    riderPanel.setBounds (bottom.removeFromLeft (colW));
    bottom.removeFromLeft (gap);
    vsPanel.setBounds (bottom);

    const int kw = 124;
    {
        auto c = dsPanel.getContentArea();
        auto side = c.removeFromRight (juce::jmax (150, c.getWidth() - 3 * kw - 10));
        dsMode.setBounds (side.removeFromTop (24));
        side.removeFromTop (8);
        dsMeter.setBounds (side.removeFromTop (22));
        side.removeFromTop (8);
        dsInfo.setBounds (side);
        dsFreq->setBounds (c.removeFromLeft (kw)); dsThr->setBounds (c.removeFromLeft (kw)); dsRange->setBounds (c.removeFromLeft (kw));
    }
    {
        auto c = satPanel.getContentArea();
        auto side = c.removeFromRight (juce::jmax (150, c.getWidth() - 2 * kw - 10));
        satType.setBounds (side.removeFromTop (24));
        side.removeFromTop (8);
        satInfo.setBounds (side);
        satDrive->setBounds (c.removeFromLeft (kw)); satMix->setBounds (c.removeFromLeft (kw));
    }
    {
        auto c = riderPanel.getContentArea();
        auto side = c.removeFromRight (juce::jmax (150, c.getWidth() - 2 * kw - 10));
        riderSpeed.setBounds (side.removeFromTop (24));
        side.removeFromTop (10);
        riderGain.setBounds (side.removeFromTop (40));
        riderTarget->setBounds (c.removeFromLeft (kw)); riderRange->setBounds (c.removeFromLeft (kw));
    }
    {
        auto c = vsPanel.getContentArea();
        auto side = c.removeFromRight (juce::jmax (150, c.getWidth() - kw - 10));
        vsSpeed.setBounds (side.removeFromTop (24));
        side.removeFromTop (8);
        vsInfo.setBounds (side);
        vsAmount->setBounds (c.removeFromLeft (kw));
    }
}
}
