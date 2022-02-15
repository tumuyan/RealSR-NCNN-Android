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
    private int style_type = 0;
    private SubsamplingScaleImageView imageView;
    private TextView logTextView;
    private boolean initProcess;
    private boolean newTast;
    private final String galleryPath = Environment.getExternalStorageDirectory()
            + File.separator + Environment.DIRECTORY_DCIM
            + File.separator + "RealSR" + File.separator;
    private String dir;
    // dir="/data/data/com.tumuyan.ncnn.realsr/cache/realsr";
    private String modelName = "SR";
    private SearchView searchView;
    private MenuItem progress;

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.main_menu, menu);
        progress = menu.findItem(R.id.progress);
        if (initProcess) {
            initProcess = false;
            progress.setTitle("");
            Log.i("onCreateOptionsMenu", "onCreate() done");
        }
        return true;
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        return super.onPrepareOptionsMenu(menu);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        imageView = findViewById(R.id.photo_view);
        logTextView = findViewById(R.id.tv_log);
        searchView = findViewById(R.id.serarch_view);

        requirePremision();

        SharedPreferences mySharePerferences = getSharedPreferences("config", Activity.MODE_PRIVATE);
        int version = mySharePerferences.getInt("version", 0);

        dir = this.getCacheDir().getAbsolutePath();
        AssetsCopyer.releaseAssets(getApplicationContext(),
                "realsr", dir
                , version == BuildConfig.VERSION_CODE
        );

        SharedPreferences.Editor editor = mySharePerferences.edit();
        editor.putInt("version", BuildConfig.VERSION_CODE);
        editor.apply();

        dir = dir + "/realsr";

        run_command("chmod 777 " + dir + " -R");
//        run_command("ls " + dir + " -l");

        Spinner spinner = findViewById(R.id.spinner);
        spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
                style_type = pos;
                Log.i("setOnItemSelectedListener", "select " + pos);
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
            }
        });


        searchView.setOnQueryTextListener(new SearchView.OnQueryTextListener() {
            @Override
            public boolean onQueryTextSubmit(String query) {
                newTast = true;
                progress.setTitle("");

                String q = searchView.getQuery().toString().trim();
                if (q.equals("help")) {
                    logTextView.setText(getString(R.string.default_log));
                } else {
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
            run20("cp output.png " + filePath);
            File file = new File(filePath);
            if (file.exists()) {
                Intent intent = new Intent(Intent.ACTION_MEDIA_SCANNER_SCAN_FILE);
                Uri uri = Uri.fromFile(file);
                intent.setData(uri);
                sendBroadcast(intent);
                Toast.makeText(getApplicationContext(), "Saved!", Toast.LENGTH_SHORT).show();
            } else {
                Toast.makeText(getApplicationContext(), "Fail!", Toast.LENGTH_SHORT).show();
            }
        });

        findViewById(R.id.btn_run).setOnClickListener(view -> {
            newTast = true;
            progress.setTitle("");
            {

                new Thread(() -> {
                    switch (style_type) {
                        case 1:
                            run20("./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGAN");
                            break;
                        case 2:
                            run20("./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGANv2-anime -s 2");
                            break;
                        case 3:
                            run20("./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGANv2-anime");
                            break;
                        case 4:
                            run20("./realsr-ncnn -i input.png -o output.png  -m models-DF2K_JPEG");
                            break;
                        case 5:
                            run20("./realsr-ncnn -i input.png -o output.png  -m models-DF2K");
                            break;
                        case 6:
                            run20("./srmd-ncnn -i input.png -o output.png  -m models-srmd -s 4");
                            break;
                        case 7:
                            run20("./srmd-ncnn -i input.png -o output.png  -m models-srmd -s 3");
                            break;
                        case 8:
                            run20("./srmd-ncnn -i input.png -o output.png  -m models-srmd -s 2");
                            break;
                        default:
                            run20("./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGAN-anime");
                    }
                    runOnUiThread(
                            () -> {
                                imageView.setImage(ImageSource.uri(dir + "/output.png"));
                            }
                    );

                }).start();

            }

        });

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
                file.mkdir();
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
    public synchronized boolean run_command(@NonNull String command) {

        if (command.trim().length() < 1) {
            Log.d("run_command", "command=" + command + "; break");
            return false;
        }

        StringBuilder con = new StringBuilder();
        String result;
        Process p;

        try {
            p = Runtime.getRuntime().exec(command);
            BufferedReader br = new BufferedReader(new InputStreamReader(p.getInputStream()));
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

    public synchronized String run20(@NonNull String cmd) {
        Log.i("run20", "cmd = " + cmd);

        if (cmd.startsWith("./realsr-ncnn") || cmd.startsWith("./rsmd-ncnn")) {
            modelName = "Real-ESRGAN-anime";
            if (cmd.matches(".+\\s-m(\\s+)models-.+")) {
                modelName = cmd.replaceFirst(".+\\s-m(\\s+)models-([^\\s]+).*", "$2");
            }
            runOnUiThread(() -> progress.setTitle(getResources().getString(R.string.busy)));

        } else
            modelName = "SR";
        final boolean run_ncnn = !modelName.equals("SR");
        newTast = false;
        // shell进程
        Process process;
        // 对应进程的3个流
        BufferedReader successResult;
        BufferedReader errorResult;
        DataOutputStream os;

        // 保存的执行结果
        StringBuilder result = new StringBuilder();

        try {
            process = Runtime.getRuntime().exec("sh");
        } catch (Exception e) {
            e.printStackTrace();
            return e.getMessage();
        }

        successResult = new BufferedReader(new InputStreamReader(process.getInputStream()));
        errorResult = new BufferedReader(new InputStreamReader(process.getErrorStream()));
        os = new DataOutputStream(process.getOutputStream());

        try {
            // 写入要执行的命令
            os.flush();

            os.writeBytes("cd " + dir + "\n");
            os.flush();

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
                    if (newTast) {
                        process.destroy();
                        result.append("break");
                        progress.setTitle("break");
                        return result.toString();
                    }
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
            }

            Log.i("run20", "process.getErrorStream() finish");

            try {
                while ((line = successResult.readLine()) != null) {

                    result.append(line).append("\n");
                    if (newTast) {
                        process.destroy();
                        result.append("break");
                        progress.setTitle("break");
                        return result.toString();
                    }

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
            return e.getMessage();
        }

        try {
            if (process.exitValue() != 0)
                process.destroy();

        } catch (Exception e) {
            e.printStackTrace();
        }

        runOnUiThread(() -> {
            logTextView.setText(result.append("\nfinish"));
            progress.setTitle(getResources().getString(R.string.done));
        });


        Log.i("run20", "finish");
        return result.toString();
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
                }
        );

        Log.i("saveImage", "finish");
        return true;
    }
}


