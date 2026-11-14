# Desktop renderer debug viewer

`libertyvr-render-debug` opens prepared local game content directly into an
observation camera. It uses the game's world loader, streaming, visibility,
draw submission, Vulkan backend, shaders, rendering preferences and profiler.
There is no player entity, mission script execution, save loading or gameplay
frontend. Neither an OpenXR runtime nor an OpenXR loader library is required
to run the viewer. Synthetic stereo is rendered on the desktop GPU.

## Build and launch

Build the shared desktop project with an assembled re3 source tree as described
in [README.md](README.md). A normal desktop build/install includes the viewer
and its `libertyvr-render-debug-upstate` sibling. The manager's **Renderer debug
viewer** action launches it without the PCVR runtime/headset diagnostics.
Use the same prepared content directory as the game. Assets remain local.

```text
libertyvr-render-debug --game-data /local/prepared-data --user-data /local/viewer-user
libertyvr-render-debug --game-data /local/prepared-data --user-data /local/viewer-user --scenario /local/scene.json --output /local/results
```

Append `.exe` on Windows. `--help` works without assets. `pcvr-paths.ini` supplies
default data/user paths when present. The viewer always appends `render-debug`
to an explicitly supplied/default configured user root, so pointing at a normal
PCVR user directory cannot overwrite game preferences or saves. Without a user
root, it uses `render-debug-user/render-debug` beside the prepared data.

Settings, ImGui layout, bookmarks, named paths, profiler exports, screenshots
and runs are stored under the viewer root. **Save viewer settings** persists
requested restart settings. Render dimensions, view count, MSAA, mip generation,
reflection buffer scale, imported profile and content selection require restart.
The panel shows the currently active render dimensions and content.

Classic and Original Xbox scenarios select their prepared variant for this
process without rewriting the installation's selection. An Upstate scenario
requires the existing verified/enabled local Upstate package and dispatches to
the viewer sibling. Original game assets are never bundled.

## Interactive tools

- **F1** shows/hides the collapsible settings panel. **F2** independently
  shows/hides the live statistics window, which remains available while editing.
- Hold **right mouse** in the viewport to capture the flycam. Releasing it
  restores the cursor for UI interaction. Starting a right click on an ImGui
  window leaves that gesture with the UI. **WASD** moves, **Q/E** descends/ascends;
  **Shift** speeds up and **Ctrl** slows down. Optional gamepad sticks provide
  translation/look; triggers provide vertical movement. Input requires focus
  and keyboard/mouse camera input is active only during the held flycam gesture.
  **Quit / Exit** beside **Save viewer settings** also works in fullscreen.
- Camera controls include numeric position/quaternion, horizontal FOV, clipping,
  speed and sensitivity. Bookmarks preserve pose and FOV. Named paths support
  recording, timed keyframes, editing, playback, scrubbing, looping and speed.
  Positions/FOV interpolate linearly; rotations use shortest-path quaternion
  interpolation. Scrubbing does not rewind the world.
- Display controls include bordered, borderless and desktop fullscreen windows,
  window dimensions, independent render dimensions, VSync and an FPS cap.
  Mono is the default; stereo can present either eye or both side by side.
- Environment controls include the engine's four supported weather types,
  blending, time of day, clock progression, simulation pause, and optional
  ambient vehicles/pedestrians. The default is sunny noon without population.
  Ambient civilian activity uses shared population/traffic code without player
  combat, missions or wanted escalation.
- Graphics controls edit the same live preferences as the VR menu. Visualization
  includes solid/wireframe/hidden-line geometry, shading/depth/overdraw modes,
  visibility layers, collision/bounds and velocity overlays.
  World LOD **On** uses the production camera's minimum `2.2x` detail-distance
  multiplier, including in mono; widening the FOV cannot bypass that floor.
  Performance/Aggressive intentionally select earlier LODs. Statistics and run
  summaries expose the camera multiplier alongside the selected mode.
- Statistics show CPU work, supported GPU render-pass timings, frame-time history,
  memory and the existing detailed renderer counters/occlusion maps. GPU logical
  draw categories are not presented as independently timed hardware passes.
  Unsupported measurements remain unavailable. RenderDoc capture requires the
  existing optional injected RenderDoc integration.

## Scenarios and reports

Start with [render-debug-example.json](render-debug-example.json). The same
versioned JSON is loadable/editable/saveable in ImGui or runnable through CLI.
Unknown fields, invalid quaternion/keyframe data and invalid numeric ranges are
rejected. Paths and screenshots refer only to camera data and generated images.

Top-level fields:

| Field | Meaning/default |
| --- | --- |
| `version` | Required, `1` |
| `content` | `selected`, `classic`, `xbox`, or `upstate` |
| `settings` | Viewer display, environment, renderer and visualization settings |
| `camera` | Position `[x,y,z]`, normalized quaternion `[x,y,z,w]`, horizontal `fov` |
| `path` | Optional array of `{time,camera}`; starts at zero, strictly increasing |
| `quest_profile` | Optional captured `.quest-profile`, relative to the scenario |
| `timestep` | Simulation seconds per rendered frame; default `1/60` |
| `duration` | Measured seconds; default path length, or 10 seconds without a path |
| `warmup_frames` | Unmeasured frames per pass; default 120 |
| `repetitions` | Passes; default 3 |
| `screenshots` | Optional zero-based measured-frame indexes, captured each pass |

Camera-local axes are right `+X`, forward `+Y`, up `+Z`, in game world units.
Batch mode disables VSync and the FPS cap. Camera playback advances by fixed
simulation steps, independent of achieved presentation rate. Each pass resets
the world, clock, camera and ambient systems, prepares initial streaming, then
warms up before measurement. Settings are locked during a run. F1 exposes the
cancel button. Closing the window interrupts the batch.

Each run creates a fresh `run-N` directory; previous runs are never replaced.
Outputs include the scenario, source/content/profile identities, effective
settings, per-frame CSV, summary JSON and requested BMP screenshots. CPU work,
presentation cost, frame interval and sampled GPU execution are separate.
Delayed GPU samples are attached to their source frame; warmup/drain frames and
missing samples are excluded. Summary statistics include mean/p50/p95/p99.
Screenshot readback is synchronous and affects its frame's presentation cost.

Exit codes are `0` for completion, `1` for runtime/output failure or interruption,
and `2` for invalid/unsupported startup input. Incomplete runs retain a summary
with `completed: false`; process crashes may leave its last running status.
The default output root is `runs` under the viewer user directory.

Captured Quest profiles reuse requested eye dimensions, eye poses/FOV and
supported renderer settings. Explicit scenario graphics overrides are recorded
as modifications. Profiles cannot enable player gameplay. Hardware capability
failures are reported rather than silently reducing the requested render extent.
The active view always measures the desktop GPU: it is not Quest emulation or a
headset-performance prediction. These viewer reports are separate from the
gameplay matched-replay comparer described in [BENCHMARKS.md](../../BENCHMARKS.md).

Runs warm caches. Camera/environment settings are repeatable; ambient AI,
particles, streaming, driver scheduling and thermal conditions can vary. Use
several passes, inspect effective settings, and validate an optimization on its
actual target. Headless rendering and a live external control API are deferred.

## Validation

`render-debug-core` CTest validates JSON/scenario round trips, malformed input,
camera interpolation, fixed-step playback, profile geometry and output isolation.
It also checks hold-to-fly capture ownership, release, focus loss and UI gestures.
Viewer `--help` checks do not require game data. Runtime acceptance requires
locally owned prepared content and a Vulkan display. Native Linux runtime checks
must be performed on Linux; Windows/WSL results do not establish Linux support.
