package com.libertyvr.quest;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.util.*;

/** Shared on-disk contract with the native scanner; no audio or Android UI dependencies. */
public final class MusicPlaylistStore {
    private MusicPlaylistStore() {}
    public static boolean music(File file) {
        try {
            String n = file.getName();
            return file.isFile() && file.getCanonicalFile().equals(file.getAbsoluteFile()) &&
                n.length() <= 200 && !n.startsWith(".") && !n.matches(".*[\\p{Cntrl}].*") &&
                n.matches("(?i).*\\.(mp3|m4a|ogg|opus|wav|flac)");
        } catch (IOException e) { return false; }
    }
    public static List<File> ordered(File root) throws IOException {
        Map<String, File> files = new TreeMap<>();
        File[] children = root.listFiles();
        if (children != null) for (File f : children) if (music(f)) files.put(f.getName(), f);
        if (files.size() > 1000) throw new IOException("Library exceeds 1000 files");
        List<File> ordered = new ArrayList<>();
        File playlist = new File(root, ".playlist-v1");
        if (playlist.isFile() && playlist.length() <= 262144)
            for (String name : Files.readAllLines(playlist.toPath(), StandardCharsets.UTF_8)) {
                File file = files.remove(name); if (file != null) ordered.add(file);
            }
        ordered.addAll(files.values()); return ordered;
    }
    public static String revision(List<File> files) throws Exception {
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        for (File f : files) digest.update((f.getName() + "\u0000" + f.length() + "\u0000" + f.lastModified() + "\n").getBytes(StandardCharsets.UTF_8));
        StringBuilder result = new StringBuilder();
        for (byte b : digest.digest()) result.append(String.format(java.util.Locale.ROOT, "%02x", b & 255));
        return result.toString();
    }
    public static void save(File root, List<String> names, String expected) throws Exception {
        List<File> files = ordered(root);
        if (!revision(files).equals(expected)) throw new IOException("Library changed; refresh before saving order");
        Set<String> actual = new HashSet<>(); for (File f : files) actual.add(f.getName());
        if (names.size() != actual.size() || new HashSet<>(names).size() != names.size() || !actual.equals(new HashSet<>(names)))
            throw new IOException("Playlist must contain each current music file exactly once");
        atomic(new File(root, ".playlist-v1"), (String.join("\n", names) + "\n").getBytes(StandardCharsets.UTF_8));
        rescan(root);
    }
    public static void atomic(File destination, byte[] data) throws IOException {
        File temp = new File(destination.getParentFile(), destination.getName() + ".tmp");
        try {
            try (FileOutputStream out = new FileOutputStream(temp)) { out.write(data); out.getFD().sync(); }
            Files.move(temp.toPath(), destination.toPath(), StandardCopyOption.REPLACE_EXISTING, StandardCopyOption.ATOMIC_MOVE);
        } finally { if (temp.exists()) temp.delete(); }
    }
    public static void rescan(File root) throws IOException {
        atomic(new File(root, ".rescan"), Long.toString(System.nanoTime()).getBytes(StandardCharsets.US_ASCII));
    }
    public static String clean(String value) {
        String text = value == null ? "" : value.replaceAll("[\\p{Cntrl}]", " ").trim();
        return text.substring(0, Math.min(240, text.length()));
    }
    public static String display(String filename, String artist, String album, String title) {
        List<String> parts = new ArrayList<>();
        if (!clean(artist).isEmpty()) parts.add(clean(artist));
        if (!clean(album).isEmpty()) parts.add(clean(album));
        parts.add(clean(title).isEmpty() ? clean(filename) : clean(title));
        return String.join(" - ", parts);
    }
    public static String quoted(String value) { return "\"" + value.replaceAll("[\\p{Cntrl}]", " ").replace("\\", "\\\\").replace("\"", "\\\"") + "\""; }
}
