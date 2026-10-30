package com.libertyvr.quest;

import java.io.File;
import java.io.IOException;

/** Fixed-name read-only diagnostics boundary, also exercised by host tests. */
final class PerfCaptureFiles {
    static boolean validRenderDocName(String name) {
        return name != null && name.length() <= 192 && name.matches(
                "[0-9]{8}T[0-9]{6}Z-[0-9]+(_[A-Za-z0-9-]+)*\\.rdc");
    }

    static File renderDocDirectory(File externalRoot) throws IOException {
        if (externalRoot == null) throw new IOException("External storage unavailable");
        File expected = new File(externalRoot.getCanonicalFile(), "renderdoc-captures");
        if (!expected.getCanonicalFile().equals(expected)) {
            throw new IOException("RenderDoc directory must not be redirected");
        }
        return expected;
    }

    static File resolveRenderDoc(File externalRoot, String name) throws IOException {
        if (!validRenderDocName(name)) throw new IOException("Invalid RenderDoc name");
        File expected = new File(renderDocDirectory(externalRoot), name);
        if (!expected.getCanonicalFile().equals(expected)) {
            throw new IOException("RenderDoc file must not be redirected");
        }
        return expected;
    }

    static boolean validName(String name) {
        return validName(name, false);
    }

    static boolean validName(String name, boolean replay) {
        return name != null && name.length() <= 128 && name.matches(
                "[0-9]{8}T[0-9]{6}Z-[0-9]+\\." + (replay ? "(sim|run)" : "(frames|session|draws)") + "\\.csv");
    }

    static File directory(File externalRoot) throws IOException {
        return directory(externalRoot, false);
    }

    static File directory(File externalRoot, boolean replay) throws IOException {
        if (externalRoot == null) throw new IOException("External storage unavailable");
        File expected = new File(externalRoot.getCanonicalFile(), replay ? "simulation-replays" : "perf-captures");
        if (!expected.getCanonicalFile().equals(expected)) {
            throw new IOException("Capture directory must not be redirected");
        }
        return expected;
    }

    static File resolve(File externalRoot, String name) throws IOException {
        return resolve(externalRoot, name, false);
    }

    static File resolve(File externalRoot, String name, boolean replay) throws IOException {
        if (!validName(name, replay)) throw new IOException("Invalid diagnostics name");
        File root = directory(externalRoot, replay);
        File file = new File(root, name).getCanonicalFile();
        if (!root.equals(file.getParentFile())) throw new IOException("Capture escapes diagnostics directory");
        return file;
    }
}
