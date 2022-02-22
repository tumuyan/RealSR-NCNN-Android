package com.tumuyan.ncnn.realsr;

import androidx.appcompat.app.AppCompatActivity;

import android.app.Activity;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.EditText;
import android.widget.Spinner;

public class SettingActivity extends AppCompatActivity {
    SharedPreferences mySharePerferences;
    private int selectCommand;

    EditText editDefaultCommand;
    EditText editTile;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_setting);

        setTitle(getResources().getText(R.string.setting));

        mySharePerferences = getSharedPreferences("config", Activity.MODE_PRIVATE);
        selectCommand = mySharePerferences.getInt("selectCommand", 0);
        int tileSize = mySharePerferences.getInt("tileSize", 0);
        String defaultCommand = mySharePerferences.getString("defaultCommand", "");

        editTile = findViewById(R.id.editTile);
        editTile.setText("" + tileSize);
        editDefaultCommand = findViewById(R.id.editDefaultCommand);
        editDefaultCommand.setText(defaultCommand);

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
        editor.putInt("tileSize", Integer.parseInt(editTile.getText().toString()));
        editor.putString("defaultCommand", editDefaultCommand.getText().toString());
        editor.apply();

    }
}