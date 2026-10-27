package com.libertyvr.quest;

import java.io.File;
import java.io.IOException;

public final class TestPerfCaptureFiles {
    public static void main(String[] args) throws Exception {
        File external = new File(args[0]).getCanonicalFile();
        String valid = "20261027T001530Z-123456789.frames.csv";
        check(PerfCaptureFiles.validName(valid));
        check(PerfCaptureFiles.validName(valid.replace("frames", "session")));
        for (String invalid : new String[] {"../quest_perf.csv", "quest_perf.csv", "/data/private",
                "20261027T001530Z-1.frames.csv/../../saves", "20261027T001530Z-1.frames.csv.tmp", "", "name.csv"}) {
            check(!PerfCaptureFiles.validName(invalid));
            try { PerfCaptureFiles.resolve(external, invalid); throw new AssertionError(); }
            catch (IOException expected) { }
        }
        File result = PerfCaptureFiles.resolve(external, valid);
        check(result.getParentFile().equals(new File(external, "perf-captures")));
        check(result.getName().equals(valid));
        System.out.println("PASS: profiler export filename and canonical-path boundaries");
    }
    private static void check(boolean value) { if (!value) throw new AssertionError(); }
}
