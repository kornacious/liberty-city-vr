package com.libertyvr.quest;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.HashSet;
import java.util.UUID;

/** Complete-variant activation runs as the app UID, not the scoped-storage shell. */
public final class VariantFiles {
    private static final String[] REQUIRED = {
        "data/main.scm", "data/default.dat", "data/gta3.dat", "data/handling.cfg", "data/weapon.dat",
        "models/gta3.img", "models/gta3.dir", "models/frontend.txd", "models/menu.txd", "models/fonts.txd",
        "anim/ped.ifp", "anim/cuts.img", "anim/cuts.dir", "text/american.gxt", "audio/sfx.raw", "audio/sfx.sdt"
    };
    public static void activate(File gameData, String argument) throws IOException {
        if(argument == null || !argument.matches("(pc|xbox):\\.stage-(pc|xbox)-[a-f0-9]{32}"))
            throw new IOException("Invalid variant activation request");
        String[] parts = argument.split(":", 2);
        String id = parts[0];
        if(!parts[1].startsWith(".stage-" + id + "-")) throw new IOException("Variant staging identity mismatch");
        File rawVariants = new File(gameData.getCanonicalFile(), "variants");
        File variants = rawVariants.getCanonicalFile();
        if(!variants.equals(rawVariants.getAbsoluteFile())) throw new IOException("Linked variant directory");
        File stage = new File(variants, parts[1]);
        if(!stage.getCanonicalFile().equals(stage.getAbsoluteFile())) throw new IOException("Linked variant staging directory");
        String identity = id.equals("pc") ? "LCVR_VARIANT_PC_1" : "LCVR_VARIANT_XBOX_1";
        HashSet<String> files = new HashSet<>();
        File inventory = new File(stage,"VARIANT_INFO.txt");
        if(!inventory.getCanonicalFile().equals(inventory.getAbsoluteFile())) throw new IOException("Linked variant inventory");
        try(BufferedReader input = new BufferedReader(new InputStreamReader(new FileInputStream(inventory), StandardCharsets.UTF_8))) {
            if(!identity.equals(input.readLine())) throw new IOException("Invalid variant identity");
            String line;
            while((line = input.readLine()) != null) {
                int tab = line.indexOf('\t');
                if(tab < 1) throw new IOException("Invalid variant inventory");
                String path = line.substring(tab+1);
                if(path.isEmpty() || path.startsWith("/") || path.contains("..") || path.contains("\\") || path.contains(":"))
                    throw new IOException("Unsafe variant inventory path");
                long bytes;
                try { bytes = Long.parseLong(line.substring(0,tab)); } catch(NumberFormatException error) { throw new IOException("Invalid variant size"); }
                File file = new File(stage,path);
                if(bytes < 0 || !files.add(path) || files.size() > 20000 || !file.isFile() || file.length() != bytes ||
                    !file.getCanonicalFile().equals(file.getAbsoluteFile()) || !file.getCanonicalPath().startsWith(stage.getCanonicalPath() + File.separator)) throw new IOException("Incomplete variant content");
                try(FileInputStream readable = new FileInputStream(file)) { readable.read(); }
            }
        }
        for(String path : REQUIRED) if(!files.contains(path)) throw new IOException("Missing required variant content");
        File destination = new File(variants,id);
        if(destination.exists() && (!destination.getCanonicalFile().equals(destination.getAbsoluteFile()) ||
            !new File(destination,"VARIANT_INFO.txt").isFile())) throw new IOException("Refusing to replace unmanaged variant data");
        try(FileOutputStream marker = new FileOutputStream(new File(variants,"MANAGED.txt"))) {
            marker.write("Liberty City VR complete variants v1\n".getBytes(StandardCharsets.UTF_8));
        }
        File backup = new File(variants,".backup-"+id+"-"+UUID.randomUUID().toString());
        boolean previous = destination.exists();
        if(previous && !destination.renameTo(backup)) throw new IOException("Cannot preserve previous variant");
        if(!stage.renameTo(destination)) {
            if(previous && !backup.renameTo(destination)) throw new IOException("Variant activation and rollback failed; backup preserved");
            throw new IOException("Variant activation failed; previous variant restored");
        }
    }
}
