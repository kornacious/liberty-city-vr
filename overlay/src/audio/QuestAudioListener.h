#pragma once

namespace QuestAudio {
struct Listener {
    float left[3] = {-1, 0, 0};
    float forward[3] = {0, 1, 0};
    float up[3] = {0, 0, 1};
    float position[3] = {};

    void Translate(const float world[3], float local[3]) const
    {
        const float delta[3] = {world[0] - position[0], world[1] - position[1], world[2] - position[2]};
        local[0] = left[0]*delta[0] + left[1]*delta[1] + left[2]*delta[2];
        local[1] = forward[0]*delta[0] + forward[1]*delta[1] + forward[2]*delta[2];
        local[2] = up[0]*delta[0] + up[1]*delta[1] + up[2]*delta[2];
    }
};
}
