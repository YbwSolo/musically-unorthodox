#include "Course.h"

// The same beat, made in Logic Pro and in FL Studio. Key commands and menu names were checked against the
// makers' own manuals (Apple's Logic Pro User Guide, Image-Line's FL Studio manual) when these were written.
// Both programs move things between versions, so each track says how to find something that has moved.
namespace mu::course
{
using S = Scene;

std::vector<Lesson> logicLessons()
{
    return {
    { "lg.around", "Logic Pro", "Find your way around", {
        { "One window",
          "Logic keeps everything in one main window. Tracks run across the middle. The control bar along the top has play, record, the tempo and the key. Panels open and close around the edges. Move the mouse over each area of the picture to read what it is for.", S::Daw, 0 },
        { "The keys worth learning first",
          "You can make a whole beat with these. Space plays and stops. R records. The single letters open and close panels: Y for the Library of sounds, P for the piano roll, X for the mixer, O for the loop browser. If a key does something else on your Mac, look it up under Logic Pro, Key Commands, Edit Assignments.", S::Card, 0 },
        { "Start a project",
          "Choose File, New. Pick Software Instrument and press Create. A software instrument track plays sounds from inside the computer, which is what you want for melodies, drums and 808s. An audio track is for recording a microphone or dragging in a sample.", S::Daw, 0 },
        { "If something is not where this says",
          "Apple renames and moves things between versions. Open the Help menu and type the name into the search box: Logic points at the menu item. The names used in these lessons are the ones in Apple's Logic Pro User Guide.", S::Card, 0 } },
      nullptr, kTabNone, nullptr },

    { "lg.tempo", "Logic Pro", "Tempo, key and metronome", {
        { "Set the tempo",
          "The tempo is in the display at the top centre of the window. Double-click the number, type the BPM and press Return. Do this before you record anything: recordings follow the tempo that was set when they were made.", S::Pulse, 1 },
        { "Metronome and count-in",
          "Press K to switch the metronome click on and off. The button next to it, marked 1234, gives a count-in before recording starts. Leave both on while you record and switch the click off to listen back.", S::Pulse, 0 },
        { "Set the key",
          "The key signature is shown next to the tempo. Click it and choose your key, for example C minor. This does not stop you playing wrong notes, but Logic's loops, its Session Players and the piano roll's scale tools all follow it.", S::Keys, 4 },
        { "Loop a section while you work",
          "Press C to switch Cycle on. Drag in the ruler at the top to set the yellow strip over four or eight bars. Logic now loops that section so you can keep adding parts without stopping.", S::Pulse, 2 } },
      nullptr, kTabNone, nullptr },

    { "lg.notes", "Logic Pro", "Instruments and the piano roll", {
        { "Choose a sound",
          "Select the track and press Y to open the Library on the left. Pick a category and a patch: the track loads it at once. Press Command and K to open Musical Typing and play notes on the computer keyboard, so you can audition sounds without a MIDI keyboard.", S::Card, 1 },
        { "Record or draw",
          "To record, press R and play. To draw instead, hold Control and click an empty part of the track and choose Create MIDI Region, then double-click the region or press P to open the piano roll. Hold Command and the pointer becomes a pencil: click to add a note, drag its right edge to change its length.", S::Roll, 0 },
        { "Stay in key",
          "In the piano roll, the panel on the left has Scale Quantize. Choose the root and the scale, select the notes with Command and A, and Logic moves any note outside the key to the nearest note inside it. Use it as a check after you play something in.", S::Keys, 5 },
        { "Fix the timing",
          "Select the notes and press Q. They snap to the grid value set in Time Quantize in the same panel: choose 1/16 for most melodies and drums. If it sounds too stiff afterwards, lower the Strength slider so the notes move most of the way and keep a little of your feel.", S::Grid, 1 },
        { "Repeat and vary",
          "Select a region and press Command and R to repeat it straight after itself. Then open the copy and change the ending. That is \"same, same, different\" done in three key presses.", S::Roll, 2 } },
      nullptr, kTabNone, nullptr },

    { "lg.drums", "Logic Pro", "Drums: kits and the Step Sequencer", {
        { "Load a kit",
          "Make a new software instrument track with Option, Command and N. Press Y for the Library and choose Electronic Drum Kit, then a kit. These kits open in Drum Machine Designer, a grid of pads where every pad is one drum sound that you can swap for your own sample by dragging it onto the pad.", S::Card, 2 },
        { "Make a pattern region",
          "Hold Control and click an empty part of the drum track and choose Create Pattern Region. The Step Sequencer opens at the bottom: one row for each drum, sixteen steps across, exactly like the grid in these lessons. Click a step to switch it on.", S::Grid, 2 },
        { "Velocity and hi-hat rolls",
          "The Step Sequencer's own menu bar has an edit mode selector. Switch it from Step On/Off to Velocity and drag steps up and down to make hits softer or harder. Switch it to Note Repeat and a single step plays two, three or four fast hits: that is a hi-hat roll.", S::Grid, 7 },
        { "Length and feel",
          "The pattern length and the step rate are at the top of the sequencer: 16 steps at a rate of 1/16 is one bar. Each row can also have its own rate, so the hats can run in triplets while the kick stays straight. The pattern's Swing setting leans every second step late.", S::Grid, 3 },
        { "Prefer the piano roll?",
          "Drums work in the piano roll too. Create a MIDI region instead of a pattern region and press P. Each drum is a note: the kick is usually C1 and the snare D1, and the kit's own names show at the left edge.", S::Grid, 4 } },
      nullptr, kTabNone, nullptr },

    { "lg.808", "Logic Pro", "808s in Logic", {
        { "Load an 808 into Quick Sampler",
          "Drag an 808 sample from the Finder or the loop browser onto the empty space below the track headers and choose Quick Sampler (Original). Logic makes a new instrument that plays the sample across the keyboard, lower keys for lower notes.", S::Card, 2 },
        { "Tune it",
          "Quick Sampler shows the root key it has detected for the sample. Play C on the keyboard beside a piano playing C. If they disagree, change the root key, or the Tune control, until they match. An 808 that is out of tune makes the whole beat sound wrong.", S::Keys, 1 },
        { "One note at a time, with glide",
          "Find Quick Sampler's polyphony setting and choose Mono or Legato, so it plays one note at a time and a new 808 note cuts the old one off. Then raise its Glide control. Now when two notes overlap in the piano roll, the pitch slides from the first to the second: the 808 slide.", S::Roll, 15 },
        { "Write the line",
          "Open the piano roll and put 808 notes where the kicks are, on the root note of the key. Move one or two to other notes of the scale. For a slide, draw the next note so it starts just before the first one ends.", S::Roll, 11 } },
      nullptr, kTabNone, nullptr },

    { "lg.finish", "Logic Pro", "Arrange, mix and bounce", {
        { "Lay out the song",
          "Select all the regions of your loop and press Command and R to repeat them along the timeline. Then make sections by deleting: remove the drums from the first four bars for an intro, mute a melody layer in the verses. Press G to show the global tracks and add markers to name each section.", S::Song, 1 },
        { "The mixer",
          "Press X. Every track has a fader, a pan knob and Audio FX slots. Click an empty Audio FX slot and choose Audio Units, then Musically Unorthodox, to put this plugin on a track. Set levels first, with the faders, before you add anything.", S::Layers, 3 },
        { "Bounce the beat",
          "Set the cycle strip over the whole song, or let Logic use the end of the last region. Press Command and B. Under PCM choose the Wave file format and 24 bit, and switch Normalize off so the level you mixed is the level you get. Tick MP3 as well if you need one to send.", S::Card, 3 },
        { "Export the stems",
          "Choose File, Export, All Tracks as Audio Files. Logic writes one file per track, all starting at the same point so they line up in any other program. This is what an engineer or an artist's studio will ask you for.", S::Card, 3 } },
      nullptr, kTabNone, nullptr },
    };
}

std::vector<Lesson> flLessons()
{
    return {
    { "fl.around", "FL Studio", "Find your way around", {
        { "Five windows",
          "FL Studio is built from five windows that float over each other. The Channel rack holds your sounds and a step grid. The Piano roll is where notes are written. The Playlist is where patterns are laid out into a song. The Mixer has the faders and effects. The Browser on the left is your sounds and files. Move the mouse over each to read more.", S::Daw, 1 },
        { "The function keys",
          "Each window has a function key, and you will use them hundreds of times a session. F5 is the Playlist, F6 the Channel rack, F7 the Piano roll, F9 the Mixer. Alt and F8 shows the Browser. On a Mac laptop you may need to hold fn as well.", S::Card, 4 },
        { "Patterns and the song",
          "This is the idea that makes FL different. You build small patterns, such as a drum pattern or a melody pattern, in the Channel rack and Piano roll. Then you paint those patterns into the Playlist to make the song. The button beside play switches between hearing one pattern (PAT) and hearing the whole song (SONG). The L key switches too.", S::Song, 0 },
        { "If something is not where this says",
          "Image-Line moves things between versions. Press F1 in any window and the manual opens at the page for that window. The names used in these lessons are the ones in Image-Line's FL Studio manual.", S::Card, 4 } },
      nullptr, kTabNone, nullptr },

    { "fl.tempo", "FL Studio", "Tempo, metronome and patterns", {
        { "Set the tempo",
          "The tempo is the number in the toolbar at the top. Drag it up or down, or right-click it to pick a value or type one in. The same right-click menu has Tap: click along to a song and FL works out its BPM, exactly as you did in the Theory lessons.", S::Pulse, 4 },
        { "The metronome",
          "Hold Ctrl and press M, or click the metronome button in the toolbar, to hear a click while you play. The button beside it gives a count-in before recording.", S::Pulse, 0 },
        { "Play with the computer keyboard",
          "Hold Ctrl and press T to turn the typing keyboard into a piano. The same shortcut turns it off again. Remember it is on: while it is, single-letter shortcuts play notes instead of doing their job.", S::Keys, 0 },
        { "Make a new pattern for each part",
          "Keep each part in its own pattern: one for drums, one for the melody, one for the 808. Press F4 to make a new, empty pattern and F2 to name and colour it. Separate patterns are what let you drop the drums out of an intro later.", S::Song, 1 } },
      nullptr, kTabNone, nullptr },

    { "fl.notes", "FL Studio", "Instruments and the Piano roll", {
        { "Add an instrument",
          "In the Channel rack, click the plus button at the bottom and choose an instrument. FLEX is a good first choice, with hundreds of ready sounds. FL Keys is a simple piano. Click the channel's name to open it and pick a sound.", S::Card, 5 },
        { "Open the Piano roll",
          "Right-click the channel's name and choose Piano roll, or select the channel and press F7. Left-click to draw a note, drag its right edge to set the length, right-click to delete it. The next note you draw copies the length of the last one you touched.", S::Roll, 0 },
        { "Mark the scale",
          "FL can shade the keys of your scale so the notes outside the key stand out. In the Piano roll's menu, look for Scale highlighting, under Helpers or View depending on the version, then choose the root and the scale. Recent versions add a snap-to-scale switch that moves any note you edit onto the scale.", S::Keys, 5 },
        { "Chords with the stamp tool",
          "The stamp tool places a whole chord with one click. Choose a chord type from its menu, then click the root note in the Piano roll. Untick Only one in that menu to keep stamping. It is the fastest way to try a progression.", S::Keys, 7 },
        { "See your other parts",
          "With ghost notes on, the notes of the other channels in the same pattern show faintly behind the one you are editing. Press Alt and V, or look for Ghost channels in the Piano roll's menu. It lets you write a counter melody or an 808 line while looking at the melody.", S::Roll, 8 },
        { "Fix and repeat",
          "Ctrl and Q snaps the selected notes to the grid, or all of them if none are selected. Ctrl and B copies the selection, or everything in view, to the right: write one bar, press it, then change the ending.", S::Roll, 2 } },
      nullptr, kTabNone, nullptr },

    { "fl.drums", "FL Studio", "Drums in the Channel rack", {
        { "Load the sounds",
          "Open the Browser and find a drum folder, under Packs or wherever you keep your own kits. Click a sample to hear it. Drag a kick, a snare or clap, a closed hat and an open hat into the Channel rack. Each becomes a channel with a row of step buttons.", S::Card, 6 },
        { "Click in the pattern",
          "Left-click a step to switch it on, right-click to switch it off. Sixteen steps are one bar, shaded in groups of four so you can see the beats. This is the same grid you used in the drum lessons. Press Space to hear it loop.", S::Grid, 2 },
        { "Velocity with the graph editor",
          "Select a channel and click the small graph button at the top of the Channel rack. A row of bars appears under the steps: drag them to set how hard each hit is. Lower every second hat and the pattern starts to breathe.", S::Grid, 7 },
        { "Hi-hat rolls",
          "Steps are too coarse for a roll. Right-click the hat channel and open its Piano roll. Change the snap, the magnet at the top, to a finer value such as 1/4 step or 1/3 step for triplets, and paint a burst of short notes at the end of the bar.", S::Grid, 8 },
        { "Swing",
          "The Swing knob at the top of the Channel rack leans every second step late for all the step patterns at once. A little goes a long way: start around a quarter of the way up.", S::Grid, 3 } },
      nullptr, kTabNone, nullptr },

    { "fl.808", "FL Studio", "808s in FL Studio", {
        { "Load and tune",
          "Drag an 808 sample into the Channel rack and click its name to open the channel settings. At the bottom is a small keyboard with one key marked: the root note. Right-click the key that matches the sample's real pitch. Now when you play C, you hear C.", S::Keys, 1 },
        { "Stop notes overlapping",
          "Two 808s ringing at once turn to mud. In the channel settings, open the page with the Group settings and set the Cut and Cut by numbers to the same value, or use Cut itself from the channel's menu. Each new 808 note now stops the one before it.", S::Card, 6 },
        { "Write the line",
          "Open the 808's Piano roll and place notes where the kicks are, on the root of your key. Make them long enough to carry until the next note. Move one or two to other notes of the scale for variation.", S::Roll, 11 },
        { "Slide notes",
          "This is how drill and trap 808s bend. In the Piano roll's toolbar, switch on Slide, the small triangle, and draw a note above or below the 808 note while it is still sounding. The 808 bends to that pitch over the length of the slide note. Slide notes work on FL's own samplers and instruments.", S::Roll, 15 } },
      nullptr, kTabNone, nullptr },

    { "fl.finish", "FL Studio", "Arrange, mix and export", {
        { "Paint the song",
          "Press F5 for the Playlist and switch to SONG mode. Choose a pattern in the picker at the top, select the paint tool with B, and click and drag along a track to lay it down. Put the drums on one track, the melody on another, the 808 on a third. Right-click removes a block.", S::Song, 1 },
        { "Send every sound to the Mixer",
          "By default every channel plays through the master. Select all the channels in the Channel rack and press Ctrl and L: each gets its own Mixer track, named to match. Press F9 to see them.", S::Layers, 3 },
        { "Add effects, and this plugin",
          "Click a Mixer track, then click an empty slot in the rack on the right and choose an effect. Musically Unorthodox is listed with your installed plugins once FL has scanned for it: Options, Manage plugins, Find installed plugins. Set levels with the faders before you add anything.", S::Card, 7 },
        { "Export",
          "Hold Ctrl and press R to export a WAV, or Ctrl, Shift and R for an MP3. Choose Full song so the whole Playlist is rendered. For stems, tick Split mixer tracks in the same window: FL writes one file for each Mixer track, all the same length.", S::Card, 7 } },
      nullptr, kTabNone, nullptr },
    };
}
}
