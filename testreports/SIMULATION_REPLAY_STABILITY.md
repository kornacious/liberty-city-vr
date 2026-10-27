# Replay props and route stability investigation

Local investigation on 2026-10-27. Only diagnostic tapes/reports were read from
the personal Quest 3 device. Original game assets were not needed for source
changes, host tests or APK compilation; diagnostic recordings remain local and
outside version control.

## Evidence

Of 21 available run reports, 20 ended with `ROUTE DRIFT EXCEEDED 50 METERS` after
about 7.0–42.7 simulation seconds. The one completed run was about 6.0 seconds,
not a completed two-minute route. The aborted reports contain substantial speed
and heading divergence, including runs whose recording and playback ghost flags
were both on. These aggregate reports identify the termination condition; they
cannot establish the exact collision or physics step that initiated divergence.

The previous ghost policy retained object collisions, including street
furniture. Removing those contacts helps avoid obstacles, but replaying only
controls with changing simulation timesteps and incomplete starting physics
state cannot guarantee a matching two-minute path. Raising the drift threshold
alone would allow a benchmark to continue in the wrong location.

## Changes

- Ghost recording/playback ignore physical/dummy props and known street lights,
  trees and fences represented as static entities. Ordinary roads/buildings and
  the native moving bridge road models remain solid. Ambient-to-ambient contacts
  remain unchanged.
- An explicit Route Assistance option defaults on with playback ghost. It
  conditions car pose and velocity from the recorded path each frame while
  native simulation still executes. It uses sector relinking without moving-list
  or audio churn and clears stale suspension contacts after large adjustments.
- Input-only mode remains available with its original drift abort. Assistance is
  disabled with playback ghost off. Run reports and profiler session context
  distinguish the modes; correction counts and pre-correction drift are retained.

Assisted timing captures should be compared with other assisted captures using
the same settings. Assistance cannot validate unmodified vehicle dynamics,
restore traffic/RNG/weather, or reproduce recorded HMD motion. Older tapes supply
estimated path velocity and an upright basis, so exact recorded roll/banking is
not restored. New recordings with prop ghosting avoid recording prop impacts
into the reference path, but existing valid V1/V2 tapes remain supported.

## Validation

- 2,099 focused host assertions pass, including a full generated 120-second route
  with changing timesteps and deliberately injected 80-metre/heading/velocity
  disturbances, completion, correction metadata and normal profiler shutdown.
- The available 120-second headset tape loads all 11,856 source samples and
  validates 12,000 assisted reference frames with changing 5–17 ms steps across
  its entire duration. The combined invocation passes 2,101 assertions.
- Android arm64 debug compilation passes.
- The APK is installed with `install -r` on the personal Quest 3 device, keeping
  game data and saves. The local replay diagnostics endpoint remains accessible.

The host runtime uses engine stubs; the full route test verifies recorder/player
logic, not Quest driving physics or visual smoothness. Headset route playback
remains the required user validation after installation.
