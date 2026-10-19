# Third-party notices

- **librw** — MIT License, copyright aap. The `librw/` directory is a fork;
  its original license is preserved at `librw/LICENSE`. The Vulkan backend
  (`librw/src/vulkan/`) was written for this project.
- **OpenXR** — the loader is fetched at build time as the official Khronos
  `org.khronos.openxr:openxr_loader_for_android` package. The repository also
  includes Khronos OpenXR 1.1.58 headers. Both are Apache License 2.0; the
  bundled header license is preserved under `overlay/vendor/openxr-1.1.58/`.
- **Qualcomm SGSR2 research code** — adapted under the BSD 3-Clause License.
  It is disabled in the release player path; the original notice is preserved
  at `librw/src/vulkan/SGSR_LICENSE.txt`.
- **VR hand meshes** (`overlay/gamefiles/models/vrhands/`) — baked from the
  UltimateXR SDK hand models, copyright VRMADA, MIT License.
- **VR Port Foundations & Contributions**:
  - **GeT_RiCh** — Original author (based on his Discord account) of the [vice-city-vr-quest](https://github.com/dubrovskiy-yevhen-stakelogic/vice-city-vr-quest) VR port for the major implementation made for GTA Vice City using the reVC engine as a contribution for VR support.
  - **whachuvr** — For the full body IK implementation fork on top of GeT_RiCh's repository ([whachuvr/vice-city-vr-quest](https://github.com/whachuvr/vice-city-vr-quest)).
  - **re3 and reVC teams** — For their amazing effort and reverse-engineering work.
  - **Gemini 4 Argon and Gemini 3.8 Flash** — For porting GeT_RiCh and whachuvr's work from reVC / Vice City to re3 / GTA 3.
  - Their collective work offered a huge contribution for a solid starting point for the GTA III VR for standalone Quest porting efforts.
  - **OpenAI Codex** — Assisted with ongoing Liberty City VR Quest development,
    including implementation, debugging, regression testing and documentation.
    Codex is planned to take over the ongoing coding-assistance role under the
    project maintainer's direction and review; see the
    [planned development areas](README.md#planned-development-with-codex).
- **This repository contains no complete re3 source tree and no assets from
  the original game or its publishers.** The patch files contain only the
  modifications and context required to transform a user-supplied re3 tree.
