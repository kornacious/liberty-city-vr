package com.libertyvr.quest;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.util.List;

public final class TestBenchmarkAssets {
    private static void check(boolean value) { if (!value) throw new AssertionError(); }
    public static void main(String[] args) throws Exception {
        File external = new File(args[0]).getCanonicalFile();
        File data = new File(external, "gamedata/data"); data.mkdirs();
        File models = new File(external, "gamedata/models"); models.mkdirs();
        Files.write(new File(data, "handling.cfg").toPath(), "abc".getBytes("UTF-8"));
        Files.write(new File(models, "test.dff").toPath(), new byte[] {0, 1, 2});
        List<BenchmarkAssets.Entry> inventory = BenchmarkAssets.inventory(external);
        check(inventory.size() == 2);
        check(inventory.get(0).path.equals("data/handling.cfg"));
        check(inventory.get(0).sha256.equals("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
        check(BenchmarkAssets.resolve(external, inventory.get(0).token).equals(new File(data, "handling.cfg")));
        for (String path : new String[] {"../saves/GTA3sf1.b", "data/../models/test.dff", "/data/handling.cfg", "data//handling.cfg", "audio/test.wav", "data\\handling.cfg", "data/a\nfile"}) {
            try { BenchmarkAssets.resolve(external, BenchmarkAssets.token(path)); throw new AssertionError(); }
            catch (IOException expected) { }
        }
        Files.write(new File(data, "HANDLING.cfg").toPath(), "ambiguous".getBytes("UTF-8"));
        // Case-insensitive host filesystems cannot create both names; on those,
        // path/canonical checks remain covered and the inventory has one file.
        if (data.list().length == 2) {
            try { BenchmarkAssets.inventory(external); throw new AssertionError(); }
            catch (IOException expected) { }
        }
        System.out.println("PASS: benchmark asset scope, hashes, paths and case boundaries");
    }
}
