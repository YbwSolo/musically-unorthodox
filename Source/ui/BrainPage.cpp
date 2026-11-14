#include "BrainPage.h"
#include "../PluginProcessor.h"

namespace mu
{
using namespace theme;

namespace
{
const char* const kStarters[4] = { "What should I fix first on this track?", "Why does my vocal sound muddy?", "What do attack and release do?", "How loud should the master be?" };
const char* const kFooter = "An AI model running on this computer wrote this. It can be wrong: check it with your ears.";
}

// The conversation: each question in a yellow-edged box on the right, each answer as plain text under it.
class BrainPage::Talk : public juce::Component
{
public:
    explicit Talk (MUProcessor& p) : proc (p) {}

    int heightFor (int width) const
    {
        float y = 6.0f;
        const float w = (float) width - 16.0f;
        for (auto& e : proc.brainTalk)
        {
            y += wrappedHeight (e.question, juce::jmin (w * 0.78f, 520.0f) - 24.0f, bodyBold (13.0f)) + 20.0f + 10.0f;
            y += wrappedHeight (e.answer.isEmpty() ? juce::String ("Thinking") : e.answer, w - 8.0f, body (13.5f), 1.45f) + 8.0f;
            if (e.answer.isNotEmpty() && ! e.failed) y += 18.0f;
            y += 18.0f;
        }
        return (int) std::ceil (y) + 6;
    }

    void paint (juce::Graphics& g) override
    {
        float y = 6.0f;
        const float w = (float) getWidth() - 16.0f;
        for (auto& e : proc.brainTalk)
        {
            const float qw = juce::jmin (w * 0.78f, 520.0f);
            const float qh = wrappedHeight (e.question, qw - 24.0f, bodyBold (13.0f)) + 20.0f;
            const juce::Rectangle<float> q (8.0f + w - qw, y, qw, qh);
            g.setColour (raised); g.fillRoundedRectangle (q, 6.0f);
            g.setColour (yellow); g.fillRoundedRectangle (q.withWidth (3.0f), 1.5f);
            drawWrapped (g, e.question, q.reduced (12.0f, 10.0f), bodyBold (13.0f), text);
            y += qh + 10.0f;

            if (e.answer.isEmpty())
            {
                g.setFont (body (13.5f)); g.setColour (textDim);
                g.drawText ("Thinking" + juce::String::repeatedString (".", 1 + ((int) (dots * 3.0f)) % 3), juce::Rectangle<float> (12.0f, y, w, 20.0f), juce::Justification::centredLeft);
                y += wrappedHeight ("Thinking", w - 8.0f, body (13.5f), 1.45f) + 8.0f;
            }
            else
            {
                const float ah = wrappedHeight (e.answer, w - 8.0f, body (13.5f), 1.45f);
                drawWrapped (g, e.answer, { 12.0f, y, w - 8.0f, ah + 4.0f }, body (13.5f), e.failed ? textDim : text, 1.45f);
                y += ah + 8.0f;
                if (! e.failed)
                {
                    g.setFont (body (10.5f)); g.setColour (textMute);
                    g.drawText (kFooter, juce::Rectangle<float> (12.0f, y, w - 8.0f, 14.0f), juce::Justification::centredLeft);
                    y += 18.0f;
                }
            }
            y += 18.0f;
        }
    }

    float dots = 0.0f;

private:
    MUProcessor& proc;
};

BrainPage::BrainPage (MUProcessor& p, std::function<void (const juce::String&)> s) : proc (p), setStatus (std::move (s))
{
    talk = std::make_unique<Talk> (proc);
    viewport.setViewedComponent (talk.get(), false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addChildComponent (viewport);

    askBox.setFont (body (13.5f));
    askBox.setTextToShowWhenEmpty ("Ask about this track, or about mixing in general", textMute);
    askBox.setIndents (10, 8);
    askBox.onReturnKey = [this] { ask (askBox.getText()); };
    askBox.onEscapeKey = [this] { askBox.clear(); juce::Component::unfocusAllComponents(); };
    setHelp (askBox, "brain.ask");
    addChildComponent (askBox);
    styleButton (askButton, "Ask", true, "brain.ask");
    askButton.onClick = [this] { ask (askBox.getText()); };
    addChildComponent (askButton);
    styleButton (clearButton, "Start over", false, "brain.clear");
    clearButton.onClick = [this]
    {
        proc.brainTalk.clear(); proc.getAI().forgetConversation(); askedQuestion.clear();
        layoutTalk (false); resized(); repaint();
    };
    addChildComponent (clearButton);
    for (size_t i = 0; i < starters.size(); ++i)
    {
        styleButton (starters[i], kStarters[i], false, "brain.ask");
        starters[i].onClick = [this, i] { ask (kStarters[i]); };
        addChildComponent (starters[i]);
    }

    styleButton (getLight, "Download", true, "brain.model");
    getLight.onClick = [this] { proc.getAI().installModel ("light"); refresh(); };
    addChildComponent (getLight);
    styleButton (getSmart, "Download", false, "brain.model");
    getSmart.onClick = [this] { proc.getAI().installModel ("smart"); refresh(); };
    addChildComponent (getSmart);
    styleButton (stopButton, "Stop", false, "brain.model");
    stopButton.onClick = [this] { proc.getAI().cancelInstall(); };
    addChildComponent (stopButton);
    styleButton (otherButton, "Get the other model", false, "brain.model");
    otherButton.onClick = [this]
    {
        const auto st = proc.getAI().getStatus();
        if (st.otherId.isNotEmpty()) { proc.getAI().installModel (st.otherId); refresh(); }
    };
    addChildComponent (otherButton);

    lastAnswerId = proc.getAI().getStatus().answerId;
    proc.getAI().refresh();
    refresh();
}

BrainPage::~BrainPage() = default;

juce::String BrainPage::sizeText (juce::int64 bytes, const char* fallback) const
{
    return bytes > 0 ? "about " + juce::String ((double) bytes / 1.0e9, 1) + " GB" : juce::String (fallback);
}

void BrainPage::ask (const juce::String& typed)
{
    const auto question = typed.trim();
    auto& ai = proc.getAI();
    if (question.isEmpty() || ai.getStatus().busy || ! ai.getStatus().available) return;
    // a follow-up ("why?") is looked up together with the question before it, so the right notes are still found
    const bool followUp = askedQuestion.isNotEmpty() && juce::Time::currentTimeMillis() - askedAt < 10 * 60 * 1000;
    const auto lookUp = followUp ? askedQuestion + " " + question : question;
    askedQuestion = question;
    askedAt = juce::Time::currentTimeMillis();
    proc.brainTalk.push_back ({ question, {}, false });
    if (proc.brainTalk.size() > 40) proc.brainTalk.erase (proc.brainTalk.begin());
    ai.ask (proc.tutorPrompt (lookUp), question, (int) (juce::pointer_sized_int) &proc);
    askBox.clear();
    setStatus ("Mix Brain is thinking. The first answer after a break takes a little longer while it wakes up.");
    resized();
    layoutTalk (true);
    repaint();
}

void BrainPage::layoutTalk (bool toEnd)
{
    const int w = juce::jmax (100, viewport.getWidth() - 10);
    const int h = talk->heightFor (w);
    talk->setSize (w, juce::jmax (h, 1));
    if (toEnd) viewport.setViewPosition (0, juce::jmax (0, h - viewport.getHeight()));
    talk->repaint();
}

// What the page shows depends on whether a model is here, on its way, or still to be chosen.
void BrainPage::refresh()
{
    const auto st = proc.getAI().getStatus();
    using Setup = LocalAI::Setup;
    const View now = st.setup == Setup::Downloading ? View::Downloading
                   : st.available ? View::Chat
                   : st.setup == Setup::Unsupported ? View::Unsupported : View::Install;
    failed = st.setup == Setup::Failed;
    setupMessage = st.setupMessage;
    progress = st.progress;

    haveChoices = false; smartFits = true;
    lightName = "Lighter model"; smartName = "Smarter model";
    juce::int64 lightBytes = 0, smartBytes = 0; int smartMemory = 16; bool smartSilicon = true;
    for (auto& c : st.choices)
    {
        haveChoices = true;
        if (c.id == "smart") { smartName = c.name; smartBytes = c.bytes; smartFits = c.fits; smartMemory = c.minMemoryGb; smartSilicon = c.appleSilicon; }
        else                 { lightName = c.name; lightBytes = c.bytes; }
    }
    lightSize = sizeText (lightBytes, "about 2 GB"); smartSize = sizeText (smartBytes, "about 3 GB");
    lightNote = "Quick, short answers. Runs on any Mac the plugin runs on.";
    smartNote = "Fuller explanations and better at following a conversation. Needs " + juce::String (smartSilicon ? "an Apple-silicon Mac" : "a Mac")
              + (smartMemory > 0 ? " with " + juce::String (smartMemory) + " GB of memory" : juce::String()) + "."
              + (smartFits ? juce::String() : " This computer does not have that, so the lighter model is the one to get.");
    getLight.setButtonText ("Download " + (lightBytes > 0 ? juce::String ((double) lightBytes / 1.0e9, 1) + " GB" : juce::String ("2 GB")));
    getSmart.setButtonText ("Download " + (smartBytes > 0 ? juce::String ((double) smartBytes / 1.0e9, 1) + " GB" : juce::String ("3 GB")));
    getSmart.setEnabled (smartFits);

    modelLine = st.available ? (st.builtIn ? st.model + ", running on this computer." : st.model + ", through Ollama on this computer.") : juce::String();
    otherLine.clear();
    bool offerOther = false;
    if (st.available && st.builtIn && st.otherId.isNotEmpty())
        for (auto& c : st.choices)
            if (c.id == st.otherId)
            {
                offerOther = c.fits;
                otherLine = (c.id == "smart" ? "A smarter model is on offer: " : "A lighter, quicker model is on offer: ") + c.name + ", " + sizeText (c.bytes, c.id == "smart" ? "about 3 GB" : "about 2 GB")
                          + (c.fits ? ". It replaces the one you have." : ". This computer does not have the memory for it.");
                otherButton.setButtonText (c.id == "smart" ? "Get the smarter model" : "Get the lighter model");
            }
    facts = proc.tutorTrackNotes().upToFirstOccurrenceOf ("\n\nTHE WHOLE SESSION", false, false).fromFirstOccurrenceOf ("THIS TRACK\n", false, false).trim();

    const bool changed = now != view;
    view = now;
    const bool chat = view == View::Chat, install = view == View::Install, downloading = view == View::Downloading;
    viewport.setVisible (chat); askBox.setVisible (chat); askButton.setVisible (chat);
    clearButton.setVisible (chat && ! proc.brainTalk.empty());
    for (auto& b : starters) b.setVisible (chat && proc.brainTalk.empty());
    getLight.setVisible (install); getSmart.setVisible (install);
    stopButton.setVisible (downloading);
    otherButton.setVisible (chat && offerOther);
    askButton.setEnabled (! st.busy);
    if (changed) { resized(); if (chat) layoutTalk (true); }
    repaint();
}

void BrainPage::tick (float dt)
{
    dots += dt; talk->dots = dots;
    const auto st = proc.getAI().getStatus();
    if (st.busy) talk->repaint();
    if (st.answerId != lastAnswerId)
    {
        lastAnswerId = st.answerId;
        // the answer belongs to the last question still waiting for one
        for (auto it = proc.brainTalk.rbegin(); it != proc.brainTalk.rend(); ++it)
            if (it->answer.isEmpty())
            {
                if (st.answer.isNotEmpty()) it->answer = st.answer;
                else { it->answer = st.error.isNotEmpty() ? st.error : juce::String ("No answer came back. Ask again."); it->failed = true; }
                break;
            }
        layoutTalk (true);
        refresh();
    }
    refreshTimer -= dt;
    if (refreshTimer <= 0.0f || view == View::Downloading)
    {
        if (refreshTimer <= 0.0f) refreshTimer = 0.5f;
        refresh();
    }
}

void BrainPage::paint (juce::Graphics& g)
{
    g.fillAll (black);
    fillPanel (g, mainArea.toFloat());
    fillPanel (g, sideArea.toFloat());

    auto head = mainArea.reduced (18, 0).removeFromTop (52);
    g.setFont (headline (22.0f)); g.setColour (text);
    g.drawText ("MIX BRAIN", head, juce::Justification::centredLeft);

    auto side = sideArea.reduced (16, 14).toFloat();
    auto label = [&] (const juce::String& t) { g.setFont (headline (15.0f)); g.setColour (text); g.drawText (t, side.removeFromTop (22.0f), juce::Justification::centredLeft); side.removeFromTop (4.0f); };
    auto para = [&] (const juce::String& t, juce::Colour c = textDim)
    {
        const float h = wrappedHeight (t, side.getWidth(), body (11.5f), 1.4f);
        drawWrapped (g, t, side.removeFromTop (juce::jmin (h + 2.0f, side.getHeight())), body (11.5f), c, 1.4f);
        side.removeFromTop (12.0f);
    };

    if (view == View::Chat)
    {
        if (proc.brainTalk.empty())
        {
            auto intro = mainArea.reduced (18, 0).withTrimmedTop (52).removeFromTop (60).toFloat();
            drawWrapped (g, "Ask in your own words. Mix Brain reads the measurements of this track and of the whole session before it answers, so you can ask what is wrong, what a control does, or what to do next. Try one of these, or type your own question below.",
                         intro, body (13.0f), textDim, 1.45f);
        }
        label ("THE MODEL");
        para (modelLine + " Nothing you ask leaves this computer, and it works without the internet.");
        if (otherLine.isNotEmpty()) { para (otherLine); side.removeFromTop ((float) (otherButton.isVisible() ? 40 : 0)); }
        label ("WHAT IT CAN SEE");
        para (facts.isNotEmpty() ? facts : juce::String ("Play the song so the track can be measured."));
    }
    else
    {
        auto body13 = body (13.0f);
        auto area = mainArea.reduced (18, 0).withTrimmedTop (52).toFloat();
        const juce::String what = view == View::Unsupported
            ? "Mix Brain is an AI tutor that runs on the computer itself. The built-in version is available on Mac. If you already use Ollama on this computer, start it and this tab will use it."
            : "Mix Brain is a tutor you can ask about your track in plain words: what is wrong with it, what a control does, what to do next. It is an AI model that runs on this computer, so nothing you ask leaves the room, it works without the internet, and it costs nothing. It needs one download, and there are two sizes to choose from.";
        const float h = wrappedHeight (what, area.getWidth(), body13, 1.45f);
        drawWrapped (g, what, area.removeFromTop (h + 4.0f), body13, textDim, 1.45f);

        if (view == View::Install)
        {
            auto card = [&] (juce::Rectangle<int> r, const juce::String& kind, const juce::String& name, const juce::String& size, const juce::String& note, bool recommended, bool usable)
            {
                auto c = r.toFloat();
                g.setColour (raised); g.fillRoundedRectangle (c, 6.0f);
                if (recommended) { g.setColour (yellow); g.fillRoundedRectangle (c.withHeight (3.0f), 1.5f); }
                c = c.reduced (16.0f, 14.0f);
                g.setFont (headline (17.0f)); g.setColour (usable ? text : textDim);
                g.drawText (kind, c.removeFromTop (24.0f), juce::Justification::centredLeft);
                g.setFont (headline (30.0f)); g.setColour (usable ? yellow : textMute);
                g.drawText (size, c.removeFromTop (40.0f), juce::Justification::centredLeft);
                g.setFont (body (11.5f)); g.setColour (textMute);
                g.drawText (name, c.removeFromTop (18.0f), juce::Justification::centredLeft);
                c.removeFromTop (6.0f);
                drawWrapped (g, note, c.withTrimmedBottom (44.0f), body (12.0f), textDim, 1.4f);
            };
            card (cardLight, "LIGHTER MODEL", lightName, lightSize, lightNote, ! smartFits, true);
            card (cardSmart, "SMARTER MODEL", smartName, smartSize, smartNote, smartFits, smartFits);
            auto under = juce::Rectangle<int> (mainArea.getX() + 18, cardLight.getBottom() + 14, mainArea.getWidth() - 36, 60).toFloat();
            drawWrapped (g, failed && setupMessage.isNotEmpty() ? setupMessage + " Press a Download button to try again."
                                                                : juce::String ("You can keep working while it downloads. Either one can be swapped for the other later, from this tab or from Settings.")
                                                                  + (haveChoices ? juce::String() : juce::String (" (Sizes are approximate until the plugin has been online once.)")),
                         under, body (12.0f), failed ? text : textMute, 1.4f);
        }
        else if (view == View::Downloading)
        {
            auto bar = progressArea.toFloat();
            g.setColour (control); g.fillRoundedRectangle (bar, 4.0f);
            g.setColour (yellow); g.fillRoundedRectangle (bar.withWidth (juce::jmax (8.0f, bar.getWidth() * juce::jlimit (0.0f, 1.0f, progress))), 4.0f);
            g.setFont (bodyBold (13.0f)); g.setColour (text);
            g.drawText (setupMessage.isNotEmpty() ? setupMessage : juce::String ("Getting ready..."), progressArea.translated (0, -28).withHeight (22), juce::Justification::centredLeft);
            g.setFont (body (12.0f)); g.setColour (textMute);
            g.drawText ("You can keep working while it downloads. The chat opens here as soon as it is ready.", progressArea.translated (0, 22).withHeight (20), juce::Justification::centredLeft);
        }
        label ("HOW IT WORKS");
        para ("The model is a single file kept in your Music folder with a small helper program that runs it. The plugin starts it when you ask a question and it stops when Logic closes.");
        para ("Before answering, Mix Brain is given the measurements of this track and of the session, plus the plugin's own notes on mixing. It explains and suggests; it never changes your settings.");
        label ("TWO SIZES");
        para ("Lighter: " + lightSize + ". Smarter: " + smartSize + ", for Apple-silicon Macs with 16 GB of memory.");
    }
}

void BrainPage::resized()
{
    auto r = getLocalBounds().reduced (12);
    sideArea = r.removeFromRight (290);
    r.removeFromRight (10);
    mainArea = r;

    auto m = mainArea.reduced (18, 0);
    auto head = m.removeFromTop (52);
    clearButton.setBounds (head.removeFromRight (96).withSizeKeepingCentre (96, 28));
    m.removeFromBottom (16);

    if (view == View::Chat)
    {
        auto askRow = m.removeFromBottom (36);
        askButton.setBounds (askRow.removeFromRight (84));
        askRow.removeFromRight (8);
        askBox.setBounds (askRow);
        m.removeFromBottom (12);
        if (proc.brainTalk.empty())
        {
            m.removeFromTop (66);
            auto rows = m.removeFromTop (76);
            auto top = rows.removeFromTop (34); rows.removeFromTop (8);
            layoutRow (top, { &starters[0], &starters[1] }, 8);
            layoutRow (rows.removeFromTop (34), { &starters[2], &starters[3] }, 8);
            viewport.setBounds (m.withHeight (0));
        }
        else viewport.setBounds (m);

        // the model button sits under the line that describes the other model
        auto side = sideArea.reduced (16, 14);
        const float w = (float) side.getWidth();
        int y = side.getY() + 26 + (int) wrappedHeight (modelLine + " Nothing you ask leaves this computer, and it works without the internet.", w, body (11.5f), 1.4f) + 14;
        if (otherLine.isNotEmpty()) y += (int) wrappedHeight (otherLine, w, body (11.5f), 1.4f) + 14;
        otherButton.setBounds (side.getX(), y - 4, side.getWidth(), 30);
    }
    else
    {
        const juce::String what = "Mix Brain is a tutor you can ask about your track in plain words: what is wrong with it, what a control does, what to do next. It is an AI model that runs on this computer, so nothing you ask leaves the room, it works without the internet, and it costs nothing. It needs one download, and there are two sizes to choose from.";
        m.removeFromTop ((int) wrappedHeight (what, (float) m.getWidth(), body (13.0f), 1.45f) + 22);
        auto cards = m.removeFromTop (juce::jmin (230, m.getHeight() - 70));
        cardLight = cards.removeFromLeft ((cards.getWidth() - 12) / 2);
        cards.removeFromLeft (12);
        cardSmart = cards;
        getLight.setBounds (cardLight.reduced (16, 14).removeFromBottom (32).removeFromLeft (170));
        getSmart.setBounds (cardSmart.reduced (16, 14).removeFromBottom (32).removeFromLeft (170));
        progressArea = juce::Rectangle<int> (mainArea.getX() + 18, cardLight.getY() + 40, mainArea.getWidth() - 36 - 100, 12);
        stopButton.setBounds (progressArea.getRight() + 16, progressArea.getCentreY() - 15, 84, 30);
    }
}
}
