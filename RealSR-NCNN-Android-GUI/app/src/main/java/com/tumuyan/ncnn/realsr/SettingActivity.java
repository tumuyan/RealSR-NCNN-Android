package com.tumuyan.ncnn.realsr;

import androidx.appcompat.app.AppCompatActivity;

import android.app.Activity;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.ToggleButton;

import java.io.File;
import java.text.MessageFormat;

public class SettingActivity extends AppCompatActivity {
    SharedPreferences mySharePerferences;
    private int selectCommand;

    EditText editDefaultCommand, editMagickFilters, editClassicalFilters;
    EditText editTile;
    EditText editThread;
    EditText editExtraCommand;
    EditText editExtraPath;
    EditText editSavePath;
    EditText editMNNBackend;
    ToggleButton toggleKeepScreen;

    ToggleButton toggleCPU;
    ToggleButton toggleMultFiles;
    ToggleButton togglePrePng;
    ToggleButton togglePreFrame;
    ToggleButton toggleAutoSave;
    ToggleButton toggleSearchView;
    ToggleButton toggleFinalCommand;
    Spinner spinnerFormat, spinnerName, spinnerName2, spinnerOrientation, spinnerNotify;
    private final String galleryPath = Environment.getExternalStorageDirectory()
            + File.separator + Environment.DIRECTORY_DCIM
            + File.separator + "RealSR";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_setting);

        setTitle(getResources().getText(R.string.setting));

        mySharePerferences = getSharedPreferences("config", Activity.MODE_PRIVATE);
        selectCommand = mySharePerferences.getInt("selectCommand", 0);
        int tileSize = mySharePerferences.getInt("tileSize", 0);
        String extraCommand = mySharePerferences.getString("extraCommand", "");
        String defaultCommand = mySharePerferences.getString("defaultCommand", "");
        String classicalFilters = mySharePerferences.getString("classicalFilters", getString(R.string.default_classical_filters));
        String magickFilters = mySharePerferences.getString("magickFilters", getString(R.string.default_magick_filters));
        String threadCount = mySharePerferences.getString("threadCount", "");
        boolean keepScreen = mySharePerferences.getBoolean("keepScreen", false);
        boolean useMultFiles = mySharePerferences.getBoolean("useMultFiles", false);
        boolean prePng = mySharePerferences.getBoolean("PrePng", true);
        boolean preFrame = mySharePerferences.getBoolean("PreFrame", true);
        String extraPath = mySharePerferences.getString("extraPath", "");
        String savePath = mySharePerferences.getString("savePath", "");
        boolean useCPU = mySharePerferences.getBoolean("useCPU", false);
        boolean autoSave = mySharePerferences.getBoolean("autoSave", false);
        boolean showSearchView = mySharePerferences.getBoolean("showSearchView", false);
        boolean showFinalCommand = mySharePerferences.getBoolean("showFinalCommand", false);
        int format = mySharePerferences.getInt("format", 0);
        int name = mySharePerferences.getInt("name", 0);
        int name2 = mySharePerferences.getInt("name2", 0);
        int orientation = mySharePerferences.getInt("ORIENTATION", 0);
        int notify = mySharePerferences.getInt("notify", 0);
        int mnnBackend = mySharePerferences.getInt("mnnBackend", 3);

        editTile = findViewById(R.id.editTile);
        editTile.setText(String.format("%d", tileSize));
        editExtraCommand = findViewById(R.id.editExtraCommand);
        editExtraCommand.setText(extraCommand);
        editExtraCommand.setHint("./waifu2x-ncnn -i input.png -o output.png -m " + galleryPath + "cunet");
        editDefaultCommand = findViewById(R.id.editDefaultCommand);
        editDefaultCommand.setText(defaultCommand);
        editClassicalFilters = findViewById(R.id.editClassicalFilters);
        editClassicalFilters.setText(classicalFilters);
        editMagickFilters = findViewById(R.id.editMagickFilters);
        editMagickFilters.setText(magickFilters);
        editExtraPath = findViewById(R.id.editExtraPath);
        editExtraPath.setText(extraPath);
        editSavePath = findViewById(R.id.editSavePath);
        editSavePath.setText(savePath);
        editSavePath.setHint(galleryPath);
        editThread = findViewById(R.id.editThread);
        editThread.setText(threadCount);
        editMNNBackend = findViewById(R.id.editMNNBackend);
        editMNNBackend.setText(MessageFormat.format("{0}", mnnBackend));
        toggleKeepScreen = findViewById(R.id.toggle_keep_screen);
        toggleKeepScreen.setChecked(keepScreen);
        toggleMultFiles = findViewById(R.id.toggle_mult_files);
        toggleMultFiles.setChecked(useMultFiles);
        togglePrePng = findViewById(R.id.toggle_pre_png);
        togglePrePng.setChecked(prePng);
        togglePreFrame = findViewById(R.id.toggle_pre_frames);
        togglePreFrame.setChecked(preFrame);
        toggleAutoSave = findViewById(R.id.toggle_auto_save);
        toggleAutoSave.setChecked(autoSave);
        toggleCPU = findViewById(R.id.toggle_cpu);
        toggleCPU.setChecked(useCPU);
        toggleSearchView = findViewById(R.id.toggle_serarch_view);
        toggleSearchView.setChecked(showSearchView);
        toggleFinalCommand = findViewById(R.id.toggle_final_command);
        toggleFinalCommand.setChecked(showFinalCommand);

        spinnerFormat = findViewById(R.id.spinner_format);
        spinnerFormat.setSelection(format);

        spinnerName = findViewById(R.id.spinner_name);
        spinnerName.setSelection(name);
        spinnerName2 = findViewById(R.id.spinner_name2);
        spinnerName2.setSelection(name2);
        spinnerOrientation = findViewById(R.id.spinner_orientation);
        spinnerOrientation.setSelection(orientation);
        spinnerNotify = findViewById(R.id.spinner_notify);
        spinnerNotify.setSelection(notify);

        Spinner spinner = findViewById(R.id.spinner);
        spinner.setSelection(selectCommand);
        spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
                selectCommand = pos;
                Log.i("setOnItemSelectedListener", "select " + pos);
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
            }
        });

/*
        findViewById(R.id.btn_download_models).setOnClickListener(view -> {
            Intent intent = new Intent(this, DownloadActivity.class);
            this.startActivity(intent);
            overridePendingTransition(0, android.R.anim.slide_out_right);
        });
*/


        findViewById(R.id.btn_save).setOnClickListener(view -> {
            save();
        });

        findViewById(R.id.btn_reset).setOnClickListener(v -> {
            spinner.setSelection(2);
            spinnerFormat.setSelection(0);
            spinnerName.setSelection(0);
            spinnerName2.setSelection(0);
            toggleCPU.setChecked(false);
            toggleAutoSave.setChecked(false);
            toggleSearchView.setChecked(false);
            toggleFinalCommand.setChecked(false);
            editSavePath.setText("");
            editTile.setText("0");
            editThread.setText("");
            editExtraPath.setText("");
            editMNNBackend.setText("3");
            editDefaultCommand.setText("./realsr-ncnn -i input.png -o output.png -m models-Real-ESRGANv3-anime -s 2");
            editClassicalFilters.setText(getString(R.string.default_classical_filters));
            editMagickFilters.setText(getString(R.string.default_magick_filters));
            save();
        });

        findViewById(R.id.btn_reset_low).setOnClickListener(v -> {
            spinner.setSelection(9);
            spinnerFormat.setSelection(0);
            spinnerName.setSelection(0);
            spinnerName2.setSelection(0);
            toggleCPU.setChecked(false);
            toggleAutoSave.setChecked(false);
            toggleSearchView.setChecked(false);
            toggleFinalCommand.setChecked(false);
            editSavePath.setText("");
            editTile.setText("32");
            editThread.setText("1:1:1");
            editExtraPath.setText("");
            editMNNBackend.setText("3");
            editDefaultCommand.setText("");
            editClassicalFilters.setText(getString(R.string.default_classical_filters));
            editMagickFilters.setText(getString(R.string.default_magick_filters));
            save();
        });

    }


    @Override
    public void onPause() {
        super.onPause();
        overridePendingTransition(0, android.R.anim.slide_out_right);
//        overridePendingTransition(android.R.anim.fade_in, android.R.anim.slide_out_right);
    }


    private void save() {
        SharedPreferences.Editor editor = mySharePerferences.edit();
        editor.putInt("selectCommand", selectCommand);

        String tileSize = editTile.getText().toString();
        if (tileSize.length() < 1) {
            tileSize = "0";
            editTile.setText(tileSize);
        }
        editor.putInt("tileSize", Integer.parseInt(tileSize));
        editor.putString("defaultCommand", editDefaultCommand.getText().toString());

        String extraCommand = editExtraCommand.getText().toString().trim();
        extraCommand = extraCommand.replaceAll("\\s*\n\\s*", "\n");
        editExtraCommand.setText(extraCommand);
        editor.putString("extraCommand", extraCommand);


        String classicalFilters = editClassicalFilters.getText().toString().trim().replaceAll("\\s+", " ");
        editClassicalFilters.setText(classicalFilters);
        editor.putString("classicalFilters", classicalFilters);

        String magickFilters = editMagickFilters.getText().toString().trim().replaceAll("\\s+", " ");
        editMagickFilters.setText(magickFilters);
        editor.putString("magickFilters", magickFilters);

        String threadCount = editThread.getText().toString().trim().replaceAll("[\\s/]+", ":");
        if (threadCount.length() > 0) {
            if (!threadCount.matches("(\\d+):(\\d+):(\\d+)")) {
                editThread.setError(getString(R.string.thread_count_err));
                return;
            }
        }

        String extraPath = editExtraPath.getText().toString().trim();
        if (folderHasErr(extraPath, editExtraPath))
            return;
        editor.putString("extraPath", extraPath);

        String savePath = editSavePath.getText().toString().trim();
        if (folderHasErr(savePath, editSavePath))
            return;
        editor.putString("savePath", savePath);

        editThread.setText(threadCount);
        editor.putString("threadCount", threadCount);

        editor.putBoolean("keepScreen", toggleKeepScreen.isChecked());
        editor.putBoolean("useMultFiles", toggleMultFiles.isChecked());
        editor.putBoolean("PrePng", togglePrePng.isChecked());
        editor.putBoolean("PreFrame", togglePreFrame.isChecked());
        editor.putBoolean("useCPU", toggleCPU.isChecked());
        editor.putBoolean("autoSave", toggleAutoSave.isChecked());
        editor.putBoolean("showSearchView", toggleSearchView.isChecked());
        editor.putBoolean("showFinalCommand", toggleFinalCommand.isChecked());
        editor.putInt("format", (int) spinnerFormat.getSelectedItemId());
        editor.putInt("name", (int) spinnerName.getSelectedItemId());
        editor.putInt("name2", (int) spinnerName2.getSelectedItemId());
        editor.putInt("ORIENTATION", (int) spinnerOrientation.getSelectedItemId());
        editor.putInt("notify", (int) spinnerNotify.getSelectedItemId());
        editor.putInt("mnnBackend", Integer.parseInt(editMNNBackend.getText().toString()));
        editor.apply();
    }

    public boolean folderHasErr(String path, EditText editText) {
        if (!path.isEmpty()) {
            File file = new File(path);
            if (!file.exists()) {
                editText.setError(getString(R.string.path_not_exist));
                return true;
            }
            if (!file.isDirectory()) {
                editText.setError(getString(R.string.path_not_dir));
                return true;
            }
        }
        return false;
    }
}

