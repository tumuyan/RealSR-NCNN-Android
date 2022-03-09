package com.tumuyan.ncnn.realsr;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.icu.text.SimpleDateFormat;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.SearchView;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import com.davemorrissey.labs.subscaleview.ImageSource;
import com.davemorrissey.labs.subscaleview.SubsamplingScaleImageView;

import java.io.BufferedReader;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.util.Date;

public class MainActivity extends AppCompatActivity {
    private static final int SELECT_IMAGE = 1;
    private static final int MY_PERMISSIONS_REQUEST = 100;
    private int selectCommand = 0;
    private String threadCount = "";
    private SubsamplingScaleImageView imageView;
    private TextView logTextView;
    private boolean initProcess;
    private final String galleryPath = Environment.getExternalStorageDirectory()
            + File.separator + Environment.DIRECTORY_DCIM
            + File.separator + "RealSR" + File.separator;
    private File outputFile;
    private String dir;
    // dir="/data/data/com.tumuyan.ncnn.realsr/cache/realsr";
    private String modelName = "SR";
    private SearchView searchView;
    private MenuItem progress;
    private Spinner spinner;
    private Process process;
    private boolean newTask;


    private final String[] command = new String[]{
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGAN-anime",
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGAN",
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGANv2-anime -s 2",
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGANv2-anime",
            "./realsr-ncnn -i input.png -o output.png  -m models-DF2K_JPEG",
            "./realsr-ncnn -i input.png -o output.png  -m models-DF2K",
            "./srmd-ncnn -i input.png -o output.png  -m models-srmd -s 4",
            "./srmd-ncnn -i input.png -o output.png  -m models-srmd -s 3",
            "./srmd-ncnn -i input.png -o output.png  -m models-srmd -s 2",
            "./realcugan-ncnn -i input.png -o output.png  -m models-nose -s 2  -n 0",
            "./realcugan-ncnn -i input.png -o output.png  -m models-se -s 2  -n -1",
            "./realcugan-ncnn -i input.png -o output.png  -m models-se -s 2  -n 0",
            "./realcugan-ncnn -i input.png -o output.png  -m models-se -s 2  -n 1",
            "./realcugan-ncnn -i input.png -o output.png  -m models-se -s 2  -n 2",
            "./realcugan-ncnn -i input.png -o output.png  -m models-se -s 2  -n 3",
            "./realcugan-ncnn -i input.png -o output.png  -m models-se -s 4  -n -1",
            "./realcugan-ncnn -i input.png -o output.png  -m models-se -s 4  -n 0",
            "./realcugan-ncnn -i input.png -o output.png  -m models-se -s 4  -n 3",
            "./resize-ncnn -i input.png -o output.png  -m nearest -s 2",
            "./resize-ncnn -i input.png -o output.png  -m nearest -s 4",
            "./resize-ncnn -i input.png -o output.png  -m bilinear -s 2",
            "./resize-ncnn -i input.png -o output.png  -m bilinear -s 4"
    };
    private int tileSize;
    private boolean keepScreen;

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.main_menu, menu);
        progress = menu.findItem(R.id.progress);
        if (initProcess) {
            initProcess = false;
            progress.setTitle("");
            Log.i("onCreateOptionsMenu", "onCreate() done");
        }
        progress.setOnMenuItemClickListener(item -> {
            stopCommand();
            return false;
        });
        return true;
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        return super.onPrepareOptionsMenu(menu);
    }

    @Override
    public void onResume() {
        super.onResume();

        SharedPreferences mySharePerferences = getSharedPreferences("config", Activity.MODE_PRIVATE);
        tileSize = mySharePerferences.getInt("tileSize", 0);
        threadCount = mySharePerferences.getString("threadCount", "");
        keepScreen = mySharePerferences.getBoolean("keepScreen", false);

    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        imageView = findViewById(R.id.photo_view);
        logTextView = findViewById(R.id.tv_log);
        searchView = findViewById(R.id.serarch_view);


        SharedPreferences mySharePerferences = getSharedPreferences("config", Activity.MODE_PRIVATE);
        int version = mySharePerferences.getInt("version", 0);
        String defaultCommand = mySharePerferences.getString("defaultCommand", "");
        searchView.setQuery(defaultCommand, false);

        dir = this.getCacheDir().getAbsolutePath();
        AssetsCopyer.releaseAssets(this,
                "realsr", dir
                , version == BuildConfig.VERSION_CODE
        );

        SharedPreferences.Editor editor = mySharePerferences.edit();
        editor.putInt("version", BuildConfig.VERSION_CODE);
        editor.apply();

        dir = dir + "/realsr";

        outputFile = new File(dir, "output.png");

        run_command("chmod 777 " + dir + " -R");

        spinner = findViewById(R.id.spinner);
        selectCommand = mySharePerferences.getInt("selectCommand", 2);
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


        searchView.setOnQueryTextListener(new SearchView.OnQueryTextListener() {
            @Override
            public boolean onQueryTextSubmit(String query) {

                String q = searchView.getQuery().toString().trim();


                if (q.equals("help")) {
                    logTextView.setText(getString(R.string.default_log));
                } else if (q.equals("lr")) {
                    imageView.setVisibility(View.VISIBLE);
                    imageView.setImage(ImageSource.uri(dir + "/input.png"));
                    logTextView.setText(getString(R.string.lr));
                } else if (q.equals("hr")) {
                    imageView.setVisibility(View.VISIBLE);
                    imageView.setImage(ImageSource.uri(dir + "/output.png"));
                    logTextView.setText(getString(R.string.hr));
                } else if (q.startsWith("show ")) {
                    imageView.setVisibility(View.VISIBLE);
                    imageView.setImage(ImageSource.uri(q.replaceFirst("(\\s+)show(\\s+)", "")));
                    logTextView.setText(getString(R.string.show));
                } else {
                    stopCommand();
                    new Thread(() -> run20(query)).start();
                }
                return false;
            }

            //用户输入字符时激发该方法
            @Override
            public boolean onQueryTextChange(String newText) {
                if (newText.trim().length() < 2) {
                    if (progress != null)
                        progress.setTitle("");
                    return true;
                }
                if (imageView.getVisibility() == View.VISIBLE)
                    imageView.setVisibility(View.GONE);
                return true;
            }
        });
        findViewById(R.id.btn_open).setOnClickListener(view -> {
            Intent i = new Intent(Intent.ACTION_PICK);
            i.setType("image/*");
            startActivityForResult(i, SELECT_IMAGE);
        });

        findViewById(R.id.btn_save).setOnClickListener(view -> {

            SimpleDateFormat f = new SimpleDateFormat("MMdd_HHmmss");
            String filePath = galleryPath + modelName + "_" + f.format(new Date()) + ".png";
            run_command("cp " + dir + "/output.png " + filePath);
            File file = new File(filePath);
            if (file.exists()) {
                Intent intent = new Intent(Intent.ACTION_MEDIA_SCANNER_SCAN_FILE);
                Uri uri = Uri.fromFile(file);
                intent.setData(uri);
                sendBroadcast(intent);
                Toast.makeText(getApplicationContext(), R.string.save_succeed, Toast.LENGTH_SHORT).show();
            } else {
                Toast.makeText(getApplicationContext(), R.string.save_fail, Toast.LENGTH_SHORT).show();
            }
        });

        findViewById(R.id.btn_run).setOnClickListener(view -> {
            progress.setTitle("");
            {
                stopCommand();
                outputFile.delete();
                if (keepScreen) {
                    view.setKeepScreenOn(true);
                }
                new Thread(() -> {
                    if (selectCommand >= command.length || selectCommand < 0) {
                        Log.w("btn_run.onClick", "select=" + selectCommand + ", length=" + command.length);
                        selectCommand = 0;
                    }
                    StringBuffer cmd = new StringBuffer(command[selectCommand]);
                    if (tileSize > 0)
                        cmd.append(" -t ").append(tileSize);
                    if (threadCount.length() > 0)
                        cmd.append(" -j ").append(threadCount);

                    if (run20(cmd.toString())) {
                        if (outputFile.exists()) {
                            runOnUiThread(
                                    () -> {
                                        imageView.setVisibility(View.VISIBLE);
                                        imageView.setImage(ImageSource.uri(dir + "/output.png"));
                                        logTextView.setText(getString(R.string.hr) + "\n" + logTextView.getText());
                                        if (keepScreen) {
                                            view.setKeepScreenOn(false);
                                        }
                                    }
                            );
                        } else {
                            runOnUiThread(
                                    () -> {
                                        imageView.setVisibility(View.VISIBLE);
                                        imageView.setImage(ImageSource.uri(dir + "/input.png"));
                                        logTextView.setText(getString(R.string.lr) + "\n" + logTextView.getText());
                                        if (keepScreen) {
                                            view.setKeepScreenOn(false);
                                        }
                                    }
                            );
                        }

                    }
                }).start();
            }
        });

        findViewById(R.id.btn_setting).setOnClickListener(view -> {
            Intent intent = new Intent(this, SettingActivity.class);
            this.startActivity(intent);

            overridePendingTransition(0, android.R.anim.slide_out_right);
//            overridePendingTransition(android.R.anim.fade_in, android.R.anim.slide_out_right);
        });

//        System.load(dir + "/libncnn.so");

        requirePremision();

        if (progress != null)
            progress.setTitle("");
        else
            initProcess = true;
    }

    private void requirePremision() {
        if (ContextCompat.checkSelfPermission(this,
                Manifest.permission.WRITE_EXTERNAL_STORAGE)
                != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this,
                    new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE},
                    MY_PERMISSIONS_REQUEST);

        } else {
            //权限已经被授予，在这里直接写要执行的相应方法即可
            File file = new File(galleryPath);
            if (file.isFile())
                file.delete();
            if (!file.exists())
                file.mkdirs();
        }
    }


    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        if (requestCode == MY_PERMISSIONS_REQUEST) {
            if (grantResults[0] == PackageManager.PERMISSION_GRANTED) {

            } else {
                // Permission Denied
                Toast.makeText(MainActivity.this, "Permission Denied", Toast.LENGTH_SHORT).show();
            }
        }
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {

        if (resultCode == RESULT_OK && null != data) {
            Uri url = data.getData();

            if (requestCode == SELECT_IMAGE && null != url) {
                InputStream in;

                try {
                    in = getContentResolver().openInputStream(url);
                    if (null != in)
                        saveImage(in);
                    else
                        Toast.makeText(this, "input == null", Toast.LENGTH_SHORT).show();
                } catch (Exception e) {
                    e.printStackTrace();
                    return;
                }
            }

        }
        super.onActivityResult(requestCode, resultCode, data);
    }

    // 在主进程执行命令但是不刷新UI，也不被打断
    public boolean run_command(@NonNull String command) {

        if (command.trim().length() < 1) {
            Log.d("run_command", "command=" + command + "; break");
            return false;
        }

        StringBuilder con = new StringBuilder();
        String result;

        try {
            Process process = Runtime.getRuntime().exec(command);
            BufferedReader br = new BufferedReader(new InputStreamReader(process.getInputStream()));
            while ((result = br.readLine()) != null) {
                con.append(result);
                con.append('\n');
            }

        } catch (IOException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();

            Log.d("run_command", "command=" + command + "; crash; result=" + con);
            return false;
        }

        Log.d("run_command", "command=" + command + "; finish; result=" + con);
        return true;
    }

    private String progressText = "";

    public synchronized boolean run20(@NonNull String cmd) {
        newTask = false;
        Log.i("run20", "cmd = " + cmd);
        final long timeStart = System.currentTimeMillis();

        if (cmd.startsWith("./realsr-ncnn") || cmd.startsWith("./srmd-ncnn") || cmd.startsWith("./realcugan-ncnn") || cmd.startsWith("./resize-ncnn")) {
            modelName = "Real-ESRGAN-anime";
            if (cmd.matches(".+\\s-m(\\s+)models-.+")) {
                modelName = cmd.replaceFirst(".+\\s-m(\\s+)models-([^\\s]+).*", "$2");
            }
            if (modelName.matches("(se|nose)")) {
                modelName = "Real-CUGAN-" + modelName;
            } else if (cmd.matches(".+\\s-m(\\s+)(bicubic|bilinear|nearest).*")) {
                modelName = cmd.replaceFirst(".+\\s-m(\\s+)(bicubic|bilinear|nearest).*", "Classical-$2");
            }

            runOnUiThread(() -> progress.setTitle(getResources().getString(R.string.busy)));

        } else
            modelName = "SR";
        final boolean run_ncnn = !modelName.equals("SR");

        // 对应process进程的3个流
        BufferedReader successResult;
        BufferedReader errorResult;
        DataOutputStream os;

        // 保存的执行结果
        StringBuilder result = new StringBuilder();

        try {
            process = Runtime.getRuntime().exec("sh");
        } catch (Exception e) {
            e.printStackTrace();
            return false;
        }

        successResult = new BufferedReader(new InputStreamReader(process.getInputStream()));
        errorResult = new BufferedReader(new InputStreamReader(process.getErrorStream()));
        os = new DataOutputStream(process.getOutputStream());

        try {
            // 写入要执行的命令
            os.flush();

            os.writeBytes("cd " + dir + "\n");
            os.flush();

//            if(run_ncnn){
//                os.writeBytes("rm output.png\n");
//                os.flush();
//            }

            Log.i("run20", "write cmd start");

            os.write(cmd.getBytes());
            os.writeBytes("\n");
            os.flush();

            Log.i("run20", "write cmd finish");

            os.writeBytes("exit\n");
            os.flush();
            os.close();

            String line;
            Log.i("run20", "process.getErrorStream() start");

            // 读取错误输出
            try {
                while ((line = errorResult.readLine()) != null) {

                    result.append(line).append("\n");
                    boolean p = run_ncnn && line.matches("\\d([0-9.]*)%");
                    progressText = line;

                    runOnUiThread(() -> {
                        logTextView.setText(result);
                        if (p)
                            progress.setTitle(progressText);
                    });

                    Log.d("run20 errorResult", line);
                }
            } catch (Exception e) {
                e.printStackTrace();
            } finally {
                try {
                    errorResult.close();
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }

            Log.i("run20", "process.getErrorStream() finish");

            try {
                while ((line = successResult.readLine()) != null) {

                    result.append(line).append("\n");

                    boolean p = run_ncnn && line.matches("\\d([0-9.]*)%");
                    progressText = line;

                    runOnUiThread(() -> {
                        logTextView.setText(result);
                        if (p)
                            progress.setTitle(progressText);
                    });

                    Log.d("run20 successResult", line);
                }
            } catch (Exception e) {
                e.printStackTrace();
            } finally {
                try {
                    successResult.close();
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
            Log.i("run20", "process.getSuccessStream() finish");

        } catch (Exception e) {
            e.printStackTrace();
            return false;
        }
        Log.d("run_20", "finish, process " + (process != null));

        try {
            Log.d("run_20", "finish, exitValue " + process.exitValue());
            if (process.exitValue() != 0)
                process.destroy();
        } catch (Exception e) {
            e.printStackTrace();
        }

        if (newTask || process == null) {
            runOnUiThread(() -> {
                logTextView.setText(result.append("\nbreak"));
                progress.setTitle("");
            });
            return false;
        }


        result.append("\nfinish, use ").append((float) (System.currentTimeMillis() - timeStart) / 1000).append(" second");

        runOnUiThread(() -> {
            if (run_ncnn)
                logTextView.setText(result.append(", ").append(modelName));
            else
                logTextView.setText(result);
            progress.setTitle(getResources().getString(R.string.done));
        });

        Log.i("run20", "finish");
        return true;
    }

    private void stopCommand() {
        if (process != null) {
            process.destroy();
            if (progress != null)
                progress.setTitle("");
        }
        newTask = true;
    }

    private boolean saveImage(@NonNull InputStream in) {

        Log.i("saveImage", "start ");
        File file = new File(dir + "/input.png");

        if (file.exists()) {
            file.delete();
        }
        try {
            file.createNewFile();
            OutputStream outStream = new FileOutputStream(file);

            byte[] buffer = new byte[4112];
            int read;
            while ((read = in.read(buffer)) != -1) {
                outStream.write(buffer, 0, read);
            }

            outStream.flush();
            outStream.close();

        } catch (IOException e) {
            e.printStackTrace();
            return false;
        }
        try {
            in.close();
        } catch (Exception e) {
            e.printStackTrace();
        }

        Log.i("saveImage", "decodeFile");

        Log.i("saveImage", "runOnUiThread");
        runOnUiThread(
                () -> {
                    imageView.setVisibility(View.VISIBLE);
                    imageView.setImage(ImageSource.uri(dir + "/input.png"));
                    logTextView.setText(getString(R.string.lr));
                }
        );

        Log.i("saveImage", "finish");
        return true;
    }
}


