package com.libertyvr.quest;

import android.app.Activity;
import android.app.AlertDialog;

/** Asset-independent startup notice: works before any game fonts/textures load. */
public final class InstallationStatus {
    public static void show(Activity activity) {
        activity.runOnUiThread(() -> new AlertDialog.Builder(activity)
            .setTitle("Game assets required")
            .setMessage("PC/Xbox base not installed or incomplete. Use Build and install in the Liberty City VR Manager to install a complete PC or Original Xbox variant.")
            .setCancelable(false)
            .setPositiveButton("Close", (dialog, which) -> activity.finish())
            .show());
    }
}
