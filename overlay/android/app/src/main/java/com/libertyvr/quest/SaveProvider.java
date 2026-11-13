package com.libertyvr.quest;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.Binder;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.provider.OpenableColumns;

import java.io.File;
import java.io.FileNotFoundException;

/**
 * A deliberately tiny ADB bridge for Liberty City save slots.
 *
 * Android's scoped external storage lets the release app create its saves but
 * prevents the ADB shell from reading those app-owned files.  Making the APK
 * debuggable merely to transfer saves would weaken the release.  This provider
 * instead exposes eight manual files and the dedicated autosave, and accepts only calls from the app
 * itself, root, or Android's shell UID used by `adb shell content`.
 */
public final class SaveProvider extends ContentProvider {
    private static final int ROOT_UID = 0;
    private static final int SHELL_UID = 2000;
    private static final String[] GAME_DATA_DIRECTORIES = {
            "data", "TEXT", "anim", "txd", "skins", "mp3", "movies",
            "models", "Audio", "audio",
            "data/maps",
            "data/maps/comnbtm",
            "data/maps/comntop",
            "data/maps/comroad",
            "data/maps/comse",
            "data/maps/comsw",
            "data/maps/indroads",
            "data/maps/industne",
            "data/maps/industnw",
            "data/maps/industse",
            "data/maps/industsw",
            "data/maps/landne",
            "data/maps/landsw",
            "data/maps/making",
            "data/maps/subroads",
            "data/maps/temppart",
            "data/maps/generic",
            "data/paths",
            "models/coll", "models/Coll",
            "models/generic", "models/Generic",
            "models/vrhands",
            "modelsets", "modelsets/modern", "modelsets/modern/models",
            "modelsets/modern/models/coll", "modelsets/modern/models/generic",
            "modelsets/modern/txd",
            "modelsets/xbox", "modelsets/xbox/models",
            "modelsets/xbox/models/coll", "modelsets/xbox/models/generic",
            "modelsets/xbox/txd"
    };

    @Override
    public boolean onCreate() {
        return true;
    }

    private void enforceAdbOrAppCaller() {
        final int caller = Binder.getCallingUid();
        if (caller != ROOT_UID && caller != SHELL_UID && caller != Process.myUid()) {
            throw new SecurityException("Liberty City saves are available only through ADB");
        }
    }

    private int parseSlot(Uri uri) throws FileNotFoundException {
        if (uri == null || uri.getPathSegments().size() != 2 ||
                !"slot".equals(uri.getPathSegments().get(0))) {
            throw new FileNotFoundException("Expected content://.../slot/1 through /slot/9 (Autosave)");
        }
        final int slot;
        try {
            slot = Integer.parseInt(uri.getPathSegments().get(1));
        } catch (NumberFormatException error) {
            throw new FileNotFoundException("Invalid save slot");
        }
        if (slot < 1 || slot > 9) {
            throw new FileNotFoundException("Save slot must be between 1 and 9 (Autosave)");
        }
        return slot;
    }

    private File saveFile(int slot) throws FileNotFoundException {
        if (getContext() == null) {
            throw new FileNotFoundException("Application context is unavailable");
        }
        final File externalRoot = getContext().getExternalFilesDir(null);
        if (externalRoot == null) {
            throw new FileNotFoundException("External files directory is unavailable");
        }
        // The Quest frontend stores 1.b..8.b and autosave.b in gamedata. Keep this
        // bridge aligned with that on-disk contract; the transfer tools hide
        // the platform-specific name from users.
        final File gameData = new File(externalRoot, "gamedata");
        if (!gameData.isDirectory() && !gameData.mkdirs()) {
            throw new FileNotFoundException("Cannot create the game-data directory");
        }
        // ADB's shell UID may write files into an existing app-specific
        // directory, but current Horizon OS refuses to create its children.
        // Bootstrap the documented retail-data layout as the app UID so a
        // completely clean install can be populated with ordinary adb push.
        for (String directory : GAME_DATA_DIRECTORIES) {
            final File child = new File(gameData, directory);
            if (!child.isDirectory() && !child.mkdirs()) {
                throw new FileNotFoundException(
                        "Cannot create game-data child " + directory);
            }
        }
        return new File(gameData, slot == 9 ? "autosave.b" : slot + ".b");
    }

    @Override
    public ParcelFileDescriptor openFile(Uri uri, String mode)
            throws FileNotFoundException {
        enforceAdbOrAppCaller();
        final File file = saveFile(parseSlot(uri));
        final int flags;
        if ("r".equals(mode)) {
            flags = ParcelFileDescriptor.MODE_READ_ONLY;
        } else if ("w".equals(mode) || "wt".equals(mode)) {
            flags = ParcelFileDescriptor.MODE_WRITE_ONLY |
                    ParcelFileDescriptor.MODE_CREATE |
                    ParcelFileDescriptor.MODE_TRUNCATE;
        } else {
            throw new FileNotFoundException("Supported modes are r and w");
        }
        return ParcelFileDescriptor.open(file, flags);
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection,
            String[] selectionArgs, String sortOrder) {
        enforceAdbOrAppCaller();
        try {
            final int slot = parseSlot(uri);
            final File file = saveFile(slot);
            final String[] columns = projection == null ?
                    new String[] { OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE } :
                    projection;
            final MatrixCursor cursor = new MatrixCursor(columns, 1);
            final MatrixCursor.RowBuilder row = cursor.newRow();
            for (String column : columns) {
                if (OpenableColumns.DISPLAY_NAME.equals(column)) {
                    row.add(slot == 9 ? "GTA3sfautosave.b" : "GTA3sf" + slot + ".b");
                } else if (OpenableColumns.SIZE.equals(column)) {
                    row.add(file.isFile() ? file.length() : 0L);
                } else {
                    row.add(null);
                }
            }
            return cursor;
        } catch (FileNotFoundException error) {
            throw new IllegalArgumentException(error.getMessage(), error);
        }
    }

    @Override
    public String getType(Uri uri) {
        enforceAdbOrAppCaller();
        return "application/octet-stream";
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        throw new UnsupportedOperationException("Use openFile in write mode");
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException("Save deletion is not exposed");
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        throw new UnsupportedOperationException("Use openFile in write mode");
    }

    @Override
    public Bundle call(String method, String arg, Bundle extras) {
        enforceAdbOrAppCaller();
        if("activate-variant".equals(method)) {
            Bundle result = new Bundle();
            try {
                if(getContext()==null || getContext().getExternalFilesDir(null)==null) throw new java.io.IOException("Application storage unavailable");
                VariantFiles.activate(new File(getContext().getExternalFilesDir(null),"gamedata"),arg);
                result.putString("status","activated");
            } catch(java.io.IOException error) { result.putString("error",error.getMessage()); }
            return result;
        }
        if ("mkdir".equals(method) && arg != null && !arg.isEmpty()) {
            if (getContext() != null) {
                final File externalRoot = getContext().getExternalFilesDir(null);
                if (externalRoot != null) {
                    final File gameData = new File(externalRoot, "gamedata");
                    final File target = new File(gameData, arg.replace('\\', '/'));
                    target.mkdirs();
                }
            }
        }
        return null;
    }
}
