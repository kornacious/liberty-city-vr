package com.libertyvr.quest;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.media.MediaMetadataRetriever;
import android.net.Uri;
import android.os.Binder;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import org.json.JSONObject;
import org.json.JSONArray;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.util.Properties;
import java.io.FileNotFoundException;
import java.security.MessageDigest;

/** Shell-only, bounded music transfer bridge. No arbitrary path access. */
public final class MusicProvider extends ContentProvider {
    private static final long MAX_FILE = 512L * 1024 * 1024;
    private static final long RESERVE = 1024L * 1024 * 1024;
    private static final String NAME = "[A-Za-z0-9][A-Za-z0-9_.-]{0,119}\\.(?i:mp3|m4a|ogg|opus|wav|flac)";
    @Override public boolean onCreate() { return true; }
    private void enforce() {
        int uid = Binder.getCallingUid();
        if (uid != 0 && uid != 2000 && uid != Process.myUid())
            throw new SecurityException("Music transfers require ADB");
    }
    private File root() throws Exception {
        File external = getContext().getExternalFilesDir(null);
        if (external == null) throw new Exception("External storage unavailable");
        File root = new File(external, "gamedata/mp3");
        if (!root.isDirectory() && !root.mkdirs()) throw new Exception("Cannot create music directory");
        return root;
    }
    private String name(String name) throws Exception {
        if (name == null || !name.matches(NAME)) throw new Exception("Invalid music filename");
        return name;
    }
    private File staged(String name) throws Exception { return new File(root(), ".incoming-" + name(name)); }
    private boolean music(File file) {
        return MusicPlaylistStore.music(file);
    }
    private long duration(File file, Properties cache) {
        String key = file.getName(), prefix = file.length() + "," + file.lastModified() + ",";
        String old = cache.getProperty(key, "");
        if (old.startsWith(prefix)) {
            try { return Long.parseLong(old.substring(prefix.length())); } catch (NumberFormatException ignored) {}
        }
        long ms = 0;
        try (MediaMetadataRetriever metadata = new MediaMetadataRetriever()) {
            metadata.setDataSource(file.getAbsolutePath());
            ms = Long.parseLong(metadata.extractMetadata(MediaMetadataRetriever.METADATA_KEY_DURATION));
        } catch (Exception ignored) {}
        if (ms < 0 || ms > 2073600000L) ms = 0;
        cache.setProperty(key, prefix + ms); return ms;
    }
    private long totalDuration(File root) throws Exception {
        File index = new File(root, ".import-index-v1");
        Properties cache = new Properties();
        if (index.isFile()) try (FileInputStream in = new FileInputStream(index)) { cache.load(in); }
        long total = 0; File[] files = root.listFiles();
        if (files != null) for (File file : files) if (music(file)) total += duration(file, cache);
        File temp = new File(root, ".import-index-v1.tmp");
        try (FileOutputStream out = new FileOutputStream(temp)) { cache.store(out, "Music metadata only"); }
        if (!temp.renameTo(index)) throw new Exception("Cannot save import metadata");
        return total;
    }
    private int count(File root) {
        File[] files = root.listFiles(); int count = 0;
        if (files != null) for (File f : files) if (music(f)) ++count;
        return count;
    }
    @Override public synchronized ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        enforce();
        try {
            if (uri.getPathSegments().size() == 1 && "board-export".equals(uri.getLastPathSegment()) && "r".equals(mode)) {
                File file = new File(root().getParentFile(), "radio_board_export.ini");
                return ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY);
            }
            if (uri.getPathSegments().size() == 1 && "playlist".equals(uri.getLastPathSegment()) && ("w".equals(mode) || "wt".equals(mode)))
                return ParcelFileDescriptor.open(new File(root(), ".incoming-playlist"), ParcelFileDescriptor.MODE_CREATE | ParcelFileDescriptor.MODE_WRITE_ONLY | ParcelFileDescriptor.MODE_TRUNCATE);
            if (uri.getPathSegments().size() != 2 || !"upload".equals(uri.getPathSegments().get(0)) || !("w".equals(mode) || "wt".equals(mode)))
                throw new Exception("Expected upload/<safe music filename> in write mode");
            if (root().getUsableSpace() < RESERVE) throw new Exception("Keep at least 1 GiB free");
            File file = staged(uri.getLastPathSegment());
            return ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_CREATE | ParcelFileDescriptor.MODE_WRITE_ONLY | ParcelFileDescriptor.MODE_TRUNCATE);
        } catch (Exception e) { throw new FileNotFoundException(e.getMessage()); }
    }
    private String sha256(File file) throws Exception {
        MessageDigest hash = MessageDigest.getInstance("SHA-256");
        try (FileInputStream in = new FileInputStream(file)) {
            byte[] buffer = new byte[65536]; int n;
            while ((n = in.read(buffer)) != -1) hash.update(buffer, 0, n);
        }
        StringBuilder hex = new StringBuilder(); for (byte b : hash.digest()) hex.append(String.format("%02x", b & 255));
        return hex.toString();
    }
    @Override public synchronized Bundle call(String method, String arg, Bundle extras) {
        enforce(); JSONObject result = new JSONObject();
        try {
            File root = root();
            if ("info".equals(method)) {
                result.put("version", 3);
                result.put("count", count(root)); result.put("free", root.getUsableSpace());
                result.put("duration", totalDuration(root));
                JSONArray names = new JSONArray(); File[] files = root.listFiles();
                if (files != null) for (File file : files) if (music(file)) names.put(file.getName());
                result.put("names", names);
            } else if ("library".equals(method)) {
                String[] request = (arg == null ? "0" : arg).split(":", 2);
                int offset = Integer.parseInt(request[0]);
                java.util.List<File> files = MusicPlaylistStore.ordered(root);
                String revision = MusicPlaylistStore.revision(files);
                if (offset < 0 || offset > files.size() || (request.length == 2 && !revision.equals(request[1]))) throw new Exception("Library changed; refresh again");
                MusicCatalog catalog = new MusicCatalog(root); JSONArray tracks = new JSONArray();
                for (int i = offset; i < Math.min(files.size(), offset + 50); ++i) tracks.put(catalog.describe(files.get(i)));
                catalog.persist();
                if (!revision.equals(MusicPlaylistStore.revision(MusicPlaylistStore.ordered(root)))) throw new Exception("Library changed; refresh again");
                result.put("revision", revision); result.put("total", files.size()); result.put("tracks", tracks);
            } else if ("set-order".equals(method) || "apply-library".equals(method)) {
                File staged = new File(root, ".incoming-playlist");
                try {
                    if (!staged.isFile() || staged.length() > 262144) throw new Exception("Missing or oversized playlist upload");
                    String payload = new String(java.nio.file.Files.readAllBytes(staged.toPath()), java.nio.charset.StandardCharsets.UTF_8);
                    JSONObject changes = "apply-library".equals(method) ? new JSONObject(payload) : null;
                    JSONArray json = changes == null ? new JSONArray(payload) : changes.getJSONArray("order");
                    java.util.List<String> names = new java.util.ArrayList<>();
                    for (int i = 0; i < json.length(); ++i) names.add(json.getString(i));
                    if (changes == null) MusicPlaylistStore.save(root, names, arg);
                    else {
                        JSONArray removed = changes.getJSONArray("delete"); java.util.List<String> deleted = new java.util.ArrayList<>();
                        for (int i = 0; i < removed.length(); ++i) deleted.add(removed.getString(i));
                        MusicPlaylistStore.apply(root, names, deleted, arg);
                    }
                    result.put("revision", MusicPlaylistStore.revision(MusicPlaylistStore.ordered(root)));
                } finally { if (staged.exists()) staged.delete(); }
            } else if ("original-name".equals(method)) {
                JSONObject hint = new JSONObject(arg); String safe = name(hint.getString("name"));
                if (!new File(root, safe).isFile()) throw new Exception("Music file is not installed");
                MusicCatalog catalog = new MusicCatalog(root); catalog.original(safe, hint.getString("filename")); catalog.persist(); MusicPlaylistStore.rescan(root);
            } else if ("check".equals(method)) {
                if (arg == null || !arg.contains(":")) throw new Exception("Expected filename:sha256");
                String[] parts = arg.split(":", 2); File destination = new File(root, name(parts[0]));
                boolean exists = destination.isFile();
                if (exists && !sha256(destination).equals(parts[1])) throw new Exception("Destination exists with different contents");
                result.put("exists", exists);
            } else if ("abort".equals(method)) {
                File temp = staged(arg); if (temp.exists() && !temp.delete()) throw new Exception("Could not remove incomplete upload");
            } else if ("commit".equals(method)) {
                if (arg == null || !arg.contains(":")) throw new Exception("Expected filename:sha256");
                String[] parts = arg.split(":", 2); String name = name(parts[0]);
                if (!parts[1].matches("[a-f0-9]{64}")) throw new Exception("Invalid checksum");
                File temp = staged(name), destination = new File(root, name);
                if (!temp.isFile() || temp.length() == 0 || temp.length() > MAX_FILE) throw new Exception("File must be between 1 byte and 512 MiB");
                if (!sha256(temp).equals(parts[1])) throw new Exception("Transfer checksum mismatch");
                if (destination.exists()) {
                    if (sha256(destination).equals(parts[1])) { if (!temp.delete()) throw new Exception("Cannot clean duplicate upload"); result.put("duplicate", true); }
                    else throw new Exception("Destination exists; refusing to overwrite");
                } else {
                    if (count(root) >= 1000) throw new Exception("Library limit is 1000 files");
                    if (root.getUsableSpace() < RESERVE) throw new Exception("Keep at least 1 GiB free");
                    long duration;
                    try (MediaMetadataRetriever metadata = new MediaMetadataRetriever()) {
                        metadata.setDataSource(temp.getAbsolutePath());
                        duration = Long.parseLong(metadata.extractMetadata(MediaMetadataRetriever.METADATA_KEY_DURATION));
                    }
                    if (duration <= 0 || duration > 2073600000L) throw new Exception("Invalid or excessive audio duration");
                    if (totalDuration(root) + duration > 2073600000L) throw new Exception("Library exceeds 24 days total duration");
                    if (!temp.renameTo(destination)) throw new Exception("Cannot finalize upload");
                    result.put("duration", duration);
                }
                result.put("bytes", destination.length());
                // Main-thread audio service notices this marker; completed
                // transfers are indexed without restarting the game.
                File marker = new File(root, ".rescan");
                try (FileOutputStream out = new FileOutputStream(marker)) { out.write(Long.toString(System.nanoTime()).getBytes(java.nio.charset.StandardCharsets.US_ASCII)); }
            } else throw new Exception("Unknown transfer method");
            result.put("ok", true);
        } catch (Exception e) { try { result.put("ok", false); result.put("error", e.getMessage() == null ? e.toString() : e.getMessage()); } catch (Exception ignored) {} }
        Bundle bundle = new Bundle(); bundle.putString("result", result.toString()); return bundle;
    }
    @Override public String getType(Uri uri) { return "application/octet-stream"; }
    @Override public Cursor query(Uri uri, String[] p, String s, String[] a, String order) { throw new UnsupportedOperationException(); }
    @Override public Uri insert(Uri uri, ContentValues values) { throw new UnsupportedOperationException(); }
    @Override public int update(Uri uri, ContentValues values, String s, String[] a) { throw new UnsupportedOperationException(); }
    @Override public int delete(Uri uri, String s, String[] a) { throw new UnsupportedOperationException(); }
}
