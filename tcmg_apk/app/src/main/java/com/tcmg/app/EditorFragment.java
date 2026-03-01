package com.tcmg.app;

import android.content.Context;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Toast;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import com.google.android.material.tabs.TabLayout;
import com.tcmg.app.databinding.FragmentEditorBinding;

import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;

/**
 * Editor Fragment — forces landscape for maximum editing space.
 */
public final class EditorFragment extends Fragment {

    private static final String[] FILE_NAMES = {"tcmg.conf", "tcmg.srvid2"};

    @Nullable private FragmentEditorBinding binding;
    private int     activeFileIndex = 0;
    private boolean unsavedChanges  = false;

    @Override @Nullable
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
                             @Nullable Bundle savedInstanceState) {
        binding = FragmentEditorBinding.inflate(inflater, container, false);
        return binding.getRoot();
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        setupTabs();
        setupButtons();
        setupTextWatcher();
        loadFile(activeFileIndex);
    }

    @Override
    public void onResume() {
        super.onResume();
        // Force landscape — maximises editor width
        requireActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR);
        refreshWarning();
    }

    @Override
    public void onPause() {
        super.onPause();
        // Restore free rotation when leaving editor
        requireActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        binding = null;
    }

    // ── Setup ─────────────────────────────────────────────────────────────────

    private void setupTabs() {
        if (binding == null) return;
        binding.tabLayoutEditor.addTab(binding.tabLayoutEditor.newTab().setText(R.string.editor_tab_conf));
        binding.tabLayoutEditor.addTab(binding.tabLayoutEditor.newTab().setText(R.string.editor_tab_servid2));

        binding.tabLayoutEditor.addOnTabSelectedListener(new TabLayout.OnTabSelectedListener() {
            @Override public void onTabSelected(TabLayout.Tab tab) {
                if (unsavedChanges) showUnsavedDialog(tab.getPosition());
                else { activeFileIndex = tab.getPosition(); loadFile(activeFileIndex); }
            }
            @Override public void onTabUnselected(TabLayout.Tab tab) {}
            @Override public void onTabReselected(TabLayout.Tab tab) {}
        });
    }

    private void setupButtons() {
        if (binding == null) return;
        binding.btnSave.setOnClickListener(v -> saveCurrentFile());
        binding.btnReload.setOnClickListener(v -> { unsavedChanges = false; loadFile(activeFileIndex); });
    }

    private void setupTextWatcher() {
        if (binding == null) return;
        binding.etEditor.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int st, int c, int a) {}
            @Override public void onTextChanged(CharSequence s, int st, int b, int c) {}
            @Override public void afterTextChanged(Editable e) { unsavedChanges = true; }
        });
    }

    private void refreshWarning() {
        if (binding == null) return;
    }

    // ── File I/O ──────────────────────────────────────────────────────────────

    private void loadFile(int index) {
        if (binding == null) return;
        File file = getConfigFile(index);
        if (!file.exists()) {
            binding.etEditor.setText("");
            binding.etEditor.setHint(getString(R.string.editor_empty));
            unsavedChanges = false;
            return;
        }
        try (FileReader reader = new FileReader(file)) {
            char[] buf = new char[(int) file.length()];
            int len = reader.read(buf);
            binding.etEditor.setText(len > 0 ? new String(buf, 0, len) : "");
            binding.etEditor.setHint("");
            binding.etEditor.setSelection(0);
            unsavedChanges = false;
        } catch (IOException e) {
            binding.etEditor.setText("");
            binding.etEditor.setHint("Error reading file: " + e.getMessage());
        }
    }

    private void saveCurrentFile() {
        if (binding == null) return;
        File file = getConfigFile(activeFileIndex);
        if (!file.getParentFile().exists()) file.getParentFile().mkdirs();
        String content = binding.etEditor.getText() != null
                ? binding.etEditor.getText().toString() : "";
        try (FileWriter writer = new FileWriter(file, false)) {
            writer.write(content);
            writer.flush();
            unsavedChanges = false;
            Toast.makeText(requireContext(), R.string.editor_saved, Toast.LENGTH_SHORT).show();
        } catch (IOException e) {
            Toast.makeText(requireContext(),
                    getString(R.string.editor_save_error, e.getMessage()),
                    Toast.LENGTH_LONG).show();
        }
    }

    @NonNull
    private File getConfigFile(int index) {
        return new File(requireContext().getFilesDir().getAbsolutePath(), FILE_NAMES[index]);
    }

    private void showUnsavedDialog(int targetIndex) {
        if (getContext() == null) return;
        new androidx.appcompat.app.AlertDialog.Builder(requireContext())
                .setTitle("Unsaved Changes")
                .setMessage("Discard unsaved changes?")
                .setPositiveButton("Discard", (d, w) -> {
                    unsavedChanges = false;
                    activeFileIndex = targetIndex;
                    loadFile(activeFileIndex);
                    if (binding != null)
                        binding.tabLayoutEditor.selectTab(binding.tabLayoutEditor.getTabAt(targetIndex));
                })
                .setNegativeButton("Keep Editing", (d, w) -> {
                    if (binding != null)
                        binding.tabLayoutEditor.selectTab(binding.tabLayoutEditor.getTabAt(activeFileIndex));
                })
                .show();
    }
}
