package com.tumuyan.ncnn.realsr;

import androidx.appcompat.app.AppCompatActivity;

import android.app.Activity;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.widget.AdapterView;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.TextView;

public class SettingActivity extends AppCompatActivity {
    SharedPreferences mySharePerferences;
    private int selectCommand;

    EditText editDefaultCommand;
    EditText editTile;
    EditText editThread;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_setting);

        setTitle(getResources().getText(R.string.setting));

        mySharePerferences = getSharedPreferences("config", Activity.MODE_PRIVATE);
        selectCommand = mySharePerferences.getInt("selectCommand", 0);
        int tileSize = mySharePerferences.getInt("tileSize", 0);
        String defaultCommand = mySharePerferences.getString("defaultCommand", "");
        String threadCount = mySharePerferences.getString("threadCount","");

        editTile = findViewById(R.id.editTile);
        editTile.setText("" + tileSize);
        editDefaultCommand = findViewById(R.id.editDefaultCommand);
        editDefaultCommand.setText(defaultCommand);
        editThread = findViewById(R.id.editThread);
        editThread.setText(threadCount);

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
            editTile.setText("0");
            editThread.setText("");
            editDefaultCommand.setText("./realsr-ncnn -i input.png -o output.png -m models-Real-ESRGANv2-anime -s 2");
            save();
        });

        findViewById(R.id.btn_reset_low).setOnClickListener(v -> {
            spinner.setSelection(9);
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
        if(tileSize.length()<1){
            tileSize="0";
            editTile.setText(tileSize);
        }
        editor.putInt("tileSize", Integer.parseInt(tileSize));
        editor.putString("defaultCommand", editDefaultCommand.getText().toString());

        String threadCount = editThread.getText().toString().trim().replaceAll("[\\s/]+",":");
        if(threadCount.length()>0) {
            if (!threadCount.matches("(\\d+):(\\d+):(\\d+)")) {
                editThread.setError(getString(R.string.thread_count_err));
                return;
            }
        }
        editThread.setText(threadCount);
        editor.putString("threadCount",threadCount);

        editor.apply();
    }
}