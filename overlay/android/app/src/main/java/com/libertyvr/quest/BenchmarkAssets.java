package com.libertyvr.quest;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Base64;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;

/** Local shell-only benchmark inventory/reads, never arbitrary app files or saves. */
final class BenchmarkAssets {
    static final String[] ROOTS = {"data", "models", "anim", "txd", "modelsets"};
    static final class Entry {
        final String path, token, sha256;
        final long size;
        Entry(String path, long size, String sha256) {
            this.path = path; this.size = size; this.sha256 = sha256;
            this.token = token(path);
        }
    }
    static String token(String path) {
        return Base64.getUrlEncoder().withoutPadding().encodeToString(path.getBytes(StandardCharsets.UTF_8));
    }
    static String key(String path) {
        StringBuilder out = new StringBuilder();
        for (char c : path.toCharArray()) out.append(c >= 'A' && c <= 'Z' ? (char)(c-'A'+'a') : c);
        return out.toString();
    }
    private static File root(File external) throws IOException {
        if (external == null) throw new IOException("Storage unavailable");
        File game = new File(external.getCanonicalFile(), "gamedata");
        if (!game.equals(game.getCanonicalFile())) throw new IOException("Redirected game directory");
        return game;
    }
    static File resolve(File external, String encoded) throws IOException {
        if (encoded == null || encoded.length() > 2048 || !encoded.matches("[A-Za-z0-9_-]+")) throw new IOException("Invalid asset token");
        String path;
        try { path = new String(Base64.getUrlDecoder().decode(encoded), StandardCharsets.UTF_8); }
        catch (IllegalArgumentException error) { throw new IOException("Invalid asset token"); }
        if (!token(path).equals(encoded) || path.indexOf('\\') >= 0 || path.indexOf(':') >= 0 || path.length() > 1024)
            throw new IOException("Invalid asset path");
        String[] parts = path.split("/", -1);
        boolean allowed = false;
        for (String name : ROOTS) if (parts[0].equals(name)) allowed = true;
        if (!allowed || parts.length < 2 || parts.length > 18) throw new IOException("Asset scope denied");
        for (String part : parts) {
            if (part.isEmpty() || part.equals(".") || part.equals("..")) throw new IOException("Invalid asset component");
            for (char c : part.toCharArray()) if (c < 32 || c == 127) throw new IOException("Invalid asset component");
        }
        File file = new File(root(external), path);
        if (!file.equals(file.getCanonicalFile()) || !file.isFile()) throw new IOException("Asset unavailable or redirected");
        return file;
    }
    private static void walk(File external, File game, File directory, int depth, List<Entry> out, long[] total) throws IOException {
        if (depth > 16 || !directory.equals(directory.getCanonicalFile())) throw new IOException("Invalid asset tree");
        File[] files = directory.listFiles();
        if (files == null) throw new IOException("Unreadable asset directory");
        for (File file : files) {
            if (!file.equals(file.getCanonicalFile())) throw new IOException("Redirected asset");
            if (file.isDirectory()) { walk(external, game, file, depth+1, out, total); continue; }
            if (!file.isFile()) throw new IOException("Unsupported asset entry");
            String path = file.getPath().substring(game.getPath().length()+1).replace(File.separatorChar, '/');
            resolve(external, token(path));
            long size = file.length(), modified = file.lastModified();
            if (size > 2147483648L || out.size() >= 20000 || (total[0] += size) > 34359738368L) throw new IOException("Asset inventory limit exceeded");
            MessageDigest digest;
            try { digest = MessageDigest.getInstance("SHA-256"); } catch (NoSuchAlgorithmException error) { throw new IOException("SHA-256 unavailable"); }
            byte[] buffer = new byte[65536];
            try (FileInputStream stream = new FileInputStream(file)) {
                int count;
                while ((count = stream.read(buffer)) >= 0) digest.update(buffer, 0, count);
            }
            if (size != file.length() || modified != file.lastModified()) throw new IOException("Asset changed during inventory");
            StringBuilder hex = new StringBuilder();
            for (byte value : digest.digest()) { hex.append("0123456789abcdef".charAt((value&255)>>>4)); hex.append("0123456789abcdef".charAt(value&15)); }
            out.add(new Entry(path, size, hex.toString()));
        }
    }
    static List<Entry> inventory(File external) throws IOException {
        File game = root(external); List<Entry> entries = new ArrayList<>(); long[] total = {0};
        for (String name : ROOTS) {
            File directory = new File(game, name);
            if (directory.exists()) walk(external, game, directory, 0, entries, total);
        }
        entries.sort(Comparator.comparing(entry -> key(entry.path)));
        HashSet<String> names = new HashSet<>();
        for (Entry entry : entries) if (!names.add(key(entry.path))) throw new IOException("Ambiguous asset filename case");
        if (entries.isEmpty()) throw new IOException("No benchmark assets");
        return entries;
    }
}
