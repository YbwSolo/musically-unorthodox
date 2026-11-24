#include "Course.h"

namespace mu::course
{
const std::vector<Track>& tracks()
{
    static const std::vector<Track> all = {
        { "Music theory", "Theory", "Time, notes, scales, chords and melody: what works, and why.", theoryLessons() },
        { "Make a beat", "Beats", "From an empty project to a finished beat, in the order it is done.", beatLessons() },
        { "In Logic Pro", "Logic", "The same beat, step by step in Logic Pro.", logicLessons() },
        { "In FL Studio", "FL", "The same beat, step by step in FL Studio.", flLessons() },
        { "Mixing", "Mixing", "How EQ, compression and loudness work, with this plugin.", mixingLessons() } };
    return all;
}
}
