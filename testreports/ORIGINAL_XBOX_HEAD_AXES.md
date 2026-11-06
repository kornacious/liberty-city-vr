# Original Xbox head camera axis correction

Local Windows work, 2026-11-06. Original assets remained local and unchanged.

The preceding direct-head attachment assumed the head matrix's `at` axis was
vertical. Xbox Claude's bone convention uses `right` (+X) vertically, `up` (+Y)
forward and `at` (+Z) left. The local retail bind hierarchy and re3's skinned-head
IK agree: `CPedIK::RotateHead` uses bone X for yaw and bone Z for pitch.

The camera now decodes camera-up from bone X, forward from bone Y and right
from negative bone Z. There is no constant Euler turn or heading compensation.
Head position, full animated rotation, same-frame carrier following and headset
tracking are retained; Classic and third-person paths remain unchanged.

The previous synthetic fixture used generic camera-aligned bone axes, so it
missed the authored Xbox convention. It now uses the Xbox convention throughout
and has an explicit neutral upright case requiring world +Z up, +X right and
+Y forward. Animated pitch/roll cases compare against independently constructed
camera axes while preserving the head position.

Validation:

- 21,617 native head attachment and final camera checks passed.
- Full ARM64 debug APK built and updated the personal Quest 3 device.
- No asset reimport or camera calibration change is required.

Visual headset confirmation remains pending. Verify a level startup horizon,
normal heading and head turns, followed by train boarding and knockdown recovery.
