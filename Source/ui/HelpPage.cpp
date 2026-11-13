#include "HelpPage.h"
#include "Theme.h"

namespace mu
{
void HelpPage::Content::paint (juce::Graphics& g)
{
    if (sections == nullptr) return;
    for (auto& s : *sections)
    {
        auto r = s.body->getBounds();
        theme::drawSectionTitle (g, juce::Rectangle<int> (r.getX() - 12, r.getY() - 30, r.getWidth() + 12, 24), s.title);
    }
}

HelpPage::HelpPage (MUProcessor& p) : proc (p)
{
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    content.sections = &sections;

    addSection ("The 5 minute workflow",
        "1. Put Musically Unorthodox LAST on every track you want balanced, after the plugins the song was mixed with.\n"
        "2. On each one, load a CHAIN from the header (MU Lead Vocal, MU 808, MU Beat...) or just set the ROLE.\n"
        "3. Put one on the Stereo Out and load MU Master Prep. That instance runs the session.\n"
        "4. Play the loudest section of the song. On the master's MIX tab press LEARN & BALANCE and keep it playing for 20 seconds.\n"
        "5. Every track's trim moves so the vocal sits on top and the song lands on the target. Check the LEVELS tab on the master: when the ticks are green, bounce it for mastering.\n\n"
        "Nothing here replaces listening. The plugin gets you to a correct, consistent starting point fast; your ears make the record.");

    addSection ("Why the numbers matter",
        "Mastering engineers want a mix that is loud enough to judge but with room to work. The targets we use by default are -18 LUFS integrated and -6 dBTP true peak with no limiter on the master. "
        "Trap, drill and UK rap mixes often come in hotter (-14 to -12 LUFS); that is fine as long as the peaks are controlled and it is not pre-limited. Change the targets on the MIX tab if the brief asks for something else.\n\n"
        "LUFS is loudness the way ears hear it, not meter peaks. Two tracks at the same peak can be 10 dB apart in loudness. That is why we balance by LUFS and only use peaks as a ceiling.");

    addSection ("Roles and targets",
        "Each instance has a ROLE. The balance works in LU relative to the lead vocal: by default the beat sits 1.5 LU under the vocal, the 808 2.5 under, kick 3.5 under, backing vocals 6 under, FX 12 under. "
        "These are starting points for a hip-hop / R&B balance. You can change every one of them in the Targets panel on the master's MIX tab, and they are saved with the project.\n\n"
        "Auto detect guesses the role from the sound. It is good at bass vs vocal vs beat; set it yourself when you can.");

    addSection ("Learn & Balance vs Auto balance",
        "LEARN & BALANCE wipes every instance's reading, listens for 20 seconds and then applies one set of moves. Use it on the loudest section (the hook). It is the most accurate way to hit the target.\n\n"
        "BALANCE NOW applies the current plan using whatever has been measured so far.\n\n"
        "AUTO BALANCE keeps gliding every track towards the plan while the song plays. Good while you are still arranging; switch it off once you are happy so you can ride faders by hand without it fighting you.\n\n"
        "AUTO TRIM lets the master instance adjust its own gain so the integrated number lands exactly on the target. PEAK GUARD is a transparent 2 ms look-ahead limiter at the ceiling, a safety net only.");

    addSection ("The chain on each track",
        "TRIM > GATE > EQ > COMPRESSOR > DE-ESSER > SATURATION > RIDER > VOCAL SPACE > OUTPUT\n\n"
        "GATE closes between phrases so room noise and headphone bleed disappear. HOLD keeps word endings.\n"
        "EQ is six bands; any bell or shelf can be DYNAMIC, meaning it only moves when the level in that band crosses the threshold. That is how we handle mud (200-350 Hz), harshness (2.5-5 kHz) and masking without making the track thin when it is quiet.\n"
        "COMPRESSOR evens out the performance. AUTO SET reads the track's level and role and picks threshold, ratio and timing (4:1 fast for a lead vocal, 2:1 slow for a beat).\n"
        "DE-ESSER controls S and T sounds. Split band only turns down the top end during an ess.\n"
        "SATURATION adds harmonics so a vocal sounds dense and an 808 reads on phone speakers. It is level-compensated; listen for weight, not volume.\n"
        "RIDER is a slow automatic fader that keeps the vocal at a consistent level, like riding the fader by hand.\n"
        "VOCAL SPACE lives on beat and instrument instances. It dips the beat at 260 Hz, 2.6 kHz and 7 kHz only while the lead vocal instance is singing.");

    addSection ("Clean Harsh and masking fixes",
        "CLEAN HARSH (EQ tab) looks at the long-term spectrum of the track for narrow peaks that stick out between 1.2 and 12 kHz and adds dynamic cuts for them. Play the track for ten seconds first.\n\n"
        "The MASKING list on the master's MIX tab compares every pair of tracks and flags where a lower-priority track is sitting on top of a higher one (a beat on the vocal's presence, a pad on the 808). FIX adds a dynamic cut on the track that should give way.");

    addSection ("Common problems",
        "Session table only shows this instance: the other instances are in a different host process. Logic, Ableton, FL, Reaper and Cubase run plugins in-process, so this is rare; check nothing is running in Rosetta.\n\n"
        "A track hit the 12 dB move limit: its own fader or gain staging is far off. Fix the fader in the DAW and run Learn & Balance again.\n\n"
        "Vocal Space does nothing: there is no instance with the role Lead vocal, or this instance's role is a vocal or master.\n\n"
        "Integrated reading is stuck: it only counts while the track is playing above -70 LUFS; press RESET METERS on the LEVELS tab after big changes.\n\n"
        "Latency: the plugin reports a small fixed delay (saturation oversampling plus the peak guard look-ahead) and the DAW compensates for it automatically.");

    addSection ("About",
        juce::String ("Musically Unorthodox mix assistant v") + MU_VERSION_STRING + ". Built for the MU studio in Northampton for residency and camp sessions. "
        "Everything runs on this computer; nothing is sent anywhere.\n\nmusicallyunorthodox.com");
}

void HelpPage::addSection (const juce::String& title, const juce::String& body)
{
    Section s;
    s.title = title;
    s.body = std::make_unique<TextBlock>();
    s.body->setText (body, 12.5f, theme::textDim);
    content.addAndMakeVisible (*s.body);
    sections.push_back (std::move (s));
}

void HelpPage::paint (juce::Graphics& g) { g.fillAll (theme::bg); }

void HelpPage::layoutContent()
{
    const int w = viewport.getWidth() - 10;
    const int textW = juce::jmin (760, w - 48);
    int y = 24;
    for (auto& s : sections)
    {
        y += 30;
        const int h = s.body->heightForWidth (textW);
        s.body->setBounds (30, y, textW, h);
        y += h + 26;
    }
    content.setSize (w, y + 16);
}

void HelpPage::resized()
{
    viewport.setBounds (getLocalBounds().reduced (18, 0));
    layoutContent();
}
}
