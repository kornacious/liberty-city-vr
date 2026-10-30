package com.libertyvr.quest;

import java.io.File;
import java.io.IOException;

public final class TestPerfCaptureFiles {
    public static void main(String[] args) throws Exception {
        File external = new File(args[0]).getCanonicalFile();
        String valid = "20261027T001530Z-123456789.frames.csv";
        check(PerfCaptureFiles.validName(valid));
        check(PerfCaptureFiles.validName(valid.replace("frames", "session")));
        check(PerfCaptureFiles.validName(valid.replace("frames", "draws")));
        check(!PerfCaptureFiles.validName(valid.replace("frames", "draws"), true));
        for (String invalid : new String[] {"../quest_perf.csv", "quest_perf.csv", "/data/private",
                "20261027T001530Z-1.frames.csv/../../saves", "20261027T001530Z-1.frames.csv.tmp", "", "name.csv"}) {
            check(!PerfCaptureFiles.validName(invalid));
            try { PerfCaptureFiles.resolve(external, invalid); throw new AssertionError(); }
            catch (IOException expected) { }
        }
        File result = PerfCaptureFiles.resolve(external, valid);
        check(result.getParentFile().equals(new File(external, "perf-captures")));
        check(result.getName().equals(valid));
        String demo = valid.replace("frames", "sim");
        String run = valid.replace("frames", "run");
        check(PerfCaptureFiles.validName(demo, true));
        check(PerfCaptureFiles.validName(run, true));
        check(!PerfCaptureFiles.validName(valid, true));
        check(!PerfCaptureFiles.validName(demo, false));
        File replay = PerfCaptureFiles.resolve(external, demo, true);
        check(replay.getParentFile().equals(new File(external, "simulation-replays")));
        for (String invalid : new String[] {"../saves", "20261027T001530Z-1.sim.csv/../../saves",
                "/sdcard/game/gta3.img", "20261027T001530Z-1.sim.csv.tmp", "", "name.sim.csv"}) {
            check(!PerfCaptureFiles.validName(invalid, true));
            try { PerfCaptureFiles.resolve(external, invalid, true); throw new AssertionError(); }
            catch (IOException expected) { }
        }
        String rdc = "20261027T001530Z-123456789_capture.rdc";
        check(PerfCaptureFiles.validRenderDocName(rdc));
        check(PerfCaptureFiles.validRenderDocName(rdc.replace("_capture", "_frame123")));
        check(!PerfCaptureFiles.validName(rdc));
        File capture = PerfCaptureFiles.resolveRenderDoc(external, rdc);
        check(capture.getParentFile().equals(new File(external, "renderdoc-captures")));
        for (String invalid : new String[] {null, "", "../"+rdc, "/sdcard/"+rdc,
                "name.rdc", rdc+".tmp", "20261030T145630Z-1_../gta3.img", "gta3.img",
                "20261030T145630Z-1_capture%2frdc", "20261030T145630Z-1_capture..rdc"}) {
            check(!PerfCaptureFiles.validRenderDocName(invalid));
            try { PerfCaptureFiles.resolveRenderDoc(external, invalid); throw new AssertionError(); }
            catch (IOException expected) { }
        }
        try { PerfCaptureFiles.renderDocDirectory(null); throw new AssertionError(); }
        catch (IOException expected) { }
        System.out.println("PASS: profiler/replay/RenderDoc export filename and canonical-path boundaries");
    }
    private static void check(boolean value) { if (!value) throw new AssertionError(); }
}
