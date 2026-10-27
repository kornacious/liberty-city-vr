# First Vulkan begin-frame performance pass

The recorded on-foot and Banshee sessions contain repeated roughly 37–41 ms
Vulkan-begin CPU intervals. The old recorder cannot distinguish compilation,
fence waits, resource retirement or other setup, so it does not prove which
operation caused those stalls. This change removes concrete avoidable pipeline
work and records the remaining causes separately.

## Changes

- Standard pipeline warm-up now varies depth testing and depth writing
  independently. The previous coupled loop omitted test-on/write-off states
  used by fading/transparent rendering.
- Opaque pipeline keys no longer vary with unused source/destination blend
  factors. Enabled blending, shader/topology, depth, culling, alpha testing,
  effects and strict radar-depth distinctions remain represented.
- A device-lifetime `VkPipelineCache` is passed to related pipeline creations,
  then destroyed during renderer shutdown. Creation failure falls back to the
  previous uncached path. There is no persistent cache file, cross-device data
  or new background compilation thread. This uses Vulkan's documented
  [pipeline-cache reuse mechanism](https://docs.vulkan.org/spec/latest/chapters/pipelines.html).
- Schema 3 captures compilation, fence wait, timestamp readback, retirement,
  wrist/scope preparation, residual setup and total backend begin CPU times,
  with validity and operation counts. App-frame reset and failed-begin paths
  prevent stale measurements. Timers are disabled outside profiling.
- Analysis lists the ten largest selected begin intervals over 5 ms, including
  their subphase values and operation counts. An outer-wrapper residual helps
  identify stalls outside librw beginFrame. Schemas 1 and 2 remain supported.

Frame-slot fence synchronization, safe resource retirement and deferred
compilation outside the render pass are preserved. Warm-up prepares more
variants at startup, so startup time and pipeline memory can increase. Driver
cache reuse and fewer first-use compilations need on-device measurement; this
change does not promise a particular FPS gain or elimination of every stall.

## Validation

- Full Android debug APK build passed.
- Production pipeline host tests passed: opaque key reuse, fading depth-state
  warm-up, strict/effects isolation, pending compilation accounting, failure
  reporting, render-state restoration and fresh begin snapshots.
- Existing descriptor/resource-lifetime suite passed (2,024,556 checks).
- Recorder schema/serialization and analysis validity/stall tests passed.
- Previous schema 1 driving and schema 2 MINIMAL recordings still analyze;
  missing detailed begin fields are unavailable rather than zero.
- Updated APK installed on the personal Quest 3 device without clearing data.
  Frontend launch succeeded, the application remained running, and a pipeline
  warm-up completion was observed with no pipeline-creation failure or cache
  fallback messages in the inspected log.

## Next on-device comparison

Repeat the same Banshee route with unchanged assets, graphics, refresh rate and
warm-up. Record in MINIMAL and select OFF to close. Compare begin-frame stall
frequency/duration and use the new counters to distinguish pipeline creation
from fence waits, retirement or setup. CPU wall time can include scheduling
preemption. A new gameplay capture is required before attributing the earlier
40 ms stalls to a specific operation or claiming a measured improvement.
