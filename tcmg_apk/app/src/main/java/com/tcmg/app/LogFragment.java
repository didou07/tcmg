package com.tcmg.app;

import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import com.tcmg.app.databinding.FragmentLogBinding;

/**
 * Log Fragment — live terminal log from the native ring buffer.
 *
 * <p>Forces landscape orientation when visible to maximise line width.
 * Orientation is restored to "unspecified" when the user navigates away.
 *
 * <p>No clear button — the log is append-only from the device's perspective.
 * The ring buffer in native code handles capacity automatically.
 */
public final class LogFragment extends Fragment {

    private static final int    POLL_MS   = 1000;
    private static final int    MAX_LINES = 200;
    private static final int    MAX_CHARS = 60_000;

    @Nullable private FragmentLogBinding binding;
    private final Handler  handler      = new Handler(Looper.getMainLooper());
    private final Runnable pollRunnable = new Runnable() {
        @Override public void run() {
            if (binding != null) {
                pullLogs();
                updateStatusBar();
                handler.postDelayed(this, POLL_MS);
            }
        }
    };

    private int    logNextId  = 0;
    private int    lineCount  = 0;

    // ════════════════════════════════════════════════════════════════════════

    @Override
    @Nullable
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
                             @Nullable Bundle savedInstanceState) {
        binding = FragmentLogBinding.inflate(inflater, container, false);
        return binding.getRoot();
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        // Show initial placeholder
        if (binding.tvLog.getText().length() == 0) {
            appendLine(getString(R.string.log_ready));
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        // Force landscape for maximum log visibility
        requireActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR);
        updateRotateHint();
        handler.post(pollRunnable);
    }

    @Override
    public void onPause() {
        super.onPause();
        // Restore free rotation
        requireActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
        handler.removeCallbacks(pollRunnable);
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        handler.removeCallbacks(pollRunnable);
        binding = null;
    }

    // ════════════════════════════════════════════════════════════════════════
    // Log polling
    // ════════════════════════════════════════════════════════════════════════

    private void pullLogs() {
        if (binding == null) return;
        int[]  nextId = {logNextId};
        String lines  = TcmgNative.getLogLines(logNextId, MAX_LINES, nextId);
        if (lines == null || lines.isEmpty()) return;
        logNextId = nextId[0];
        appendLine(lines);
    }

    private void appendLine(@NonNull String text) {
        if (binding == null) return;
        String cur = binding.tvLog.getText().toString();
        if (cur.length() > MAX_CHARS) {
            int trim = cur.indexOf('\n', cur.length() - MAX_CHARS / 2);
            cur = trim > 0 ? cur.substring(trim + 1) : "";
        }
        String newText = cur + text;
        binding.tvLog.setText(newText);
        // Count lines
        lineCount = 0;
        for (int i = 0; i < newText.length(); i++) {
            if (newText.charAt(i) == '\n') lineCount++;
        }
        scrollToBottom();
    }

    private void scrollToBottom() {
        if (binding == null) return;
        binding.svLog.post(() -> {
            if (binding != null) binding.svLog.fullScroll(View.FOCUS_DOWN);
        });
    }

    private void updateStatusBar() {
        if (binding == null) return;
        boolean running = TcmgNative.isRunning();
        binding.tvLogStatus.setText(running ? "● live" : "○ idle");
        binding.tvLogStatus.setTextColor(running ? 0xFF3FB950 : 0xFF586069);
        binding.tvLogLineCount.setText(lineCount + " lines");
    }

    private void updateRotateHint() {
        if (binding == null) return;
        int orientation = requireActivity().getResources().getConfiguration().orientation;
        boolean isLandscape = orientation == Configuration.ORIENTATION_LANDSCAPE;
        binding.tvRotateHint.setVisibility(isLandscape ? View.GONE : View.VISIBLE);
    }
}
