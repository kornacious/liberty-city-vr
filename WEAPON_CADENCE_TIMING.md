# Tracked firearm cadence repair

## Cause

The tracked on-foot firing path scheduled its next shot using
`CWeaponInfo::m_nFiringRate`. In the clean re3 source, that field is loaded but
never used to pace normal player attacks. `CPed::Attack` instead fires at an
animation event and restarts the attack at `m_fAnimLoopStart` after
`m_fAnimLoopEnd`. The authored loop fields are converted from 30 Hz animation
frames into seconds by `CWeaponInfo::LoadWeaponData`.

Local inspection of the user's GTA III weapon data confirmed that using the
unused field gives the reported ordering: the rifles have a tiny delay while
the Uzi has a much longer one. The old VR implementation therefore made rifle
cadence approach one shot per rendered game frame and made the Uzi too slow.
Its `next = now + delay` scheduling also discarded fractional time/overshoot
on every shot. Releasing the trigger cleared the deadline, allowing rapid
semi-auto taps to bypass an intended cooldown.

The vehicle tracked sidearm path had the same field misuse and an additional
defect: after `CWeapon::Fire` started reloading on the final round, the caller
replaced `m_nTimer` with its shorter shot deadline. That timer belongs to the
native weapon reload/state machine.

## New timing

Normal tracked gun repeats use `(loopEnd - loopStart) * 1000` milliseconds.
This is elapsed game time, not a rendered-frame counter. Fractional time and
ordinary frame overshoot carry into the next shot. Automatic guns can deliver
more than one round during a slow frame, bounded to four shots per step. A
gap over 250 ms drops firing debt; idle time, lost aim/tracking, empty clips,
reloads and blocked shots do not build up a burst to fire on recovery.

Nominal continuous-fire rates from the locally inspected authored loops:

| Gun | Repeat interval | Nominal rounds/second |
| --- | ---: | ---: |
| Uzi | 100 ms | 10 |
| AK47 | 66.67 ms | 15 |
| M16 | 33.33 ms | 30 |
| Flamethrower | 33.33 ms | 30 ammo units |

For example, the nominal M16 first-to-last-shot duration for a 60-round clip
is about 1.97 seconds, plus at most a frame of observation latency under normal
pacing. Previously, a 60-round clip could take about 0.82 seconds at 72 FPS or
0.66 seconds at 90 FPS before accounting for actual frame pacing.

Pistol/shotgun trigger edges retain semi-auto operation while honoring the
authored repeat interval. Sniper and rocket launcher cadence remains limited
by native reload duration rather than the long third-person launcher loop.
Tracked vehicle Uzi firing uses the native drive-by interval of 70 ms; a vehicle
pistol uses its normal attack loop. The inventory weapon owns a shared clock,
so moving it between hands or between these paths does not create an extra
cooldown. `CWeapon::m_nTimer` is no longer repurposed for tracked shot timing.
Manual magazine insertion still requires a deliberate trigger release.

The first trigger shot remains immediate for VR. This repair uses the native
authored repeat cadence without introducing an initial attack-animation delay.
It does not change NPC attack timing, ballistics, damage, native ammunition
accounting or reload duration. The unused firing-rate field is only a fallback
for invalid/missing animation loops.

## Verification

The host suite extracts the actual patched on-foot and vehicle firing blocks,
the native successful-shot ammunition transaction, `CWeapon::Reload` and
`CWeapon::Update`, and compiles them with the production cadence clock.
Ballistics, controller input, sound/effects and world queries are adapters.
No original game data files are copied into fixtures or committed.

61,576 checks pass across 15/30/45/60/72/80/90/120/144/180 FPS and variable
frame durations. Synthetic loop widths cover all firearm branches, controlled
magazine depletion, semi-auto press/hold/repress behavior, independent held
guns, hand transfers, failed shots, tracking loss, stalls, timer wrap, invalid
loop data, automatic/manual reloads and vehicle reload timer ownership.
Magazine depletion stays within one frame of the expected elapsed-time cadence.

Independent negative checks restore use of the unused firing-rate field and
overwrite the vehicle reload timer. They fail the shot-count and native reload
deadline assertions respectively.

The freshly assembled source passed all 43 supported host suites; eight
reference-only fixtures were excluded. The Android debug APK compiled
successfully and was installed with `adb install -r` on verified Quest 3
`REDACTED_QUEST_SERIAL`. Installation succeeded without uninstalling or clearing data.

These are source/host checks, not recorded Windows or Quest play measurements.
The user's consistent Windows reference is useful acceptance evidence; this
session does not claim a recorded desktop-to-headset timing match. Native
animation event crossing and first-person desktop control paths can differ
from a direct VR trigger. Confirm in-headset by emptying magazines at steady
and varying refresh rates, checking pistol/shotgun taps and both reload modes,
then comparing with the user's Windows reference. At low FPS, catch-up rounds
use the current tracked aim, not reconstructed historical controller poses.

## Optional pistol trigger mode

The VR Weapons page now has `PISTOL FIRE` after `MANUAL RELOADING`:

- `SINGLE SHOT` requires one trigger pull per round and remains the default.
- `HOLD TO REPEAT` continues firing at the corrected pistol cadence while held.

The choice is saved as `[VR] PistolHoldToRepeat=0/1` in `vr_settings.ini`.
It applies to tracked pistols in either hand, including immersive vehicle
sidearms. It does not change the native Windows control path or the trigger
behavior of other weapons. Native/manual reload gates and inventory-owned
cooldowns remain in effect in both modes.

The cadence suite additionally exercises both choices across five frame rates,
both hands and both firing contexts, release/repress and switching back to
single shot. A second native test extracts the settings functions and Weapons
page renderer to check defaults, persistence, menu mapping and all nine rows.
All 43 supported suites passed, including 61,696 firing checks and 70 settings/
menu checks. The APK built and installed successfully on the personal Quest 3
device. An unrelated unfinished Vulkan workspace edit failed to compile;
the disposable APK assembly used the committed `vkdevice.cpp` instead, leaving
that workspace edit untouched. In-headset menu appearance remains a manual check.
