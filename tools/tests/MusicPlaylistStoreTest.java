import com.libertyvr.quest.MusicPlaylistStore;
import java.io.*;
import java.nio.file.*;
import java.util.*;
public class MusicPlaylistStoreTest {
    static void check(boolean ok) { if (!ok) throw new AssertionError(); }
    public static void main(String[] args) throws Exception {
        File root = Files.createTempDirectory("lcvr-playlist-test").toFile();
        try {
            for (int i=1; i<=12; ++i) Files.write(new File(root, String.format("%02d.mp3", i)).toPath(), new byte[]{1});
            List<File> before = MusicPlaylistStore.ordered(root);
            String revision = MusicPlaylistStore.revision(before);
            List<String> names = new ArrayList<>(); for(File f:before) names.add(f.getName());
            names.add(1, names.remove(9));
            MusicPlaylistStore.save(root,names,revision);
            List<File> after = MusicPlaylistStore.ordered(root);
            check(after.get(1).getName().equals("10.mp3") && after.get(2).getName().equals("02.mp3"));
            try { MusicPlaylistStore.save(root,names,revision); throw new AssertionError(); } catch(IOException expected) {}
            String current = MusicPlaylistStore.revision(after);
            List<String> duplicate = new ArrayList<>(names); duplicate.set(0,duplicate.get(1));
            try { MusicPlaylistStore.save(root,duplicate,current); throw new AssertionError(); } catch(IOException expected) {}
            Files.write(new File(root,"new.ogg").toPath(),new byte[]{2});
            check(MusicPlaylistStore.ordered(root).get(12).getName().equals("new.ogg"));
            check(MusicPlaylistStore.display("song.mp3","Artist","Album","Title").equals("Artist - Album - Title"));
            check(MusicPlaylistStore.display("song.mp3","","","").equals("song.mp3"));
            check(new File(root,".rescan").isFile());
            System.out.println("PASS Java playlist insertion, persistence, revisions, duplicate rejection, append and metadata fallback");
        } finally { for(File f:root.listFiles()) Files.delete(f.toPath()); Files.delete(root.toPath()); }
    }
}
