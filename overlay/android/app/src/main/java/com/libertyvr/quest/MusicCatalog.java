package com.libertyvr.quest;

import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.media.MediaMetadataRetriever;
import org.json.JSONObject;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;

final class MusicCatalog {
    private final File root;
    private final Properties cache = new Properties(), originals = new Properties();
    MusicCatalog(File root) throws IOException {
        this.root = root;
        load(new File(root, ".catalog-v2"), cache); load(new File(root, ".original-names-v1"), originals);
    }
    private static void load(File path, Properties properties) throws IOException {
        if (path.isFile() && path.length() <= 4 * 1024 * 1024) try (FileInputStream in = new FileInputStream(path)) { properties.load(in); }
    }
    private static void save(File path, Properties properties) throws IOException {
        ByteArrayOutputStream data = new ByteArrayOutputStream(); properties.store(data, "Music metadata only"); MusicPlaylistStore.atomic(path, data.toByteArray());
    }
    void original(String name, String original) throws Exception {
        if (original == null || original.length() > 240 || original.contains("/") || original.contains("\\") || MusicPlaylistStore.clean(original).isEmpty())
            throw new IOException("Invalid original filename");
        originals.setProperty(name, MusicPlaylistStore.clean(original)); cache.remove(name);
        save(new File(root, ".original-names-v1"), originals);
    }
    JSONObject describe(File file) throws Exception {
        String original = originals.getProperty(file.getName(), file.getName());
        try {
            JSONObject old = new JSONObject(cache.getProperty(file.getName(), "{}"));
            if (old.optLong("bytes", -1) == file.length() && old.optLong("modified", -1) == file.lastModified() && original.equals(old.optString("filename"))) return old;
        } catch (Exception ignored) {}
        JSONObject item = new JSONObject();
        item.put("name", file.getName()); item.put("filename", original); item.put("bytes", file.length()); item.put("modified", file.lastModified());
        item.put("artist", ""); item.put("album", ""); item.put("title", ""); item.put("track", ""); item.put("genre", ""); item.put("year", ""); item.put("duration", 0);
        item.put("format", file.getName().substring(file.getName().lastIndexOf('.') + 1).toUpperCase(Locale.ROOT));
        item.put("mime", ""); item.put("sampleRate", 0); item.put("channels", 0); item.put("bitrate", 0);
        try (MediaMetadataRetriever m = new MediaMetadataRetriever()) {
            m.setDataSource(file.getAbsolutePath());
            String[] fields = {"artist", "album", "title", "track", "genre", "year"};
            int[] keys = {2, 1, 7, 0, 6, 8};
            for (int i = 0; i < fields.length; ++i) item.put(fields[i], MusicPlaylistStore.clean(m.extractMetadata(keys[i])));
            String ms = m.extractMetadata(9); if (ms != null) item.put("duration", Long.parseLong(ms));
            String bitrate = m.extractMetadata(20); if (bitrate != null) item.put("bitrate", Integer.parseInt(bitrate));
        } catch (Exception ignored) {}
        MediaExtractor extractor = new MediaExtractor();
        try {
            extractor.setDataSource(file.getAbsolutePath());
            for (int i = 0; i < extractor.getTrackCount(); ++i) {
                MediaFormat f = extractor.getTrackFormat(i); String mime = f.getString(MediaFormat.KEY_MIME);
                if (mime != null && mime.startsWith("audio/")) {
                    item.put("mime", mime);
                    if (f.containsKey(MediaFormat.KEY_SAMPLE_RATE)) item.put("sampleRate", f.getInteger(MediaFormat.KEY_SAMPLE_RATE));
                    if (f.containsKey(MediaFormat.KEY_CHANNEL_COUNT)) item.put("channels", f.getInteger(MediaFormat.KEY_CHANNEL_COUNT));
                    if (f.containsKey(MediaFormat.KEY_BIT_RATE)) item.put("bitrate", f.getInteger(MediaFormat.KEY_BIT_RATE));
                    break;
                }
            }
        } catch (Exception ignored) {} finally { extractor.release(); }
        item.put("display", MusicPlaylistStore.display(original, item.getString("artist"), item.getString("album"), item.getString("title")));
        cache.setProperty(file.getName(), item.toString()); return item;
    }
    void persist() throws Exception {
        List<File> files = MusicPlaylistStore.ordered(root); StringBuilder titles = new StringBuilder();
        Set<String> current = new HashSet<>(); for (File f : files) current.add(f.getName());
        cache.keySet().retainAll(current); originals.keySet().retainAll(current);
        for (File f : files) {
            JSONObject m = describe(f);
            titles.append(MusicPlaylistStore.quoted(f.getName())).append(' ').append(f.length()).append(' ').append(f.lastModified() / 1000).append(' ')
                .append(MusicPlaylistStore.quoted(m.getString("display"))).append('\n');
        }
        save(new File(root, ".catalog-v2"), cache);
        save(new File(root, ".original-names-v1"), originals);
        MusicPlaylistStore.atomic(new File(root, ".display-v1"), titles.toString().getBytes(StandardCharsets.UTF_8));
    }
}
