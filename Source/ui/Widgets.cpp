#include "Widgets.h"
#include "../PluginProcessor.h"

namespace mu
{
void setHelp (juce::Component& c, const juce::String& key) { c.getProperties().set ("help", key); }

using namespace theme;

// ---- Knob ------------------------------------------------------------------------------------------
Knob::Knob (MUProcessor& p, const juce::String& paramId, const juce::String& l, const juce::String& helpKey) : label (l)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (220);
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, paramId, slider);
    setHelp (*this, helpKey.isNotEmpty() ? helpKey : paramId);
    shown = (float) slider.valueToProportionOfLength (slider.getValue());
    // when a preset or the A/B switch moves the value, the pointer sweeps to its new position instead of jumping
    slider.onValueChange = [this]
    {
        if (slider.isMouseButtonDown() || ! isShowing()) { shown = (float) slider.valueToProportionOfLength (slider.getValue()); slider.getProperties().remove ("animPos"); }
        else if (! isTimerRunning()) startTimerHz (60);
    };
}

void Knob::timerCallback()
{
    const float target = (float) slider.valueToProportionOfLength (slider.getValue());
    shown = approach (shown, target, 1.0f / 60.0f, 0.07f);
    if (std::abs (shown - target) < 0.002f || slider.isMouseButtonDown()) { shown = target; slider.getProperties().remove ("animPos"); stopTimer(); }
    else slider.getProperties().set ("animPos", shown);
    slider.repaint();
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (15);
    const int side = juce::jmin (r.getWidth(), r.getHeight() - 16, 64);
    slider.setBounds (r.withSizeKeepingCentre (juce::jmax (side, 40), side + 16));
}

void Knob::paint (juce::Graphics& g)
{
    g.setFont (body (11.0f));
    g.setColour (dimmed ? textMute : textDim);
    g.drawFittedText (label, getLocalBounds().removeFromTop (14), juce::Justification::centred, 1, 0.85f);
}

// ---- ParamToggle -----------------------------------------------------------------------------------
ParamToggle::ParamToggle (MUProcessor& p, const juce::String& paramId, const juce::String& text, const juce::String& style, const juce::String& helpKey)
{
    setButtonText (text);
    getProperties().set ("style", style);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, paramId, *this);
    setHelp (*this, helpKey.isNotEmpty() ? helpKey : paramId);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus (false);     // the space bar should keep starting and stopping the song in the host
}

// ---- Segmented -------------------------------------------------------------------------------------
Segmented::Segmented (const juce::StringArray& i) : items (i) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

Segmented::Segmented (MUProcessor& p, const juce::String& paramId, const juce::StringArray& i, const juce::String& helpKey) : Segmented (i)
{
    if (auto* param = p.apvts.getParameter (paramId))
    {
        att = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { setIndex (juce::roundToInt (v), false); });
        att->sendInitialUpdate();
        animPos = (float) index;
    }
    setHelp (*this, helpKey.isNotEmpty() ? helpKey : paramId);
}

Segmented::~Segmented() { stopTimer(); }

void Segmented::setIndex (int i, bool notify)
{
    i = juce::jlimit (0, juce::jmax (0, items.size() - 1), i);
    if (i != index)
    {
        index = i;
        if (isShowing()) startTimerHz (60); else animPos = (float) index;
        repaint();
    }
    if (notify)
    {
        if (att != nullptr) att->setValueAsCompleteGesture ((float) i);
        if (onChange) onChange (i);
    }
}

void Segmented::timerCallback()
{
    animPos = approach (animPos, (float) index, 1.0f / 60.0f, 0.05f);
    if (std::abs (animPos - (float) index) < 0.005f) { animPos = (float) index; stopTimer(); }
    repaint();
}

void Segmented::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (control);
    g.fillRoundedRectangle (r, 5.0f);
    if (items.isEmpty()) return;
    const float cw = r.getWidth() / (float) items.size();
    const auto sel = juce::Rectangle<float> (r.getX() + animPos * cw, r.getY(), cw, r.getHeight()).reduced (2.0f);
    g.setColour (isEnabled() ? yellow : lineHi);
    g.fillRoundedRectangle (sel, 4.0f);
    for (int i = 0; i < items.size(); ++i)
    {
        const auto cell = juce::Rectangle<float> (r.getX() + (float) i * cw, r.getY(), cw, r.getHeight());
        const bool on = std::abs (animPos - (float) i) < 0.5f;
        const auto colour = on ? onYellow : (i == hover ? text : textDim);
        if (items[i].startsWith ("icon:"))
            icons::draw (g, items[i].substring (5), cell.reduced (5.0f, 5.0f), colour);
        else
        {
            g.setColour (colour);
            g.setFont (on ? bodyBold (12.0f) : body (12.0f));
            g.drawFittedText (items[i], cell.toNearestInt().reduced (3, 0), juce::Justification::centred, 1, 0.8f);
        }
    }
}

void Segmented::mouseDown (const juce::MouseEvent& e)
{
    if (items.isEmpty() || ! isEnabled()) return;
    setIndex ((int) ((float) e.x / ((float) getWidth() / (float) items.size())), true);
}

void Segmented::mouseMove (const juce::MouseEvent& e)
{
    const int h = items.isEmpty() ? -1 : juce::jlimit (0, items.size() - 1, (int) ((float) e.x / ((float) getWidth() / (float) items.size())));
    if (h != hover) { hover = h; repaint(); }
}

void Segmented::mouseExit (const juce::MouseEvent&) { hover = -1; repaint(); }

// ---- Section ---------------------------------------------------------------------------------------
Section::Section (const juce::String& t) : title (t) { setInterceptsMouseClicks (false, true); }

void Section::setPower (MUProcessor& p, const juce::String& paramId)
{
    power = std::make_unique<ParamToggle> (p, paramId, "", "power");
    power->onStateChange = [this] { repaint(); };
    addAndMakeVisible (*power);
    resized();
}

void Section::resized()
{
    if (power != nullptr) power->setBounds (10, 7, 20, 20);
}

void Section::paint (juce::Graphics& g)
{
    fillPanel (g, getLocalBounds().toFloat());
    auto header = getLocalBounds().removeFromTop (kHeader).reduced (12, 0);
    if (power != nullptr) header.removeFromLeft (26);
    g.setFont (bodyBold (13.0f));
    g.setColour (isOn() ? text : textDim);
    g.drawText (title, header, juce::Justification::centredLeft);
    if (readout.isNotEmpty())
    {
        g.setFont (body (12.0f));
        g.setColour (isOn() ? yellow : textMute);
        g.drawText (readout, header, juce::Justification::centredRight);
    }
}

// ---- IconButton ------------------------------------------------------------------------------------
IconButton::IconButton (const juce::String& iconName, const juce::String& helpKey) : juce::Button (iconName), icon (iconName)
{
    if (helpKey.isNotEmpty()) setHelp (*this, helpKey);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus (false);
}

void IconButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto r = getLocalBounds().toFloat();
    const bool on = getToggleState();
    g.setColour (on ? yellow : (highlighted || down) ? control.brighter (0.06f) : control);
    g.fillRoundedRectangle (r, 5.0f);
    const float pad = juce::jmax (4.0f, juce::jmin (r.getWidth(), r.getHeight()) * 0.24f);
    icons::draw (g, icon, r.reduced (pad), ! isEnabled() ? textMute : on ? onYellow : highlighted ? theme::text : theme::text.withAlpha (0.8f));
}

juce::TextButton& styleButton (juce::TextButton& b, const juce::String& text, bool primary, const juce::String& helpKey)
{
    b.setButtonText (text);
    b.getProperties().set ("primary", primary);
    b.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    b.setWantsKeyboardFocus (false);
    if (helpKey.isNotEmpty()) setHelp (b, helpKey);
    return b;
}

//==================================================================== PresetStrip
PresetStrip::PresetStrip (const juce::String& helpKey, const juce::String& emptyText) : empty (emptyText)
{
    styleButton (previous, "<", false, helpKey);
    styleButton (name, emptyText, false, helpKey);
    styleButton (next, ">", false, helpKey);
    previous.onClick = [this] { step (-1); };
    next.onClick = [this] { step (1); };
    name.onClick = [this] { showMenu(); };
    addAndMakeVisible (previous); addAndMakeVisible (name); addAndMakeVisible (next);
}

void PresetStrip::enableOwnPresets (MUProcessor& p, std::function<juce::String()> scopeOf, std::function<void (const juce::String&)> say)
{
    proc = &p; scope = std::move (scopeOf); status = std::move (say);
}

void PresetStrip::refresh()
{
    auto now = current ? current() : juce::String();
    // a star after the name: a control has moved since this preset was loaded
    if (now.isNotEmpty() && proc != nullptr && scope && proc->presetEdited (scope())) now << " *";
    if (now != shown || first)
    {
        first = false; shown = now;
        name.setButtonText (now.isEmpty() ? empty : now);
    }
}

void PresetStrip::step (int direction)
{
    if (! names || ! choose) return;
    auto list = names();
    const int builtIn = list.size();
    if (proc != nullptr && scope) for (auto& own : UserPresets::list (scope())) list.add (own.name);
    if (list.isEmpty()) return;
    const int at = list.indexOf (current ? current() : juce::String());
    const int target = at < 0 ? (direction > 0 ? 0 : list.size() - 1) : (at + direction + list.size()) % list.size();
    if (target < builtIn) choose (target);
    else if (status) status (proc->loadUserPreset (scope(), list[target]));
    else proc->loadUserPreset (scope(), list[target]);
    refresh();
}

//==================================================================== someone's own presets
namespace ownpresets
{
namespace
{
std::unique_ptr<juce::FileChooser>& chooser() { static std::unique_ptr<juce::FileChooser> c; return c; }

// Asks for a name inside the plugin's own window. (A separate dialog window does not reliably get the keyboard
// when the plugin is running inside a host.)
class NamePrompt : public juce::Component
{
public:
    NamePrompt (const juce::String& t, const juce::String& m, const juce::String& initial, std::function<void (const juce::String&)> d)
        : title (t), message (m), done (std::move (d))
    {
        box.setFont (body (14.0f));
        box.setText (initial, false);
        box.setIndents (10, 8);
        box.setTextToShowWhenEmpty ("Name", textMute);
        box.onReturnKey = [this] { finish (true); };
        box.onEscapeKey = [this] { finish (false); };
        addAndMakeVisible (box);
        styleButton (save, "Save", true);
        save.onClick = [this] { finish (true); };
        styleButton (cancel, "Cancel", false);
        cancel.onClick = [this] { finish (false); };
        addAndMakeVisible (save); addAndMakeVisible (cancel);
        setWantsKeyboardFocus (true);
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (black.withAlpha (0.72f));
        fillPanel (g, card.toFloat(), raised, 8.0f);
        g.setColour (yellow); g.fillRoundedRectangle (card.toFloat().withHeight (3.0f), 1.5f);
        auto r = card.reduced (22, 18);
        g.setFont (headline (20.0f)); g.setColour (text);
        g.drawText (title.toUpperCase(), r.removeFromTop (28), juce::Justification::centredLeft);
        r.removeFromTop (4);
        drawWrapped (g, message, r.removeFromTop (44).toFloat(), body (12.5f), textDim, 1.35f);
    }
    void resized() override
    {
        card = getLocalBounds().withSizeKeepingCentre (juce::jmin (460, getWidth() - 40), 210);
        auto r = card.reduced (22, 18);
        r.removeFromTop (28 + 4 + 44 + 6);
        box.setBounds (r.removeFromTop (36));
        auto buttons = r.removeFromBottom (32);
        save.setBounds (buttons.removeFromRight (110));
        buttons.removeFromRight (8);
        cancel.setBounds (buttons.removeFromRight (96));
    }
    ~NamePrompt() override { setLookAndFeel (nullptr); }
    void mouseDown (const juce::MouseEvent& e) override { if (! card.contains (e.getPosition())) finish (false); }
    void focus() { box.grabKeyboardFocus(); box.selectAll(); }

private:
    void finish (bool accepted)
    {
        const auto typed = box.getText();
        auto callback = accepted ? done : std::function<void (const juce::String&)>();
        juce::Component::SafePointer<NamePrompt> self (this);
        juce::MessageManager::callAsync ([self, callback, typed]
        {
            if (self != nullptr) { if (auto* parent = self->getParentComponent()) parent->removeChildComponent (self.getComponent()); delete self.getComponent(); }
            if (callback) callback (typed);
        });
    }
    juce::String title, message;
    std::function<void (const juce::String&)> done;
    juce::TextEditor box;
    juce::TextButton save, cancel;
    juce::Rectangle<int> card;
};

void askForName (juce::Component& near, const juce::String& title, const juce::String& message, const juce::String& initial, std::function<void (const juce::String&)> done)
{
    auto* top = near.getTopLevelComponent();
    if (top == nullptr) return;
    auto* prompt = new NamePrompt (title, message, initial, std::move (done));
    prompt->setLookAndFeel (&near.getLookAndFeel());      // the window it sits in may be the host's wrapper, which does not carry the brand look
    top->addAndMakeVisible (prompt);
    prompt->setBounds (top->getLocalBounds());
    prompt->toFront (true);
    prompt->focus();
}
}

juce::StringArray addToMenu (juce::PopupMenu& menu, MUProcessor& proc, const juce::String& scope)
{
    juce::StringArray own;
    const auto list = UserPresets::list (scope);
    const auto now = proc.scopePresetName (scope);
    bool nowIsOwn = false;
    menu.addSectionHeader ("My presets");
    if (list.empty()) menu.addItem (1, "None saved yet", false, false);
    for (size_t i = 0; i < list.size(); ++i)
    {
        own.add (list[i].name);
        nowIsOwn = nowIsOwn || list[i].name == now;
        menu.addItem (FirstOwn + (int) i, list[i].name, true, list[i].name == now);
    }
    menu.addSeparator();
    menu.addItem (SaveAs, "Save as...");
    if (nowIsOwn)
    {
        menu.addItem (Save, "Save changes to \"" + now + "\"", proc.presetEdited (scope));
        menu.addItem (Rename, "Rename \"" + now + "\"...");
        menu.addItem (Delete, "Delete \"" + now + "\"");
        menu.addItem (Export, "Export \"" + now + "\"...");
    }
    menu.addItem (Import, "Import a preset file...");
    menu.addItem (ShowFolder, "Show my presets folder");
    return own;
}

bool handle (int result, juce::Component& near, MUProcessor& proc, const juce::String& scope, const juce::StringArray& own, std::function<void (const juce::String&)> say)
{
    auto tell = [say] (const juce::String& s) { if (say) say (s); };
    const auto now = proc.scopePresetName (scope);
    juce::Component::SafePointer<juce::Component> safe (&near);
    auto* p = &proc;
    if (result >= FirstOwn)
    {
        if (result - FirstOwn < own.size()) tell (proc.loadUserPreset (scope, own[result - FirstOwn]));
        return true;
    }
    switch (result)
    {
        case SaveAs:
            askForName (near, "Save as a preset of your own", scope == "chain" ? "Saves every setting of this plugin on this track (not the track's role, its balance or the song's key)."
                                                                             : juce::String ("Saves this tool's settings. It will be listed under My presets in every song."),
                        UserPresets::list (scope).empty() || ! own.contains (now) ? juce::String() : now + " 2",
                        [p, scope, tell] (const juce::String& typed) { tell (p->saveUserPreset (scope, typed, false)); });
            return true;
        case Save:   tell (proc.saveUserPreset (scope, now, true)); return true;
        case Rename:
            askForName (near, "Rename the preset", {}, now, [p, scope, now, tell] (const juce::String& typed) { tell (p->renameUserPreset (scope, now, typed)); });
            return true;
        case Delete: tell (proc.deleteUserPreset (scope, now)); return true;
        case Import:
            chooser() = std::make_unique<juce::FileChooser> ("Choose a preset file", juce::File::getSpecialLocation (juce::File::userDesktopDirectory), juce::String ("*") + UserPresets::kExtension);
            chooser()->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [p, tell] (const juce::FileChooser& c)
            {
                if (c.getResult() != juce::File()) tell (p->importUserPreset (c.getResult(), true));
            });
            return true;
        case Export:
        {
            UserPreset preset;
            if (! UserPresets::find (scope, now, preset)) { tell ("That preset is no longer in your presets folder."); return true; }
            chooser() = std::make_unique<juce::FileChooser> ("Export the preset", juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile (preset.name + UserPresets::kExtension),
                                                             juce::String ("*") + UserPresets::kExtension);
            chooser()->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [preset, tell] (const juce::FileChooser& c)
            {
                if (c.getResult() == juce::File()) return;
                const auto done = UserPresets::exportTo (preset, c.getResult());
                tell (done.wasOk() ? "Exported \"" + preset.name + "\". Anyone with the plugin can load that file with Import." : done.getErrorMessage());
            });
            return true;
        }
        case ShowFolder:
        {
            auto folder = UserPresets::folder().getChildFile (scope);
            folder.createDirectory();
            folder.revealToUser();
            return true;
        }
        default: return false;
    }
}
}

Choice::Choice (MUProcessor& p, const juce::String& paramId, const juce::StringArray& i, const juce::String& helpKey) : proc (p), id (paramId), items (i)
{
    styleButton (button, items[0], false, helpKey.isNotEmpty() ? helpKey : paramId);
    button.onClick = [this] { showMenu(); };
    addAndMakeVisible (button);
    timerCallback();
    startTimerHz (8);
}

void Choice::timerCallback()
{
    const int now = juce::jlimit (0, juce::jmax (0, items.size() - 1), juce::roundToInt (proc.getParam (id)));
    if (now != shown) { shown = now; button.setButtonText (items[now]); }
}

void Choice::showMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());
    for (int i = 0; i < items.size(); ++i) menu.addItem (i + 1, items[i], true, i == shown);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&button).withMinimumWidth (button.getWidth()),
        [safe = juce::Component::SafePointer<Choice> (this)] (int result)
        {
            if (safe == nullptr || result <= 0) return;
            safe->proc.setParam (safe->id, (float) (result - 1));
            safe->timerCallback();
        });
}

void PresetStrip::showMenu()
{
    if (! names || ! choose) return;
    const auto list = names();
    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());
    const auto now = current ? current() : juce::String();
    if (proc != nullptr && scope && ! list.isEmpty()) menu.addSectionHeader ("Built in");
    for (int i = 0; i < list.size(); ++i) menu.addItem (i + 1, list[i], true, list[i] == now);
    juce::StringArray own;
    if (proc != nullptr && scope) own = ownpresets::addToMenu (menu, *proc, scope());
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&name).withMinimumWidth (name.getWidth()),
        [safe = juce::Component::SafePointer<PresetStrip> (this), own] (int result)
        {
            if (safe == nullptr || result <= 0 || ! safe->choose) return;
            if (safe->proc != nullptr && safe->scope && ownpresets::handle (result, *safe, *safe->proc, safe->scope(), own, safe->status)) { safe->refresh(); return; }
            safe->choose (result - 1);
            safe->refresh();
        });
}

void useTabPresets (PresetStrip& strip, MUProcessor& proc, const juce::String& scope, std::function<void (const juce::String&)> say)
{
    strip.names = [] { return juce::StringArray(); };
    strip.current = [&proc, scope] { return proc.getModulePresetName (scope); };
    strip.choose = [] (int) {};
    strip.enableOwnPresets (proc, [scope] { return scope; }, std::move (say));
}

void PresetStrip::resized()
{
    auto r = getLocalBounds();
    previous.setBounds (r.removeFromLeft (26));
    next.setBounds (r.removeFromRight (26));
    name.setBounds (r.reduced (3, 0));
}

void layoutRow (juce::Rectangle<int> area, std::initializer_list<juce::Component*> items, int gap)
{
    const int n = (int) items.size();
    if (n == 0) return;
    const float cell = ((float) area.getWidth() - (float) gap * (float) (n - 1)) / (float) n;
    int i = 0;
    for (auto* c : items)
    {
        const int x0 = area.getX() + juce::roundToInt ((cell + (float) gap) * (float) i);
        const int x1 = area.getX() + juce::roundToInt ((cell + (float) gap) * (float) i + cell);
        if (c != nullptr) c->setBounds (x0, area.getY(), x1 - x0, area.getHeight());
        ++i;
    }
}

void drawReductionBar (juce::Graphics& g, juce::Rectangle<float> r, float reductionDb, float maxDb)
{
    g.setColour (control);
    g.fillRoundedRectangle (r, 2.0f);
    const float amount = juce::jlimit (0.0f, 1.0f, std::abs (reductionDb) / maxDb);
    if (amount > 0.005f)
    {
        g.setColour (yellow);
        g.fillRoundedRectangle (r.withHeight (juce::jmax (2.0f, r.getHeight() * amount)), 2.0f);
    }
}
}
