package com.tcmg.app;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.IBinder;
import android.os.PowerManager;
import android.util.Log;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.app.NotificationCompat;

/**
 * Foreground service that hosts the native TCMG card-server.
 *
 * <p>Accepts two intents:
 * <ul>
 *   <li>{@link #ACTION_START} — starts the native server (if not already running)</li>
 *   <li>{@link #ACTION_STOP}  — stops the native server and removes the foreground
 *       notification</li>
 * </ul>
 */
public final class TcmgService extends Service {

    private static final String TAG = "TcmgService";

    public static final String ACTION_START  = "com.tcmg.app.START";
    public static final String ACTION_STOP   = "com.tcmg.app.STOP";
    public static final String EXTRA_CFGDIR  = "cfgdir";
    public static final String EXTRA_DEBUG   = "debug";

    static final String CHANNEL_ID = "tcmg_service_channel";
    static final int    NOTIF_ID   = 1001;

    private PowerManager.WakeLock mWakeLock;

    // ════════════════════════════════════════════════════════════════════════

    @Override
    public int onStartCommand(@Nullable Intent intent, int flags, int startId) {
        createNotificationChannel();

        // Handle explicit STOP
        if (intent != null && ACTION_STOP.equals(intent.getAction())) {
            Log.i(TAG, "ACTION_STOP received");
            TcmgNative.stopServer();
            releaseWakeLock();
            stopForeground(STOP_FOREGROUND_REMOVE); // API 24 = minSdk, always safe
            stopSelf();
            return START_NOT_STICKY;
        }

        // Promote to foreground
        startForeground(NOTIF_ID, buildNotification());

        // Acquire WakeLock to keep CPU alive when screen is off
        acquireWakeLock();

        // Start native server (guard against duplicate starts)
        if (!TcmgNative.isRunning()) {
            String cfgDir = intent != null ? intent.getStringExtra(EXTRA_CFGDIR) : null;
            int    debug  = intent != null ? intent.getIntExtra(EXTRA_DEBUG, 0)   : 0;
            Log.i(TAG, "Starting native server (cfgDir=" + cfgDir + " debug=" + debug + ")");
            int rc = TcmgNative.startServer(cfgDir, debug);
            if (rc != 0) Log.e(TAG, "startServer returned " + rc);
        } else {
            Log.d(TAG, "Server already running — skipping startServer()");
        }

        return START_STICKY;
    }

    @Override
    @Nullable
    public IBinder onBind(@NonNull Intent intent) {
        return null; // Not a bound service
    }

    @Override
    public void onDestroy() {
        Log.i(TAG, "onDestroy — stopping native server");
        TcmgNative.stopServer();
        releaseWakeLock();
        super.onDestroy();
    }

    // ════════════════════════════════════════════════════════════════════════
    // WakeLock
    // ════════════════════════════════════════════════════════════════════════

    private void acquireWakeLock() {
        if (mWakeLock != null && mWakeLock.isHeld()) return;
        PowerManager pm = (PowerManager) getSystemService(POWER_SERVICE);
        if (pm == null) return;
        mWakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "tcmg:server");
        mWakeLock.setReferenceCounted(false);
        mWakeLock.acquire(); // held until releaseWakeLock() is called
        Log.d(TAG, "WakeLock acquired");
    }

    private void releaseWakeLock() {
        if (mWakeLock != null && mWakeLock.isHeld()) {
            mWakeLock.release();
            Log.d(TAG, "WakeLock released");
        }
        mWakeLock = null;
    }

    // ════════════════════════════════════════════════════════════════════════
    // Notification
    // ════════════════════════════════════════════════════════════════════════

    @NonNull
    private Notification buildNotification() {
        // Tap → open MainActivity
        PendingIntent mainPi = PendingIntent.getActivity(
                this, 0,
                new Intent(this, MainActivity.class).setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP),
                PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);

        // Stop action
        PendingIntent stopPi = PendingIntent.getService(
                this, 1,
                new Intent(this, TcmgService.class).setAction(ACTION_STOP),
                PendingIntent.FLAG_IMMUTABLE);

        int    webifPort = TcmgNative.isRunning() ? TcmgNative.getWebifPort() : -1;
        String contentText = webifPort > 0
                ? getString(R.string.notif_running, webifPort)
                : getString(R.string.status_running);

        NotificationCompat.Builder builder = new NotificationCompat.Builder(this, CHANNEL_ID)
                .setContentTitle(getString(R.string.notif_title))
                .setContentText(contentText)
                .setSmallIcon(android.R.drawable.ic_media_play)
                .setContentIntent(mainPi)
                .setOngoing(true)
                .setPriority(NotificationCompat.PRIORITY_LOW)
                .addAction(android.R.drawable.ic_media_pause,
                        getString(R.string.notif_action_stop), stopPi);

        // WebIF action — show only when webif is running
        if (webifPort > 0) {
            PendingIntent webPi = PendingIntent.getActivity(
                    this, 2,
                    new Intent(Intent.ACTION_VIEW,
                            Uri.parse("http://127.0.0.1:" + webifPort)),
                    PendingIntent.FLAG_IMMUTABLE);
            builder.addAction(android.R.drawable.ic_menu_view,
                    getString(R.string.notif_action_webif), webPi);
        }

        return builder.build();
    }

    private void createNotificationChannel() {
        // NotificationChannel requires API 26+; minSdk is 24, so check is still needed
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationManager nm = getSystemService(NotificationManager.class);
            if (nm == null) return; // should never happen, but guard for safety
            NotificationChannel ch = new NotificationChannel(
                    CHANNEL_ID,
                    getString(R.string.notif_channel_name),
                    NotificationManager.IMPORTANCE_LOW);
            ch.setDescription(getString(R.string.notif_channel_desc));
            ch.setShowBadge(false);
            nm.createNotificationChannel(ch);
        }
    }
}
