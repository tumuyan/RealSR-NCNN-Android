package com.tumuyan.ncnn.realsr;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.view.View;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

public class LabelEditorActivity extends AppCompatActivity {

    private CommandListManager clm;
    private LabelAdapter adapter;
    private List<LabelItem> items;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_label_editor);
        setTitle(R.string.label_editor_title);

        // 读取 SharedPreferences 中的参数来构建 CommandListManager
        SharedPreferences sp = getSharedPreferences("config", Activity.MODE_PRIVATE);
        String extraPath = sp.getString("extraPath", "").trim();
        String extraCommand = sp.getString("extraCommand", "").trim();
        String classicalFilters = sp.getString("classicalFilters", getString(R.string.default_classical_filters));
        String magickFilters = sp.getString("magickFilters", getString(R.string.default_magick_filters));
        String[] presetLabels = getResources().getStringArray(R.array.style_array);

        clm = new CommandListManager(presetLabels, extraPath, extraCommand,
                classicalFilters.split("\\s+"), magickFilters.split("\\s+"));

        // 加载已有的自定义标签
        String customLabelsJson = sp.getString("customLabels", "");
        clm.loadCustomLabels(customLabelsJson);
        Map<String, String> customMap = clm.getCustomLabelMap();

        // 构建 LabelItem 列表
        items = new ArrayList<>();
        for (int i = 0; i < clm.getCommandCount(); i++) {
            String cmd = clm.getCommandAt(i);
            String fp = CommandListManager.commandFingerprint(cmd);
            String custom = customMap.get(fp);
            items.add(new LabelItem(cmd, fp, clm.defaultLabels[i], custom));
        }

        // 设置 RecyclerView
        RecyclerView recyclerView = findViewById(R.id.recycler_labels);
        recyclerView.setLayoutManager(new LinearLayoutManager(this));
        adapter = new LabelAdapter(items);
        recyclerView.setAdapter(adapter);

        // 复制全部
        findViewById(R.id.btn_copy_all).setOnClickListener(v -> {
            syncEditsFromAdapter();
            String text = clm.exportAllText();
            ClipboardManager clipboard = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
            ClipData clip = ClipData.newPlainText("label_config", text);
            clipboard.setPrimaryClip(clip);
            Toast.makeText(this, R.string.label_copied, Toast.LENGTH_SHORT).show();
        });

        // 粘贴全部
        findViewById(R.id.btn_paste_all).setOnClickListener(v -> {
            ClipboardManager clipboard = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
            if (clipboard.hasPrimaryClip() && clipboard.getPrimaryClip() != null
                    && clipboard.getPrimaryClip().getItemCount() > 0) {
                CharSequence text = clipboard.getPrimaryClip().getItemAt(0).getText();
                if (text != null) {
                    syncEditsFromAdapter();
                    // 先将当前 adapter 中的自定义标签同步到 clm
                    Map<String, String> currentMap = buildMapFromItems();
                    clm.setCustomLabelMap(currentMap);
                    int count = clm.importFromText(text.toString());
                    // 更新 items
                    Map<String, String> newMap = clm.getCustomLabelMap();
                    for (LabelItem item : items) {
                        item.customLabel = newMap.getOrDefault(item.fingerprint, "");
                    }
                    adapter.notifyDataSetChanged();
                    Toast.makeText(this, getString(R.string.label_imported, count), Toast.LENGTH_SHORT).show();
                } else {
                    Toast.makeText(this, R.string.label_clipboard_empty, Toast.LENGTH_SHORT).show();
                }
            } else {
                Toast.makeText(this, R.string.label_clipboard_empty, Toast.LENGTH_SHORT).show();
            }
        });

        // 清除全部自定义标签
        findViewById(R.id.btn_clear_all).setOnClickListener(v -> {
            for (LabelItem item : items) {
                item.customLabel = "";
            }
            adapter.notifyDataSetChanged();
            Toast.makeText(this, R.string.label_cleared, Toast.LENGTH_SHORT).show();
        });

        // 保存
        findViewById(R.id.btn_label_save).setOnClickListener(v -> {
            syncEditsFromAdapter();
            Map<String, String> map = buildMapFromItems();
            clm.setCustomLabelMap(map);
            String json = clm.toCustomLabelJson();
            sp.edit().putString("customLabels", json).apply();
            Toast.makeText(this, R.string.save_succeed, Toast.LENGTH_SHORT).show();
            finish();
        });
    }

    @Override
    public void onPause() {
        super.onPause();
        overridePendingTransition(0, android.R.anim.slide_out_right);
    }

    /**
     * 从 items 列表构建指纹→自定义标签的 Map
     */
    private Map<String, String> buildMapFromItems() {
        java.util.HashMap<String, String> map = new java.util.HashMap<>();
        for (LabelItem item : items) {
            if (item.customLabel != null && !item.customLabel.isEmpty()) {
                map.put(item.fingerprint, item.customLabel);
            }
        }
        return map;
    }

    /**
     * 确保 RecyclerView 中所有 EditText 的内容已同步到 items 列表。
     * 通过清除焦点来触发 OnFocusChangeListener。
     */
    private void syncEditsFromAdapter() {
        View focused = getCurrentFocus();
        if (focused != null) {
            focused.clearFocus();
        }
    }
}
