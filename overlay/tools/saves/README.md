# Liberty City VR save transfer

Open the manager (`MANAGE.bat` on Windows or `./MANAGE.sh` on Linux) and choose
**Transfer save** in the left navigation.

1. Select **Quest → PC** or **PC → Quest**.
2. Check one or more manual save slots **1 through 8**, and/or **Autosave**.
   Click or Space toggles a checkbox; Ctrl+A checks all slots.
3. Choose the **PC save folder** using Browse, or enter its path. The selected
   manual slots use `GTA3sfN.b` in this folder in both directions. Autosave uses
   `GTA3sfautosave.b` (the headset's native filename is `autosave.b`).
4. Close both games and any other save-transfer tools, connect/authorize the
   Quest 3, then choose **Perform**. **Cancel** discards unperformed edits and
   returns focus to navigation.

The default PC folder is `<workplace>/save-transfer/pc`; choose the PC game's
actual save directory to transfer directly into or out of that installation.
The direction, selected slots and folder are saved when the action is performed. The
old Windows import/export BAT shortcuts now open this page with the appropriate
preset direction. The incompatible legacy Vice City converter/scripts have been removed.

## Supported saves

This native C# action works on Windows and Linux with .NET/ADB, including the
self-contained published manager. It needs no Python, Bash or PowerShell.
It uses the Liberty City provider (`com.libertyvr.quest.saves`), which exposes
the eight manual slots and dedicated Autosave, mapped to the headset's native
filenames. Install the updated APK before transferring Autosave. Autosave staging
and backup companion files and other application files are outside this action's scope.

The current GTA III/re3 **COMPATIBLE_SAVES** build uses the same serialized
layout on Win64, Linux and Quest. Transfers preserve the save bytes; they do not
apply the old Vice City garages converter. The manager checks the GTA III header,
20 subsystem blocks, padding limits, the 5484-byte garages block and the additive
container checksum. Corrupt, truncated, Vice City and unsupported custom layouts
are rejected before replacing the destination. This validates the container, not
mission progress or compatibility with arbitrary modified game builds.

## Backups and verification

Existing destinations are backed up locally under:

```text
<workplace>/save-transfer/backups/<timestamp-and-unique-token>/
```

PC backups use `GTA3sfN.b.pc.bak`; Quest backups use
`GTA3sfN.b.quest-native.bak`. They retain the exact original bytes, including a
corrupt destination that is deliberately being replaced. They are not uploaded
or committed. Autosave backups use the same suffixes on `GTA3sfautosave.b`.
Every selected source is validated and every existing destination backed up before
the first replacement. An empty/missing or invalid selected source or failed backup
stops the batch before any destination is changed. The game is stopped once per
batch. Slots transfer sequentially; a later failure preserves already completed
transfers, stops remaining slots and reports the completed count. The failed Quest
write still receives the per-slot recovery described below.

The PC export is staged beside its destination and renamed into place after
validation. Quest transfers use device-local temporary files and ADB's binary
sync protocol, avoiding Windows shell-stdin binary corruption. Downloads are
size/SHA256 checked; imports are read back and compared byte for byte. The game
is force-stopped before accessing the slot and remains stopped afterward.

The installed provider truncates on write, so a failed/cancelled import attempts
to restore the previous slot using a separate recovery timeout. If the device
has disconnected, automatic recovery may fail: keep the game stopped, retain
its local raw backup. For a valid compatible backup, copy it into a separate
PC folder as `GTA3sfN.b` and use **PC → Quest** after reconnecting.
Do not use the slot until recovery or a verified retry succeeds. Other slots,
settings and game assets are not transferred. ADB output/identifiers stay private.

## Command interface

Open a page without starting a transfer:

```text
dotnet run --project tools/manager -c Release -- --page TransferSave
dotnet run --project tools/manager -c Release -- --page QuestToPc
dotnet run --project tools/manager -c Release -- --page PcToQuest
```

For automation use `--run TransferSave REQUEST.json`. The request contains the
usual shared `Settings` object and these `Options`:

```json
{
  "SaveDirection": 0,
  "SaveSlots": [1, 3, 9],
  "PcSaveDirectory": "/home/user/Games/LibertyCityVR/userfiles"
}
```

Slot **9** means **Autosave**; 1–8 are manual saves. Omit `SaveSlots` to keep using
the legacy single `SaveSlot` option. An explicit empty selection is rejected.
`SaveDirection` is **0** for Quest → PC and **1** for PC → Quest. On Windows use
an absolute Windows path instead. Headless transfers require exactly one
eligible authorized Quest 3; the TUI provides numbered selection for multiple
headsets without displaying identifiers.
