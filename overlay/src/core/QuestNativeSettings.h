#pragma once
#include <string>
#include <vector>

namespace QuestNativeSettings {
struct Row {
    std::string label, value;
    bool selected, available;
    float sliderProgress = -1.0f;
    Row(const char *text, bool chosen, bool enabled) : selected(chosen), available(enabled) {
        std::string line(text);
        if(line.compare(0, 2, "* ") == 0) line.erase(0, 2);
        const auto open = line.find('<'), close = line.find('>', open);
        if(open == std::string::npos || close == std::string::npos) label = line;
        else {
            label = line.substr(0, open);
            value = line.substr(open+1, close-open-1)+line.substr(close+1);
        }
        Trim(label); Trim(value);
    }
    static void Trim(std::string &text) {
        const auto first = text.find_first_not_of(' ');
        if(first == std::string::npos) { text.clear(); return; }
        text = text.substr(first, text.find_last_not_of(' ')-first+1);
    }
};
struct Page {
    bool categories = false;
    int radioStation = -1;
    bool customRadioAvailable = false;
    std::string heading, subtitle;
    std::vector<Row> rows;
    std::vector<std::string> notes;
    enum { RowsPerPage = 10 };
    int Selected() const {
        for(size_t i=0;i<rows.size();++i) if(rows[i].selected) return (int)i;
        return 0;
    }
    int First() const { return Selected()/RowsPerPage*RowsPerPage; }
};
}
namespace androidgame {
bool VrCollectNativeSettings(QuestNativeSettings::Page &page);
void VrDrawNativeSettings();
}
