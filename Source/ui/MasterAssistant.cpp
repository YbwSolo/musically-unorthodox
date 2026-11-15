#include "MasterAssistant.h"
#include "../PluginProcessor.h"

namespace mu
{
using namespace theme;

namespace
{
    struct Row { const char* id; const char* name; const char* help; };
    const Row kRows[7] = {
        { "eqEnabled",   "EQ: corrective, tonal, dynamic", "mst.eq" },
        { "mbOn",        "Multiband compression",          "mst.mb" },
        { "compEnabled", "Glue compression",               "mst.glue" },
        { "satOn",       "Saturation",                     "mst.sat" },
        { "stMonoBass",  "Mono bass",                      "mst.stereo" },
        { "clipOn",      "Soft clipper",                   "mst.clip" },
        { "peakGuard",   "True peak limiter",              "mst.limiter" } };
}

MasterAssistant::MasterAssistant (MUProcessor& p, std::function<void (const juce::String&)> status)
    : proc (p), setStatus (std::move (status)),
      intensity (p, ids::mstIntensity, "Intensity", "mst.intensity"),
      crossLow (p, ids::mbCrossLow, "Low band up to", "mst.mb"), lowThreshold (p, ids::mbThreshold (0), "Threshold", "mst.mb"), lowRatio (p, ids::mbRatio (0), "Ratio", "mst.mb"),
      clipAmount (p, ids::clipAmount, "Amount", "mst.clip"), clipSoft (p, ids::clipSoft, "Softness", "mst.clip")
{
    setOpaque (true);
    styleButton (mainButton, "ANALYSE & MASTER", true, "mst.main");
    mainButton.onClick = [this] { pressMain(); };
    styleButton (againButton, "Decide again", false, "mst.again");
    againButton.onClick = [this] { statusText = proc.restyleMaster(); setStatus (statusText); resized(); repaint(); };
    styleButton (undoButton, "Undo", false, "mst.undo");
    undoButton.onClick = [this] { statusText = proc.undoMaster(); setStatus (statusText); shownReport.clear(); resized(); repaint(); };
    styleButton (loadRefButton, "Load a file", false, "mst.reference");
    loadRefButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a finished song to lean towards", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.m4a");
        const juce::Component::SafePointer<MasterAssistant> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
        {
            if (safe == nullptr || fc.getResult() == juce::File()) return;
            safe->statusText = safe->proc.loadReferenceFile (fc.getResult());
            safe->setStatus (safe->statusText); safe->resized(); safe->repaint();
        });
    };
    styleButton (playRefButton, "Play one", false, "mst.reference");
    playRefButton.onClick = [this] { statusText = proc.isReferencePass() ? proc.finishReferencePass() : proc.startReferencePass(); setStatus (statusText); resized(); repaint(); };
    styleButton (clearRefButton, "Clear", false, "mst.reference");
    clearRefButton.onClick = [this] { proc.clearMasterReference(); statusText = "Reference cleared. The style decides the tone and the loudness again."; setStatus (statusText); resized(); repaint(); };

    styleBox.addItemList (masterStyleNames(), 1);
    styleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ids::mstStyle, styleBox);
    setHelp (styleBox, "mst.style");
    setHelp (compare, "mst.compare");
    compare.onChange = [this] (int i)
    {
        proc.setMasterCompare (i);
        setStatus (i == 0 ? juce::String ("Hearing the master as it will be bounced.")
                 : i == 1 ? juce::String ("Hearing the master turned down to the loudness the mix had, so the two can be compared fairly. This is only for listening: it is left out of a bounce.")
                          : juce::String ("Hearing the mix as it arrived, with nothing done to it. Switch between this and Level-matched. This is only for listening: it is left out of a bounce."));
    };
    for (int i = 0; i < 7; ++i)
    {
        switches[(size_t) i] = std::make_unique<ParamToggle> (proc, kRows[i].id, kRows[i].name, "switch", kRows[i].help);
        addAndMakeVisible (*switches[(size_t) i]);
    }
    report.setMultiLine (true, true); report.setReadOnly (true); report.setScrollbarsShown (true); report.setCaretVisible (false);
    report.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    report.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    report.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    report.setWantsKeyboardFocus (false);
    for (auto* c : std::initializer_list<juce::Component*> { &mainButton, &againButton, &undoButton, &loadRefButton, &playRefButton, &clearRefButton, &styleBox, &intensity, &crossLow, &lowThreshold, &lowRatio,
                                                              &clipAmount, &clipSoft, &compare, &report })
        addAndMakeVisible (c);
    statusText = "Finish the mix first. Then press the button, play the song from the start to the end, and press it again. The plugin measures the mix, decides what this song needs, sets it up and measures the result.";
    refreshReport();
}

void MasterAssistant::pressMain()
{
    using Stage = MUProcessor::MasterStage;
    const auto stage = proc.getMasterStage();
    if (stage == Stage::Listening && ! proc.isReferencePass()) statusText = proc.finishMastering();
    else if (stage == Stage::Listening) statusText = "The reference is being listened to. Press Use as reference first.";
    else statusText = proc.startMastering();
    setStatus (statusText);
    resized(); repaint();
}

void MasterAssistant::refreshReport()
{
    struct Part { const char* key; const char* title; };
    const Part parts[5] = { { "detected", "What was measured and found" }, { "processing", "What it is doing" }, { "left", "What it left alone, and why" },
                            { "notes", "What belongs in the mix, not the master" }, { "report", "The result, measured" } };
    juce::String all;
    for (auto& p : parts) all << p.key << ":" << proc.masterText (p.key).joinIntoString ("|") << "\n";
    if (all == shownReport) return;
    shownReport = all;
    report.clear();
    bool any = false;
    for (auto& p : parts)
    {
        const auto lines = proc.masterText (p.key);
        if (lines.isEmpty()) continue;
        any = true;
        report.setFont (bodyBold (13.5f)); report.setColour (juce::TextEditor::textColourId, juce::String (p.key) == "report" ? yellow : text);
        report.insertTextAtCaret (juce::String (p.title) + "\n");
        report.setFont (body (12.5f)); report.setColour (juce::TextEditor::textColourId, textDim);
        for (auto& l : lines) report.insertTextAtCaret (juce::String::fromUTF8 ("\xe2\x80\xa2 ") + l + "\n");
        report.insertTextAtCaret ("\n");
    }
    if (! any)
    {
        report.setFont (body (12.5f)); report.setColour (juce::TextEditor::textColourId, textMute);
        report.insertTextAtCaret ("Nothing has been mastered yet.\n\nWhat the assistant does: it measures the whole mix (tone, loudness, dynamics, low end, stereo), and from those measurements "
                                  "it chooses which processors this song needs. A good mix that is only quiet gets a limiter and nothing else. Every decision is listed here with the measurement behind it, "
                                  "and every control it sets can be changed afterwards.\n\nIt is a set of rules worked from measurements. It is not a trained AI model, and it cannot fix a problem inside "
                                  "one track of the mix: when it finds one, it says so.");
    }
    report.setCaretPosition (0);
}

void MasterAssistant::tick (float dt)
{
    using Stage = MUProcessor::MasterStage;
    const auto stage = proc.getMasterStage();
    const float heard = proc.masteringSeconds();
    juce::String label = "ANALYSE & MASTER";
    if (stage == Stage::Listening && ! proc.isReferencePass())
        label = heard >= MUProcessor::kMasterMinSeconds ? "MASTER IT  (" + juce::String ((int) heard / 60) + ":" + juce::String ((int) heard % 60).paddedLeft ('0', 2) + " heard)"
                                                        : "LISTENING  " + juce::String ((int) heard) + " of " + juce::String ((int) MUProcessor::kMasterMinSeconds) + " s";
    if (mainButton.getButtonText() != label) mainButton.setButtonText (label);
    if (compare.getIndex() != proc.getMasterCompare()) compare.setIndex (proc.getMasterCompare());
    const auto refLabelText = proc.isReferencePass() ? juce::String ("Use as reference") : juce::String ("Play one");
    if (playRefButton.getButtonText() != refLabelText) playRefButton.setButtonText (refLabelText);

    slow -= dt;
    if (slow > 0.0f && (int) stage == shownStage) return;
    slow = 0.3f;
    if ((int) stage != shownStage)
    {
        shownStage = (int) stage;
        if (stage == Stage::Settling) statusText = "Mastered. Keep the song playing: the level is settling on its target, then the master is measured for the report.";
        if (stage == Stage::Checking) statusText = "Measuring the master: " + juce::String ((int) MUProcessor::kMasterCheckSeconds) + " s of the song, for the report. Keep it playing.";
        if (stage == Stage::Done && ! proc.masterText ("report").isEmpty()) statusText = "Done. Read what was measured, then compare by ear: switch between Mix and Level-matched.";
        resized();
    }
    if (stage == Stage::Checking) statusText = "Measuring the master: " + juce::String ((int) heard) + " of " + juce::String ((int) MUProcessor::kMasterCheckSeconds) + " s. Keep it playing.";
    againButton.setEnabled (proc.hasMixFacts() && stage != Stage::Listening);
    undoButton.setEnabled (proc.canUndoMaster());
    clearRefButton.setEnabled (proc.getMasterReferenceName().isNotEmpty());
    const auto style = (MasterStyle) juce::jlimit (0, (int) MasterStyle::Count - 1, (int) proc.getParam (ids::mstStyle));
    blurb = masterStyleBlurb (style);
    refreshReport();
    repaint();
}

void MasterAssistant::paint (juce::Graphics& g)
{
    g.fillAll (black);
    fillPanel (g, leftArea.toFloat());
    fillPanel (g, reportArea.toFloat());
    fillPanel (g, rightArea.toFloat());
    drawWrapped (g, statusText, statusArea.toFloat(), body (12.0f), textDim);
    g.setFont (bodyBold (12.5f)); g.setColour (text);
    g.drawText ("Style", styleLabel, juce::Justification::centredLeft);
    g.drawText ("Reference", refLabel, juce::Justification::centredLeft);
    g.drawText ("Listen to", compareLabel, juce::Justification::centredLeft);
    g.setFont (body (11.0f)); g.setColour (textMute);
    g.drawText ("Level-matched: the master at the mix's loudness", compareNote, juce::Justification::centredLeft, true);
    g.setFont (bodyBold (12.5f)); g.setColour (text);
    drawWrapped (g, blurb, blurbArea.toFloat(), body (11.5f), textMute);
    const auto name = proc.getMasterReferenceName();
    g.setFont (body (11.5f)); g.setColour (name.isNotEmpty() ? yellow : textMute);
    g.drawText (name.isNotEmpty() ? "Leaning towards: " + name : "None: the style decides", refArea, juce::Justification::centredLeft, true);

    g.setFont (headline (18.0f)); g.setColour (text);
    g.drawText ("PROCESSORS", rightArea.reduced (14, 0).removeFromTop (40), juce::Justification::centredLeft);
    // what each one is doing right now
    const float values[7] = { 0.0f, proc.getMultiband().getReductionDb (0), proc.getCompressor().getGainReductionDb(), 0.0f, 0.0f, proc.getClipper().getReductionDb(), proc.getMaximizer().getGainReductionDb() };
    g.setFont (body (11.0f));
    for (int i = 0; i < 7; ++i)
    {
        if (i == 0 || i == 3 || i == 4 || proc.getParam (kRows[i].id) < 0.5f) continue;
        g.setColour (values[i] < -0.05f ? yellow : textMute);
        g.drawText (juce::String (values[i], 1) + " dB", readouts[(size_t) i], juce::Justification::centredRight);
    }
    g.setFont (bodyBold (11.5f)); g.setColour (textDim);
    g.drawText ("Multiband, low band", knobLabelA, juce::Justification::centredLeft);
    g.drawText ("Soft clipper", knobLabelB, juce::Justification::centredLeft);
}

void MasterAssistant::resized()
{
    auto r = getLocalBounds();
    leftArea = r.removeFromLeft (300);
    r.removeFromLeft (10);
    rightArea = r.removeFromRight (270);
    r.removeFromRight (10);
    reportArea = r;
    report.setBounds (reportArea.reduced (12, 10));

    auto l = leftArea.reduced (14, 12);
    mainButton.setBounds (l.removeFromTop (48));
    l.removeFromTop (8);
    statusArea = l.removeFromTop (juce::jlimit (34, 110, juce::roundToInt (wrappedHeight (statusText, (float) l.getWidth(), body (12.0f))) + 4));
    l.removeFromTop (8);
    styleLabel = l.removeFromTop (18);
    styleBox.setBounds (l.removeFromTop (26));
    l.removeFromTop (4);
    blurbArea = l.removeFromTop (34);
    l.removeFromTop (6);
    {
        auto row = l.removeFromTop (88);
        intensity.setBounds (row.removeFromLeft (96));
        row.removeFromLeft (12);
        auto col = row.withSizeKeepingCentre (row.getWidth(), 60);
        againButton.setBounds (col.removeFromTop (26));
        col.removeFromTop (8);
        undoButton.setBounds (col.removeFromTop (26));
    }
    l.removeFromTop (8);
    refLabel = l.removeFromTop (18);
    {
        auto row = l.removeFromTop (26);
        const int w = (row.getWidth() - 12) / 3;
        loadRefButton.setBounds (row.removeFromLeft (w)); row.removeFromLeft (6);
        playRefButton.setBounds (row.removeFromLeft (w + 20)); row.removeFromLeft (6);
        clearRefButton.setBounds (row);
    }
    refArea = l.removeFromTop (20);
    l.removeFromTop (6);
    compareLabel = l.removeFromTop (18);
    compare.setBounds (l.removeFromTop (26));
    compareNote = l.removeFromTop (18);

    auto p = rightArea.reduced (14, 0);
    p.removeFromTop (40);
    switchArea = p.removeFromTop (7 * 28);
    auto rows = switchArea;
    for (int i = 0; i < 7; ++i)
    {
        auto row = rows.removeFromTop (28);
        readouts[(size_t) i] = row.removeFromRight (58);
        switches[(size_t) i]->setBounds (row.withSizeKeepingCentre (row.getWidth(), 22));
    }
    p.removeFromTop (8);
    knobLabelA = p.removeFromTop (18);
    {
        auto row = p.removeFromTop (88);
        const int w = row.getWidth() / 3;
        crossLow.setBounds (row.removeFromLeft (w)); lowThreshold.setBounds (row.removeFromLeft (w)); lowRatio.setBounds (row);
    }
    p.removeFromTop (6);
    knobLabelB = p.removeFromTop (18);
    {
        auto row = p.removeFromTop (88);
        const int w = row.getWidth() / 3;
        clipAmount.setBounds (row.removeFromLeft (w)); clipSoft.setBounds (row.removeFromLeft (w));
    }
}
}
