package com.libertyvr.quest;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.Binder;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.provider.OpenableColumns;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.Arrays;

/** ADB export of profiler and simulation diagnostics; never exposes game assets or mutation. */
public final class PerfCaptureProvider extends ContentProvider {
    private void enforce() {
        int caller = Binder.getCallingUid();
        if (caller != 0 && caller != 2000 && caller != Process.myUid()) {
            throw new SecurityException("Performance exports require ADB");
        }
    }

    @Override public boolean onCreate() { return true; }

    private File capture(Uri uri) throws IOException {
        if (uri.getPathSegments().size() != 2) throw new IOException("Expected diagnostics/name");
        String scope = uri.getPathSegments().get(0);
        if (!"capture".equals(scope) && !"replay".equals(scope)) {
            throw new IOException("Expected capture/name or replay/name");
        }
        return PerfCaptureFiles.resolve(getContext().getExternalFilesDir(null), uri.getLastPathSegment(), "replay".equals(scope));
    }

    @Override public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        enforce();
        if (!"r".equals(mode)) throw new FileNotFoundException("Performance exports are read-only");
        try {
            File file = capture(uri);
            if (!file.isFile()) throw new IOException("Capture not found");
            return ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY);
        } catch (IOException error) {
            throw new FileNotFoundException(error.getMessage());
        }
    }

    @Override public Cursor query(Uri uri, String[] projection, String selection,
            String[] selectionArgs, String sortOrder) {
        enforce();
        if (uri.getPathSegments().size() != 1 ||
                (!"captures".equals(uri.getLastPathSegment()) && !"replays".equals(uri.getLastPathSegment()))) {
            throw new IllegalArgumentException("Expected captures or replays listing");
        }
        boolean replay = "replays".equals(uri.getLastPathSegment());
        MatrixCursor cursor = new MatrixCursor(new String[] {OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE});
        try {
            File root = PerfCaptureFiles.directory(getContext().getExternalFilesDir(null), replay);
            String[] names = root.list();
            if (names == null) return cursor;
            Arrays.sort(names);
            for (String name : names) {
                if (!PerfCaptureFiles.validName(name, replay)) continue;
                File file = PerfCaptureFiles.resolve(getContext().getExternalFilesDir(null), name, replay);
                if (file.isFile()) cursor.addRow(new Object[] {name, file.length()});
            }
            return cursor;
        } catch (IOException error) {
            throw new IllegalArgumentException("Cannot list performance captures");
        }
    }

    @Override public String getType(Uri uri) { enforce(); return "text/csv"; }
    @Override public Uri insert(Uri uri, ContentValues values) { throw new UnsupportedOperationException("Read-only"); }
    @Override public int update(Uri uri, ContentValues values, String s, String[] a) { throw new UnsupportedOperationException("Read-only"); }
    @Override public int delete(Uri uri, String s, String[] a) { throw new UnsupportedOperationException("Read-only"); }
}
