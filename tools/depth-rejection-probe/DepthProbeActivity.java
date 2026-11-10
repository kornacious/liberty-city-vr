package com.libertyvr.depthprobe;

import android.app.Activity;
import android.os.Bundle;
import android.view.WindowManager;
import android.widget.TextView;
import java.io.File;

public final class DepthProbeActivity extends Activity {
    static { System.loadLibrary("depthprobe"); }
    private static native String runProbe(String directory, boolean transition);
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        final TextView status = new TextView(this);
        status.setTextSize(20);
        status.setPadding(32, 32, 32, 32);
        final boolean transition = new File(getFilesDir(), "transition.flag").isFile();
        status.setText("Synthetic depth-rejection control\nRunning " + (transition ? "eight transition" : "three baseline") + " cases. No game assets are loaded.");
        setContentView(status);
        new Thread(() -> {
            final String result = runProbe(getFilesDir().getAbsolutePath(), transition);
            runOnUiThread(() -> status.setText(result));
        }, "depth-control").start();
    }
}
