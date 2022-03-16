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

public class SettingActivity extends AppCompatActivity {
    SharedPreferences mySharePerferences;
    private int selectCommand;

    EditText editDefaultCommand;
    EditText editTile;
    EditText editThread;
    EditText editExtraCommand;
    ToggleButton toggleKeepScreen;
    ToggleButton toggleCPU;
    Spinner spinnerFormat;
    private final String galleryPath = Environment.getExternalStorageDirectory()
            + File.separator + Environment.DIRECTORY_DCIM
            + File.separator + "RealSR" + File.separator;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_setting);

        setTitle(getResources().getText(R.string.setting));

        mySharePerferences = getSharedPreferences("config", Activity.MODE_PRIVATE);
        selectCommand = mySharePerferences.getInt("selectCommand", 0);
        int tileSize = mySharePerferences.getInt("tileSize", 0);
        String defaultCommand = mySharePerferences.getString("defaultCommand", "");
        String threadCount = mySharePerferences.getString("threadCount", "");
        boolean keepScreen = mySharePerferences.getBoolean("keepScreen", false);
        String extraCommand = mySharePerferences.getString("extraCommand", "");
        boolean useCPU = mySharePerferences.getBoolean("useCPU", false);
        int format = mySharePerferences.getInt("format", 0);

        editTile = findViewById(R.id.editTile);
        editTile.setText("" + tileSize);
        editDefaultCommand = findViewById(R.id.editDefaultCommand);
        editDefaultCommand.setText(defaultCommand);
        editExtraCommand = findViewById(R.id.editExtraCommand);
        editExtraCommand.setText(extraCommand);
        editExtraCommand.setHint("./waifu2x-ncnn -i input.png -o output.png -m " + galleryPath + "cunet");
        editThread = findViewById(R.id.editThread);
        editThread.setText(threadCount);
        toggleKeepScreen = findViewById(R.id.toggle_keep_screen);
        toggleKeepScreen.setChecked(keepScreen);
        toggleCPU = findViewById(R.id.toggle_cpu);
        toggleCPU.setChecked(useCPU);

        spinnerFormat = findViewById(R.id.spinner_format);
        spinnerFormat.setSelection(format);

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


        findViewById(R.id.btn_save).setOnClickListener(view -> {
            save();
        });

        findViewById(R.id.btn_reset).setOnClickListener(v -> {
            spinner.setSelection(2);
            spinnerFormat.setSelection(0);
            toggleCPU.setChecked(false);
            editTile.setText("0");
            editThread.setText("");
            editDefaultCommand.setText("./realsr-ncnn -i input.png -o output.png -m models-Real-ESRGANv2-anime -s 2");
            save();
        });

        findViewById(R.id.btn_reset_low).setOnClickListener(v -> {
            spinner.setSelection(9);
            spinnerFormat.setSelection(0);
            toggleCPU.setChecked(false);
            editTile.setText("32");
            editThread.setText("1:1:1");
            editDefaultCommand.setText("");
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

        String threadCount = editThread.getText().toString().trim().replaceAll("[\\s/]+", ":");
        if (threadCount.length() > 0) {
            if (!threadCount.matches("(\\d+):(\\d+):(\\d+)")) {
                editThread.setError(getString(R.string.thread_count_err));
                return;
            }
        }
        editThread.setText(threadCount);
        editor.putString("threadCount", threadCount);

        editor.putBoolean("keepScreen", toggleKeepScreen.isChecked());
        editor.putBoolean("useCPU", toggleCPU.isChecked());
        editor.putInt("format", (int) spinnerFormat.getSelectedItemId());

        editor.apply();
    }
}