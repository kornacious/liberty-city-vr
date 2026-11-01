# Liberty City VR save transfer

Open the manager (`MANAGE.bat` on Windows, or `dotnet run --project tools/manager
-c Release` on Windows/Linux) and choose **Transfer save** in the left navigation.

1. Select **Quest → PC** or **PC → Quest**.
2. Select a manual save slot from **1 through 8**.
3. Choose the **PC save folder** using Browse, or enter its path. The selected
   slot uses `GTA3sfN.b` in this folder in both directions.
4. Close both games and any other save-transfer tools, connect/authorize the
   Quest 3, then choose **Perform**. **Cancel** discards unperformed edits and
   returns focus to navigation.

The default PC folder is `<workplace>/save-transfer/pc`; choose the PC game's
actual save directory to transfer directly into or out of that installation.
The direction, slot and folder are saved when the action is performed. The
old Windows import/export BAT shortcuts now open this page with the appropriate
preset direction. Legacy script entries are hidden from **Tools and tests**.

## Supported saves

This native C# action works on Windows and Linux with .NET/ADB, including the
self-contained published manager. It needs no Python, Bash or PowerShell.
It uses the Liberty City provider (`com.libertyvr.quest.saves`), which exposes
exactly the eight manual slots and maps them to the headset's native filenames.
Autosaves and other application files are outside this action's scope.

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
or committed. A failed backup stops the transfer before writing a destination.

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
  "SaveSlot": 3,
  "PcSaveDirectory": "/home/user/Games/LibertyCityVR/userfiles"
}
```

`SaveDirection` is **0** for Quest → PC and **1** for PC → Quest. On Windows use
an absolute Windows path instead. Headless transfers require exactly one
eligible authorized Quest 3; the TUI provides numbered selection for multiple
headsets without displaying identifiers.
