package com.tcmg.app;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.util.Log;
import androidx.annotation.NonNull;
import androidx.core.content.ContextCompat;

/**
 * Receives {@code BOOT_COMPLETED} (and {@code MY_PACKAGE_REPLACED}) and
 * auto-starts {@link TcmgService} when the user has enabled auto-start.
 */
public final class BootReceiver extends BroadcastReceiver {

    private static final String TAG = "BootReceiver";

    /** SharedPreferences file name — shared across the whole application. */
    static final String PREF_FILE     = "tcmg_prefs";

    /** Key: whether auto-start on boot is enabled (boolean, default false). */
    static final String KEY_AUTOSTART = "autostart_enabled";

    @Override
    public void onReceive(@NonNull Context context, @NonNull Intent intent) {
        Log.d(TAG, "onReceive: " + intent.getAction());

        SharedPreferences prefs =
                context.getSharedPreferences(PREF_FILE, Context.MODE_PRIVATE);

        if (!prefs.getBoolean(KEY_AUTOSTART, false)) {
            Log.d(TAG, "Auto-start disabled — skipping");
            return;
        }

        // Use app-private files directory — always accessible, no permissions needed
        String cfgDir = context.getFilesDir().getAbsolutePath();
        Log.i(TAG, "Auto-starting TCMG service (cfgDir=" + cfgDir + ")");

        Intent svc = new Intent(context, TcmgService.class)
                .setAction(TcmgService.ACTION_START)
                .putExtra(TcmgService.EXTRA_CFGDIR, cfgDir)
                .putExtra(TcmgService.EXTRA_DEBUG, 0);

        ContextCompat.startForegroundService(context, svc);
    }
}
