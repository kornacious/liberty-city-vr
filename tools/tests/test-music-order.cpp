#include "QuestMusicPlaylist.h"
#include "RadioTitleMarquee.h"
#include <cassert>
#include <sstream>
#include <iostream>
int main() {
    std::istringstream order("c.mp3\na.mp3\nc.mp3\nmissing.mp3\n../bad.mp3\n");
    auto names = QuestMusic::OrderedNames({"b.mp3", "a.mp3", "c.mp3", "new.mp3"}, order);
    assert((names == std::vector<std::string>{"c.mp3", "a.mp3", "b.mp3", "new.mp3"}));
    assert(RadioTitle::Glyphs("Beyonc\xc3\xa9") == "Beyonce");
    assert(RadioTitle::Glyphs("\xe6\x9b\xb2") == "?");
    assert(RadioTitle::LeftToRight(0,100,16,496) == 206);
    const double travel=(600.+480.)/40.;
    assert(RadioTitle::LeftToRight(2,600,16,496) > RadioTitle::LeftToRight(1,600,16,496));
    assert(RadioTitle::LeftToRight(travel+1,600,16,496) == 496);
    assert(RadioTitle::LeftToRight(travel+2.9,600,16,496) == 496);
    assert(RadioTitle::LeftToRight(travel+3,600,16,496) == -584);
    std::cout << "PASS native playlist ordering, stale entries, UTF-8 glyphs and marquee direction/pause/repeat\n";
}
