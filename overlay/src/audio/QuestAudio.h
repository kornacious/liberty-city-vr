#pragma once

class CVector;

namespace QuestAudio {
// Game-thread listener snapshot, refreshed before servicing audio, without
// replacing the game camera used by simulation and visibility.
void UpdateListener();
const CVector &ListenerPosition();
void TranslateEntity(const CVector &world, CVector &local);
// World positions use the game's LEFT-positive listener coordinate system.
// Synthetic frontend positions encode stereo pan directly and bypass HRTFs.
void SetChannel2D(unsigned int channel, bool is2D);
}
