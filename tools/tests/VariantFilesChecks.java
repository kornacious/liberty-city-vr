package com.libertyvr.quest;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.UUID;

public final class VariantFilesChecks {
    private static final String[] REQUIRED = { "data/main.scm", "data/default.dat", "data/gta3.dat", "data/handling.cfg", "data/weapon.dat",
        "models/gta3.img", "models/gta3.dir", "models/frontend.txd", "models/menu.txd", "models/fonts.txd",
        "anim/ped.ifp", "anim/cuts.img", "anim/cuts.dir", "text/american.gxt", "audio/sfx.raw", "audio/sfx.sdt" };
    private static File stage(File root,String id,char value) throws IOException {
        File stage = new File(root,"variants/.stage-"+id+"-"+UUID.randomUUID().toString().replace("-",""));
        StringBuilder inventory = new StringBuilder(id.equals("pc")?"LCVR_VARIANT_PC_1\n":"LCVR_VARIANT_XBOX_1\n");
        for(String path:REQUIRED) {
            File file=new File(stage,path);file.getParentFile().mkdirs();Files.write(file.toPath(),new byte[]{(byte)value});inventory.append("1\t").append(path).append('\n');
        }
        Files.writeString(new File(stage,"VARIANT_INFO.txt").toPath(),inventory,StandardCharsets.UTF_8);return stage;
    }
    private static void check(boolean value) {if(!value)throw new AssertionError("Variant activation contract failed");}
    private static void reject(File root,String request) throws IOException {
        try {VariantFiles.activate(root,request);throw new AssertionError("Accepted unsafe or incomplete activation");}catch(IOException expected){}
    }
    public static void main(String[] args) throws Exception {
        File root=new File(args[0]);root.mkdirs();File first=stage(root,"pc",'a');
        reject(root,"pc:../outside");reject(root,"xbox:"+first.getName());
        VariantFiles.activate(root,"pc:"+first.getName());
        File active=new File(root,"variants/pc/data/main.scm");check(Files.readString(active.toPath()).equals("a"));
        File incomplete=stage(root,"pc",'b');Files.delete(new File(incomplete,"models/frontend.txd").toPath());
        reject(root,"pc:"+incomplete.getName());check(Files.readString(active.toPath()).equals("a"));
        File corrupt=stage(root,"pc",'b');Files.writeString(new File(corrupt,"data/main.scm").toPath(),"bad size");
        reject(root,"pc:"+corrupt.getName());check(Files.readString(active.toPath()).equals("a"));
        File replacement=stage(root,"pc",'b');VariantFiles.activate(root,"pc:"+replacement.getName());check(Files.readString(active.toPath()).equals("b"));
        check(new File(root,"variants").listFiles(f->f.getName().startsWith(".backup-pc-")).length==1);
        File xbox=stage(root,"xbox",'x');new File(root,"variants/xbox").mkdirs();Files.writeString(new File(root,"variants/xbox/unmanaged.txt").toPath(),"preserve");
        reject(root,"xbox:"+xbox.getName());check(new File(root,"variants/xbox/unmanaged.txt").isFile());
        check(new File(root,"variants/MANAGED.txt").isFile());
        File both=new File(root,"both");File pcStage=stage(both,"pc",'p');File xboxStage=stage(both,"xbox",'x');
        VariantFiles.activate(both,"pc:"+pcStage.getName());VariantFiles.activate(both,"xbox:"+xboxStage.getName());
        check(Files.readString(new File(both,"variants/pc/data/main.scm").toPath()).equals("p"));
        check(Files.readString(new File(both,"variants/xbox/data/main.scm").toPath()).equals("x"));
        System.out.println("PASS app-owned variant activation: identity, traversal, missing/corrupt files, coexistence and retained backups");
    }
}
