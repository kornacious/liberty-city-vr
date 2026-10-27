package com.libertyvr.quest;

import java.io.File;
import java.io.IOException;

/** Fixed-name read-only diagnostics boundary, also exercised by host tests. */
final class PerfCaptureFiles {
    static boolean validName(String name) {
        return name != null && name.matches("[0-9]{8}T[0-9]{6}Z-[0-9]+\\.(frames|session)\\.csv");
    }

    static File directory(File externalRoot) throws IOException {
        if (externalRoot == null) throw new IOException("External storage unavailable");
        File expected = new File(externalRoot.getCanonicalFile(), "perf-captures");
        if (!expected.getCanonicalFile().equals(expected)) {
            throw new IOException("Capture directory must not be redirected");
        }
        return expected;
    }

    static File resolve(File externalRoot, String name) throws IOException {
        if (!validName(name)) throw new IOException("Invalid performance capture name");
        File root = directory(externalRoot);
        File file = new File(root, name).getCanonicalFile();
        if (!root.equals(file.getParentFile())) throw new IOException("Capture escapes diagnostics directory");
        return file;
    }
}
