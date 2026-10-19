#pragma once

#include <cstdio>

// Save-system operations are injected so failures can be tested without game
// assets or a headset. Only the dedicated autosave and its staging/backup files
// belong to this transaction; numbered manual saves never enter it.
namespace AutosaveTransaction {
template<class Operations>
bool Save(Operations &ops, const char *destination)
{
    char temporary[260], backup[260], backupTemporary[260];
    const int a = std::snprintf(temporary, sizeof(temporary), "%s.tmp", destination);
    const int b = std::snprintf(backup, sizeof(backup), "%s.bak", destination);
    const int c = std::snprintf(backupTemporary, sizeof(backupTemporary), "%s.bak.tmp", destination);
    if(a < 0 || b < 0 || c < 0 || a >= int(sizeof(temporary)) ||
       b >= int(sizeof(backup)) || c >= int(sizeof(backupTemporary)))
        return false;
    struct Cleanup {
        Operations &ops;
        const char *temporary, *backupTemporary;
        ~Cleanup() { ops.Remove(temporary); ops.Remove(backupTemporary); }
    } cleanup = {ops, temporary, backupTemporary};

    if(!ops.Write(temporary) || !ops.Sync(temporary) || !ops.Validate(temporary))
        return false;
    // Copy, never move, the last good save. A crash during backup preparation
    // must leave the current destination available. A damaged destination must
    // not replace an older valid recovery copy.
    if(ops.Exists(destination) && ops.Validate(destination)) {
        if(!ops.Copy(destination, backupTemporary) || !ops.Sync(backupTemporary) ||
           !ops.Validate(backupTemporary) || !ops.Replace(backupTemporary, backup) ||
           !ops.SyncDirectory(destination))
            return false;
    }
    // Both files are in the same directory. Replacement happens only after
    // serialization, close, flush-to-storage and checksum validation succeeded.
    if(!ops.Replace(temporary, destination))
        return false;
    return ops.SyncDirectory(destination);
}
}
