package com.tcmg.app;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.PowerManager;
import android.provider.Settings;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;
import com.tcmg.app.databinding.ActivityMainBinding;

/**
 * MainActivity — adaptive navigation
 *   Portrait  → BottomNavigationView  (binding.bottomNav)
 *   Landscape → NavigationRailView    (binding.navRail)
 */
public final class MainActivity extends AppCompatActivity {

    public static final String PREF_FILE  = "tcmg_prefs";
    public static final String KEY_THEME  = "ui_theme";
    public static final String VAL_VOID   = "void";    // Indigo
    public static final String VAL_EMBER  = "ember";   // Pink/Crimson

    /** Legacy compat — old "matrix" value maps to void */
    public static final String VAL_MATRIX = "matrix";
    public static final String VAL_AMBER  = "amber";

    private static final String TAG_CONTROL = "frag_control";
    private static final String TAG_LOG     = "frag_log";
    private static final String TAG_EDITOR  = "frag_editor";
    private static final String KEY_TAB     = "active_tab";

    private ActivityMainBinding binding;
    private SharedPreferences   prefs;
    private String              activeTag = TAG_CONTROL;

    // ── Lifecycle ────────────────────────────────────────────────────────────

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        prefs = getSharedPreferences(PREF_FILE, Context.MODE_PRIVATE);
        applyTheme();
        super.onCreate(savedInstanceState);

        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        if (savedInstanceState != null) {
            activeTag = savedInstanceState.getString(KEY_TAB, TAG_CONTROL);
        }

        initFragments();
        setupNavigation();
        requestIgnoreBatteryOptimizations();
    }

    @Override
    protected void onSaveInstanceState(@NonNull Bundle out) {
        super.onSaveInstanceState(out);
        out.putString(KEY_TAB, activeTag);
    }

    // ── Battery Optimization ─────────────────────────────────────────────────

    /**
     * On Android 6+, ask the user to exempt this app from Doze/battery optimization.
     * Without this, the OS may kill the foreground service when the screen is off.
     */
    private void requestIgnoreBatteryOptimizations() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return;
        PowerManager pm = (PowerManager) getSystemService(POWER_SERVICE);
        if (pm == null) return;
        String pkg = getPackageName();
        if (!pm.isIgnoringBatteryOptimizations(pkg)) {
            Intent intent = new Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS);
            intent.setData(Uri.parse("package:" + pkg));
            startActivity(intent);
        }
    }

    // ── Theme ─────────────────────────────────────────────────────────────────

    private void applyTheme() {
        String t = prefs.getString(KEY_THEME, VAL_VOID);
        boolean ember = VAL_EMBER.equals(t) || VAL_AMBER.equals(t);
        setTheme(ember ? R.style.Theme_TCMG_Alt : R.style.Theme_TCMG);
    }

    public boolean isEmber() {
        String t = prefs.getString(KEY_THEME, VAL_VOID);
        return VAL_EMBER.equals(t) || VAL_AMBER.equals(t);
    }

    public void switchTheme(boolean ember) {
        prefs.edit().putString(KEY_THEME, ember ? VAL_EMBER : VAL_VOID).apply();
        recreate();
    }

    // ── Fragments ─────────────────────────────────────────────────────────────

    private void initFragments() {
        FragmentManager fm = getSupportFragmentManager();
        Fragment control = fm.findFragmentByTag(TAG_CONTROL);
        Fragment log     = fm.findFragmentByTag(TAG_LOG);
        Fragment editor  = fm.findFragmentByTag(TAG_EDITOR);

        FragmentTransaction tx = fm.beginTransaction();
        if (control == null) { control = new ControlFragment(); tx.add(R.id.fragmentContainer, control, TAG_CONTROL); }
        if (log     == null) { log     = new LogFragment();     tx.add(R.id.fragmentContainer, log,     TAG_LOG);     }
        if (editor  == null) { editor  = new EditorFragment();  tx.add(R.id.fragmentContainer, editor,  TAG_EDITOR);  }

        applyVisibility(tx, control, log, editor, activeTag);
        tx.commitNow();
        applyOrientationForTab(activeTag);
        updatePageTitle(activeTag);
    }

    private void showTab(@NonNull String tag) {
        if (tag.equals(activeTag)) return;
        activeTag = tag;

        FragmentManager fm = getSupportFragmentManager();
        Fragment control = fm.findFragmentByTag(TAG_CONTROL);
        Fragment log     = fm.findFragmentByTag(TAG_LOG);
        Fragment editor  = fm.findFragmentByTag(TAG_EDITOR);
        if (control == null || log == null || editor == null) return;

        FragmentTransaction tx = fm.beginTransaction();
        tx.setCustomAnimations(android.R.anim.fade_in, android.R.anim.fade_out);
        applyVisibility(tx, control, log, editor, tag);
        tx.commit();
        applyOrientationForTab(tag);
        updatePageTitle(tag);
    }

    private static void applyVisibility(
            @NonNull FragmentTransaction tx,
            @NonNull Fragment control, @NonNull Fragment log, @NonNull Fragment editor,
            @NonNull String activeTag) {
        if (TAG_CONTROL.equals(activeTag)) tx.show(control).hide(log).hide(editor);
        else if (TAG_LOG.equals(activeTag)) tx.show(log).hide(control).hide(editor);
        else                               tx.show(editor).hide(control).hide(log);
    }

    // ── Navigation ────────────────────────────────────────────────────────────

    private void setupNavigation() {
        int selectedId = tagToNavId(activeTag);

        if (binding.bottomNav != null) {
            binding.bottomNav.setSelectedItemId(selectedId);
            binding.bottomNav.setOnItemSelectedListener(item -> {
                showTab(navIdToTag(item.getItemId()));
                return true;
            });
        }

        if (binding.navRail != null) {
            binding.navRail.setSelectedItemId(selectedId);
            binding.navRail.setOnItemSelectedListener(item -> {
                showTab(navIdToTag(item.getItemId()));
                return true;
            });
        }
    }

    // ── Helpers ───────────────────────────────────────────────────────────────

    private int tagToNavId(@NonNull String tag) {
        if (TAG_LOG.equals(tag))    return R.id.nav_log;
        if (TAG_EDITOR.equals(tag)) return R.id.nav_editor;
        return R.id.nav_control;
    }

    @NonNull
    private String navIdToTag(int id) {
        if (id == R.id.nav_log)    return TAG_LOG;
        if (id == R.id.nav_editor) return TAG_EDITOR;
        return TAG_CONTROL;
    }

    private void applyOrientationForTab(@NonNull String tag) {
        if (TAG_LOG.equals(tag) || TAG_EDITOR.equals(tag)) {
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        } else {
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
        }
    }

    private void updatePageTitle(@NonNull String tag) {
        int resId;
        if (TAG_LOG.equals(tag))         resId = R.string.nav_log;
        else if (TAG_EDITOR.equals(tag)) resId = R.string.nav_editor;
        else                             resId = R.string.nav_control;
        setTitle(resId);
    }
}
