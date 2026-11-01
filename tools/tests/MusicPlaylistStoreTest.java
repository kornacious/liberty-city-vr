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
            List<File> library = MusicPlaylistStore.ordered(root);
            List<String> keep = new ArrayList<>(); for(File f:library) keep.add(f.getName());
            String old = MusicPlaylistStore.revision(library);
            List<String> remove = new ArrayList<>(keep.subList(1,4)); keep.removeAll(remove);
            // An atomic playlist failure after staging must restore every audio file.
            File blocker = new File(root,".playlist-v1.tmp"); check(blocker.mkdir());
            try { MusicPlaylistStore.apply(root,keep,remove,old); throw new AssertionError(); } catch(IOException expected) {}
            check(MusicPlaylistStore.revision(MusicPlaylistStore.ordered(root)).equals(old));
            check(!blocker.exists() || blocker.delete());
            try { MusicPlaylistStore.apply(root,keep,remove,"stale"); throw new AssertionError(); } catch(IOException expected) {}
            List<String> traversal = new ArrayList<>(remove); traversal.set(0,"../outside.mp3");
            try { MusicPlaylistStore.apply(root,keep,traversal,old); throw new AssertionError(); } catch(IOException expected) {}
            MusicPlaylistStore.apply(root,keep,remove,old);
            check(MusicPlaylistStore.ordered(root).size() == 10);
            for(String name:remove) check(!new File(root,name).exists());
            check(Arrays.stream(root.listFiles()).noneMatch(f -> f.getName().startsWith(".deleting-")));
            List<String> remaining = new ArrayList<>(); for(File f:MusicPlaylistStore.ordered(root)) remaining.add(f.getName());
            MusicPlaylistStore.apply(root,Collections.emptyList(),remaining,MusicPlaylistStore.revision(MusicPlaylistStore.ordered(root)));
            check(MusicPlaylistStore.ordered(root).isEmpty());
            Files.write(new File(root,"02-old.mp3").toPath(), new byte[]{1});
            Files.write(new File(root,"10-old.mp3").toPath(), new byte[]{2});
            List<String> expected = new ArrayList<>(Arrays.asList("02-old.mp3","10-old.mp3"));
            // Two numbered import batches must remain after the older visible order,
            // even when each new filename sorts before the previously imported ones.
            for (String name : Arrays.asList("01-album-a.mp3","02-album-a.mp3","01-album-b.mp3","02-album-b.mp3")) {
                File upload = new File(root,".incoming"); Files.write(upload.toPath(),name.getBytes());
                MusicPlaylistStore.commitImport(root,upload,name); expected.add(name);
                List<String> actual = new ArrayList<>(); for (File f:MusicPlaylistStore.ordered(root)) actual.add(f.getName());
                check(actual.equals(expected));
            }
            Collections.reverse(expected);
            MusicPlaylistStore.save(root,expected,MusicPlaylistStore.revision(MusicPlaylistStore.ordered(root)));
            File upload = new File(root,".incoming"); Files.write(upload.toPath(),new byte[]{3});
            String beforeFailure = MusicPlaylistStore.revision(MusicPlaylistStore.ordered(root));
            File blocked = new File(root,".playlist-v1.tmp"); check(blocked.mkdir());
            try { MusicPlaylistStore.commitImport(root,upload,"00-latest.mp3"); throw new AssertionError(); } catch(IOException failure) {}
            check(upload.isFile() && !new File(root,"00-latest.mp3").exists());
            check(MusicPlaylistStore.revision(MusicPlaylistStore.ordered(root)).equals(beforeFailure));
            check(!blocked.exists() || blocked.delete());
            MusicPlaylistStore.commitImport(root,upload,"00-latest.mp3"); expected.add("00-latest.mp3");
            List<String> actual = new ArrayList<>(); for (File f:MusicPlaylistStore.ordered(root)) actual.add(f.getName());
            check(actual.equals(expected));
            Files.write(upload.toPath(),new byte[]{4});
            try { MusicPlaylistStore.commitImport(root,upload,"00-latest.mp3"); throw new AssertionError(); } catch(IOException failure) {}
            check(upload.isFile() && new File(root,"00-latest.mp3").length() == 1);
            System.out.println("PASS Java numbered import batches append after legacy/manual order, rollback safely and refuse overwrite");
            System.out.println("PASS Java multi-delete, delete-all, stale/traversal rejection, staging cleanup and rollback");
            System.out.println("PASS Java playlist insertion, persistence, revisions, duplicate rejection, append and metadata fallback");
        } finally { for(File f:root.listFiles()) Files.delete(f.toPath()); Files.delete(root.toPath()); }
    }
}
