package com.tcmg.app;

import android.content.ActivityNotFoundException;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Toast;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;
import android.content.pm.ActivityInfo;
import androidx.fragment.app.Fragment;
import com.tcmg.app.databinding.FragmentControlBinding;

import java.net.Inet4Address;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.util.Collections;
import java.util.Locale;

public final class ControlFragment extends Fragment {

    private static final String TAG     = "ControlFrag";
    private static final int    POLL_MS = 1200;

    static volatile long serverStartMs = 0L;

    @Nullable private FragmentControlBinding binding;
    private SharedPreferences prefs;
    private final Handler  handler      = new Handler(Looper.getMainLooper());
    private final Runnable pollRunnable = new Runnable() {
        @Override public void run() {
            if (binding != null) { tickPoll(); handler.postDelayed(this, POLL_MS); }
        }
    };

    @Nullable private String wifiIp    = null;
    @Nullable private String hotspotIp = null;

    @Override
    public void onAttach(@NonNull Context context) {
        super.onAttach(context);
        prefs = context.getSharedPreferences(BootReceiver.PREF_FILE, Context.MODE_PRIVATE);
    }

    @Override @Nullable
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
                             @Nullable Bundle savedInstanceState) {
        binding = FragmentControlBinding.inflate(inflater, container, false);
        return binding.getRoot();
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        setupThemePill();
        setupListeners();
        restorePreferences();
    }

    @Override public void onResume() {
        super.onResume();
        requireActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        handler.post(pollRunnable);
    }
    @Override public void onPause() {
        super.onPause();
        requireActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
        handler.removeCallbacks(pollRunnable);
    }
    @Override public void onDestroyView() {
        super.onDestroyView();
        handler.removeCallbacks(pollRunnable);
        binding = null;
    }

    // ── Theme Pill ───────────────────────────────────────────────────────────

    private void setupThemePill() {
        if (binding == null) return;
        MainActivity host = (MainActivity) requireActivity();
        boolean isAmber = host.isEmber();

        // Update visual state
        refreshPillState(isAmber);

        binding.tvThemeMatrix.setOnClickListener(v -> {
            if (host.isEmber()) host.switchTheme(false);
        });
        binding.tvThemeAmber.setOnClickListener(v -> {
            if (!host.isEmber()) host.switchTheme(true);
        });
    }

    private void refreshPillState(boolean isEmber) {
        if (binding == null) return;
        // Pill container background
        binding.themePill.setBackgroundResource(
                isEmber ? R.drawable.bg_theme_pill_ember : R.drawable.bg_theme_pill);
        // Active chip — blue (Void) or amber (Ember)
        int activeChipRes = isEmber ? R.drawable.bg_theme_active_ember : R.drawable.bg_theme_active;
        // Inactive text color
        int inactiveColor = isEmber ? 0xFFa87c3a : 0xFF94a3b8;
        if (isEmber) {
            binding.tvThemeAmber.setBackgroundResource(activeChipRes);
            binding.tvThemeAmber.setTextColor(0xFF1a0f00);   // dark on amber
            binding.tvThemeMatrix.setBackgroundResource(android.R.color.transparent);
            binding.tvThemeMatrix.setTextColor(inactiveColor);
        } else {
            binding.tvThemeMatrix.setBackgroundResource(activeChipRes);
            binding.tvThemeMatrix.setTextColor(0xFFffffff);
            binding.tvThemeAmber.setBackgroundResource(android.R.color.transparent);
            binding.tvThemeAmber.setTextColor(inactiveColor);
        }
    }

    // ── Setup ────────────────────────────────────────────────────────────────

    private void setupListeners() {
        if (binding == null) return;
        binding.btnStart.setOnClickListener(v        -> doStartServer());
        binding.btnStop.setOnClickListener(v         -> doStopServer());
        binding.btnWebifWifi.setOnClickListener(v    -> openWebif(wifiIp));
        binding.btnWebifHotspot.setOnClickListener(v -> openWebif(hotspotIp));

        binding.switchAutostart.setOnCheckedChangeListener((sw, checked) -> {
            prefs.edit().putBoolean(BootReceiver.KEY_AUTOSTART, checked).apply();
            Toast.makeText(requireContext(),
                    checked ? R.string.toast_autostart_on : R.string.toast_autostart_off,
                    Toast.LENGTH_SHORT).show();
        });
    }

    private void restorePreferences() {
        if (binding == null) return;
        binding.switchAutostart.setChecked(prefs.getBoolean(BootReceiver.KEY_AUTOSTART, false));
    }

    // ── Poll ─────────────────────────────────────────────────────────────────

    private void tickPoll() {
        if (binding == null) return;
        boolean running   = TcmgNative.isRunning();
        int     webifPort = running ? TcmgNative.getWebifPort() : -1;
        updateStatus(running);
        refreshNetwork();
        binding.btnWebifWifi.setEnabled(webifPort > 0 && wifiIp != null);
        binding.btnWebifHotspot.setEnabled(webifPort > 0 && hotspotIp != null);
    }

    // ── Server ───────────────────────────────────────────────────────────────

    private void doStartServer() {
        String cfgDir = requireContext().getFilesDir().getAbsolutePath();
        ContextCompat.startForegroundService(requireContext(),
                new Intent(requireContext(), TcmgService.class)
                        .setAction(TcmgService.ACTION_START)
                        .putExtra(TcmgService.EXTRA_CFGDIR, cfgDir)
                        .putExtra(TcmgService.EXTRA_DEBUG, 0));
        serverStartMs = System.currentTimeMillis();
    }

    private void doStopServer() {
        requireContext().startService(new Intent(requireContext(), TcmgService.class)
                .setAction(TcmgService.ACTION_STOP));
        serverStartMs = 0L;
    }

    // ── Status ───────────────────────────────────────────────────────────────

    private void updateStatus(boolean running) {
        if (binding == null) return;
        MainActivity host = (MainActivity) requireActivity();
        int stoppedBg = host.isEmber() ? R.drawable.bg_card_stopped_ember : R.drawable.bg_card_stopped;
        binding.statusCard.setBackground(ContextCompat.getDrawable(requireContext(),
                running ? R.drawable.bg_card_running : stoppedBg));
        binding.statusDot.setImageResource(
                running ? R.drawable.status_dot_running : R.drawable.status_dot_stopped);
        binding.tvStatusText.setText(running ? R.string.status_running : R.string.status_stopped);

        // Running: primary color text; stopped: dim
        int textColor = running
                ? (host.isEmber() ? 0xFFf59e0b : 0xFF3b82f6)
                : (host.isEmber() ? 0xFFfef3c7 : 0xFFe8f0fe);
        binding.tvStatusText.setTextColor(textColor);

        binding.btnStart.setEnabled(!running);
        binding.btnStop.setEnabled(running);

        if (running && serverStartMs > 0L) {
            long s = (System.currentTimeMillis() - serverStartMs) / 1000L;
            binding.tvUptime.setText(String.format(Locale.ROOT, "%02d:%02d:%02d",
                    s / 3600, (s % 3600) / 60, s % 60));
            binding.tvUptime.setVisibility(View.VISIBLE);
            binding.tvUptimeLabel.setVisibility(View.VISIBLE);
        } else {
            binding.tvUptime.setVisibility(View.GONE);
            binding.tvUptimeLabel.setVisibility(View.GONE);
        }
    }

    // ── Network ──────────────────────────────────────────────────────────────

    private void refreshNetwork() {
        if (binding == null) return;
        wifiIp    = detectWifiIp();
        hotspotIp = detectHotspotIp(wifiIp);
        binding.tvWifiIp.setText(wifiIp    != null ? wifiIp    : getString(R.string.hint_not_available));
        binding.tvHotspotIp.setText(hotspotIp != null ? hotspotIp : getString(R.string.hint_not_available));
    }

    @Nullable private static String detectWifiIp() {
        try {
            for (NetworkInterface nif : Collections.list(NetworkInterface.getNetworkInterfaces())) {
                if (!nif.isUp() || nif.isLoopback()) continue;
                String name = nif.getName().toLowerCase(Locale.ROOT);
                if (name.startsWith("wlan") || name.startsWith("eth")) {
                    String ip = firstIpv4(nif);
                    if (ip != null) return ip;
                }
            }
        } catch (SocketException e) { Log.w(TAG, "detectWifiIp: " + e.getMessage()); }
        return null;
    }

    @Nullable private static String detectHotspotIp(@Nullable String wifiIp) {
        try {
            for (NetworkInterface nif : Collections.list(NetworkInterface.getNetworkInterfaces())) {
                if (!nif.isUp() || nif.isLoopback()) continue;
                String name = nif.getName().toLowerCase(Locale.ROOT);
                boolean isAp = name.startsWith("ap") || name.startsWith("swlan")
                        || name.equals("wlan1") || name.startsWith("p2p");
                if (!isAp) continue;
                String ip = firstIpv4(nif);
                if (ip != null && !ip.equals(wifiIp)) return ip;
            }
        } catch (SocketException e) { Log.w(TAG, "detectHotspotIp: " + e.getMessage()); }
        return null;
    }

    @Nullable private static String firstIpv4(@NonNull NetworkInterface nif) {
        for (java.net.InetAddress addr : Collections.list(nif.getInetAddresses()))
            if (!addr.isLoopbackAddress() && addr instanceof Inet4Address)
                return addr.getHostAddress();
        return null;
    }

    // ── WebIF ─────────────────────────────────────────────────────────────────

    private void openWebif(@Nullable String ip) {
        int port = TcmgNative.isRunning() ? TcmgNative.getWebifPort() : -1;
        if (port <= 0 || ip == null) {
            Toast.makeText(requireContext(), R.string.toast_webif_unavailable, Toast.LENGTH_SHORT).show();
            return;
        }
        String url = "http://" + ip + ":" + port;
        try {
            startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(url))
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        } catch (ActivityNotFoundException e) {
            ClipboardManager cm = (ClipboardManager)
                    requireContext().getSystemService(Context.CLIPBOARD_SERVICE);
            if (cm != null) cm.setPrimaryClip(ClipData.newPlainText("WebIF URL", url));
            Toast.makeText(requireContext(), "URL copied: " + url, Toast.LENGTH_LONG).show();
        }
    }
}
