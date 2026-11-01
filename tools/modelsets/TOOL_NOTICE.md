> Tool migration: standalone `.ps1`, `.sh` and `.py` entrypoints are now embedded
> manager modules. Open **Tools and tests**, or run `dotnet run --project
> tools/manager -c Release -- --tool MODULE -- ARGUMENTS` from the repository root.
> Legacy filenames below identify resource modules; old direct shell invocations
> are historical. Use the manager pages for build/install, editor build, music,
> models, repair and update. See [the manager guide](../manager/README.md) for runtime requirements.
# Modern model-set builder notice

These utilities are distributed to let players assemble an optional Modern
asset overlay locally from a legally owned GTA III installation and the pinned Xbox Version HD (re3)
pack they obtain themselves. Python 3.9+ is required on both platforms.

- `xbox-modelset.py` provides shared source validation, preparation and staging.
  Windows/Linux wrappers use this same implementation. Xbox is a legacy
  command alias; only the Modern overlay is generated.
- The IMG/TXD helper scripts and `txdcompress.cpp` are provided in source form.
  The Linux ports of the byte-level helpers are Python:
  `imgextract.py`, `imginject.py`, `imgtexaudit.py`, `txdmerge.py`.
- `txdcompress.exe` is the Windows x64 build of the adjacent
  `txdcompress.cpp` source; it imports only Windows `KERNEL32.dll`.
- The Linux build of the same source is `txdcompress` (gitignored, not
  committed). Build it once next to the source:

  ```sh
  g++ -O2 -o txdcompress txdcompress.cpp
  ```

  `build-modern-modelset.sh` refuses to run until `txdcompress` exists and is
  executable.

The utilities do not grant rights to the original game data or third-party mod
assets. Generated `modelsets/modern` content is for the player's local,
personal installation and must not be redistributed.

This repository does not currently grant a general license to reuse or
redistribute these utility sources separately from the Liberty City VR release.
Third-party components, where present, retain their own terms.
