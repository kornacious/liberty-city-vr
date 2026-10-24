#pragma once
#include "VehiclePlacement.h"
#include <vector>

namespace editor {
using Reviews = std::map<std::string, std::string>;
class History {
    struct Snapshot {
        VehiclePlacement::Pack pack;
        Reviews reviews;
    };
    std::vector<Snapshot> past, future;

  public:
    void clear() {
        past.clear();
        future.clear();
    }
    void capture(const VehiclePlacement::Pack &pack, const Reviews &reviews) {
        past.push_back({pack, reviews});
        if (past.size() > 200)
            past.erase(past.begin());
        future.clear();
    }
    bool undo(VehiclePlacement::Pack &pack, Reviews &reviews) {
        if (past.empty())
            return false;
        future.push_back({pack, reviews});
        pack = past.back().pack;
        reviews = past.back().reviews;
        past.pop_back();
        return true;
    }
    bool redo(VehiclePlacement::Pack &pack, Reviews &reviews) {
        if (future.empty())
            return false;
        past.push_back({pack, reviews});
        pack = future.back().pack;
        reviews = future.back().reviews;
        future.pop_back();
        return true;
    }
};
} // namespace editor
