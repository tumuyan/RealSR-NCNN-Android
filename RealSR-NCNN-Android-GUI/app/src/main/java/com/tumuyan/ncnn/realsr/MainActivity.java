package com.tumuyan.ncnn.realsr;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.core.content.FileProvider;

import android.Manifest;
import android.app.Activity;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.icu.text.SimpleDateFormat;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.MediaStore;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.SearchView;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import com.davemorrissey.labs.subscaleview.ImageSource;
import com.davemorrissey.labs.subscaleview.SubsamplingScaleImageView;

import java.io.BufferedReader;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Date;
import java.util.List;
import java.util.Locale;

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
    private MenuItem menuProgress;
    private Spinner spinner;
    private Process process;
    private boolean newTask;
    private int format;
    private String BUSY;

    private String[] formats;

    private String[] command = null;
    private final String[] command_0 = new String[]{
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGAN-anime",
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGAN",
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGANv3-anime -s 2",
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGANv3-anime -s 3",
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGANv3-anime -s 4",
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGANv2-anime -s 2",
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGANv2-anime",
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
            "./resize-ncnn -i input.png -o output.png  -m nearest  -n -s 2",
            "./resize-ncnn -i input.png -o output.png  -m nearest  -n -s 4",
            "./resize-ncnn -i input.png -o output.png  -m bilinear -n -s 2",
            "./resize-ncnn -i input.png -o output.png  -m bilinear -n -s 4",
    };
    private int tileSize;
    private boolean useCPU;
    private boolean keepScreen;
    private boolean prePng;


    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.main_menu, menu);
        menuProgress = menu.findItem(R.id.progress);
        if (initProcess) {
            initProcess = false;
            menuProgress.setTitle("");
            Log.i("onCreateOptionsMenu", "onCreate() done");
        }
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(@NonNull MenuItem item) {

        final String q;
        String imageName = "/output.png";
        int v = item.getItemId();
        if (v == R.id.progress) {
            stopCommand();
            return false;
        } else if(v == R.id.menu_share){
            shareImage("output.png");
            return false;
        }
        else if (v == R.id.menu_avir2) {
            q = "./resize-ncnn -i input.png -o output.png  -m avir -s 0.5";
        } else if (v == R.id.menu_de_nearest) {
            q = "./resize-ncnn -i input.png -o output.png  -m de-nearest";
        } else if (v == R.id.menu_magick2) {
            q = "./magick input.png -resize 50% output.png";
        } else if (v == R.id.menu_magick3) {
            q = "./magick input.png -resize 33.33% output.png";
        } else if (v == R.id.menu_magick4) {
            q = "./magick input.png -resize 25% output.png";
        } else if (v == R.id.menu_out2in) {
            q = "cp output.png input.png";
            imageName = "/input.png";
        } else if (v == R.id.menu_in) {
            q = "in";
        } else if (v == R.id.menu_out) {
            q = "out";
        } else if (v == R.id.menu_help) {
            q = "help";
        } else
            q = "";

        if (!run_fake_command(q)) {
            stopCommand();
            String finalImageName = imageName;
            new Thread(
                    () -> {
                        run20(q);
                        final File finalfile = new File(dir + finalImageName);
                        if (finalfile.exists()) {
                            runOnUiThread(
                                    () -> {
                                        imageView.setVisibility(View.VISIBLE);
                                        imageView.setImage(ImageSource.uri(finalfile.getAbsolutePath()));
                                    }
                            );
                        } else {
                            runOnUiThread(
                                    () -> {
                                        imageView.setVisibility(View.GONE);
                                    }
                            );
                        }
                    }
            ).start();
        }

        return super.onOptionsItemSelected(item);
    }


    public void shareImage( String path) {
        Intent share_intent = new Intent();
        ArrayList<Uri> imageUris = new ArrayList<Uri>();

        File file =  new File(dir, path);
        if(file.exists()) {

            Uri contentUri = FileProvider.getUriForFile(this,
                     BuildConfig.APPLICATION_ID + ".fileprovider",
                    file);
            imageUris.add(contentUri);

            share_intent.setAction(Intent.ACTION_SEND_MULTIPLE);//设置分享行为
            share_intent.setType("image/png");//设置分享内容的类型
            share_intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            share_intent.putParcelableArrayListExtra(Intent.EXTRA_STREAM, imageUris);
            startActivity(Intent.createChooser(share_intent, "Share"));
        }else{
            Toast.makeText(getApplicationContext(), R.string.output_not_exits, Toast.LENGTH_SHORT).show();
        }
    }


    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        return super.onPrepareOptionsMenu(menu);
    }

    @Override
    public void onResume() {
        super.onResume();

        formats = getResources().getStringArray(R.array.format);
        BUSY = getResources().getString(R.string.busy);

        SharedPreferences mySharePerferences = getSharedPreferences("config", Activity.MODE_PRIVATE);
        tileSize = mySharePerferences.getInt("tileSize", 0);
        threadCount = mySharePerferences.getString("threadCount", "");
        keepScreen = mySharePerferences.getBoolean("keepScreen", false);
        prePng = mySharePerferences.getBoolean("PrePng", true);
        useCPU = mySharePerferences.getBoolean("useCPU", false);
        format = mySharePerferences.getInt("format", 0);

        List<String> extraCmd = getExtraCommands(
                mySharePerferences.getString("extraPath", "").trim()
                , mySharePerferences.getString("extraCommand", "").trim()
        );

        if (extraCmd.size() > 0) {
            String[] presetCommand = getResources().getStringArray(R.array.style_array);
            extraCmd.addAll(0, Arrays.asList(presetCommand));
            ArrayAdapter<String> adapter = new ArrayAdapter<String>(this, android.R.layout.simple_list_item_1, extraCmd);
            spinner.setAdapter(adapter);
        }

        spinner.setSelection(selectCommand);

    }


    public boolean readFileFromShare() {
        Intent intent = getIntent();
        String type = intent.getType();
        if (intent.getAction().equalsIgnoreCase(Intent.ACTION_SEND)) {
            Uri uri = intent.getParcelableExtra(Intent.EXTRA_STREAM);
            if (uri != null) {
                try {
                    InputStream in = getContentResolver().openInputStream(uri);
                    if (null != in)
                        saveImage(in);
                    else
                        Toast.makeText(this, R.string.share_is_null, Toast.LENGTH_SHORT).show();
                    return true;
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }
        return false;
    }

    /**
     * 生成用户自定义命令. 自定义模型路径的命令与App预设命令有一样的外观和特性
     *
     * @param extraPath    自定义模型路径
     * @param extraCommand 用户预设命令
     * @return 全部扩展命令
     */
    private List<String> getExtraCommands(String extraPath, String extraCommand) {

        // 解析结果，包含模型目录、用户自定义命令（命令列表）
        List<String> cmdList = new ArrayList<>();

        // 解析模型目录的结果（下拉列表中的label）
        List<String> cmdLabel = new ArrayList<>();

        // 增加resize-ncnn经典插值放大的命令
        String[] classicalFilters = {
                "bicubic",
                "avir",
                "avir-lancir",
        };

        String[] classicalResize = {
                "2", "4"
        };

        for (String f : classicalFilters) {
            for (String s : classicalResize) {
                cmdList.add("./resize-ncnn -i input.png -o output.png  -m " + f + " -s " + s);
                cmdLabel.add("Classical-" + f + "-x" + s);
            }
        }

        // 增加magick插值放大的命令
        String[] magickFilters = {
                "Hermite",
                "Hermite",
                "Hamming",
                "Lanczos",
                "LanczosRadius",
                "Lanczos2",
                "LanczosSharp",
                "Lanczos2Sharp",
                "Lagrange",
                "Mitchell",
                "Blackman",
        };

        String[] magickResize = {
                "200%", "400%", "1000%"
        };

        for (String f : magickFilters) {
            for (String s : magickResize) {
                cmdList.add("./magick input.png -filter " + f + " -resize " + s + " output.png");
                cmdLabel.add("Magick-" + f + "-x" + s.replaceFirst("(\\d+)00%", "$1"));
            }
        }


        if (!extraPath.isEmpty()) {
            File[] folders = new File(extraPath).listFiles();
            Arrays.sort(folders, new Comparator() {
                @Override
                public int compare(Object a, Object b) {
                    return ((File) a).getName().compareTo(((File) b).getName());
                }
            });
            for (File folder : folders) {
                String name = folder.getName();
                if (folder.isDirectory() && name.startsWith("models")) {

                    // 匹配realsr模型
                    String model = name.replace("models-", "");
                    String scaleMatcher = ".*x(\\d+).*";
                    String noiseMatcher = "";
                    String command = "./realsr-ncnn -i input.png -o output.png  -m " + folder.getAbsolutePath() + " -s ";

                    // 匹配waifu2x模型
                    if (name.matches("models-(cugan|cunet|upconv).*")) {
                        model = name.replace("models-", "Waifu2x-");
                        scaleMatcher = ".*scale(\\d+).*";
                        command = "./waifu2x-ncnn -i input.png -o output.png  -m " + folder.getAbsolutePath() + " -s ";
                        noiseMatcher = "noise(\\d+).*";
                    } else if (name.startsWith("models-DF2K")) {
                        model = name.replace("models-", "RealSR-");
                    }

                    List<String> suffix = genCmdFromModel(folder, scaleMatcher, noiseMatcher);
                    for (String s : suffix) {
                        cmdList.add(command + s);
                        cmdLabel.add(model + "-x" + s.replace(" -n ", "-noise"));
                    }
                }
            }
        }


        // 模型目录显示label
        int l = command_0.length;
        command = new String[cmdList.size() + l];

        for (int i = 0; i < l; i++)
            command[i] = command_0[i];
        for (int i = 0; i < cmdList.size(); i++)
            command[l + i] = cmdList.get(i);


        // 预设命令显示命令
        if (!extraCommand.isEmpty()) {
            String[] cmds = extraCommand.split("\n");
            cmdLabel.addAll(Arrays.asList(cmds));
        }

        return cmdLabel;
    }


    /**
     * 从用户自定义模型路径加载文件，自动列出可用命令
     *
     * @param folder       自定义模型目录
     * @param scaleMatcher 缩放倍率抓取规则
     * @param noiseMatcher 降噪系数抓取规则
     * @return
     */
    private static List<String> genCmdFromModel(File folder, String scaleMatcher, String noiseMatcher) {
        List<String> list = new ArrayList<>();
        File[] files = folder.listFiles();

        List<String> names = new ArrayList<>();
        for (File f : files) {
            String name = f.getName().toLowerCase(Locale.ROOT);
            if (name.endsWith("bin"))
                names.add(name);
        }

        String[] fileNames = names.toArray(new String[0]);

        Arrays.sort(fileNames);

        for (String name : fileNames) {
            // 只解析整数倍缩放
            String s;
            if (name.matches(scaleMatcher))
                s = (name.replaceFirst(scaleMatcher, "$1"));
            else
                s = "1";

            if (!noiseMatcher.isEmpty()) {
                String noise = name.replaceFirst(noiseMatcher, "$1");
                if (noise.matches("\\d+")) {
                    int n = Integer.parseInt(noise);
                    s = s + " -n " + n;
                }
            }
            if (!list.contains(s))
                list.add(s);
        }
        return list;
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

        selectCommand = mySharePerferences.getInt("selectCommand", 2);
//        spinner.setSelection(selectCommand);
//        Log.i("selectCommand ","getconfig r="+selectCommand);

        searchView.setOnQueryTextListener(new SearchView.OnQueryTextListener() {
            @Override
            public boolean onQueryTextSubmit(String query) {

                String q = searchView.getQuery().toString().trim();

                if (!run_fake_command(q)) {
                    stopCommand();
                    new Thread(() -> run20(query)).start();
                }
                return false;
            }

            //用户输入字符时激发该方法
            @Override
            public boolean onQueryTextChange(String newText) {
                if (newText.trim().length() < 2) {
                    if (menuProgress != null)
                        menuProgress.setTitle("");
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

            if (!outputFile.exists()) {
                Toast.makeText(getApplicationContext(), R.string.output_not_exits, Toast.LENGTH_SHORT).show();
                return;
            }

            SimpleDateFormat f = new SimpleDateFormat("MMdd_HHmmss");
            String filePath = null;
            if (format == 0) {
                filePath = galleryPath + modelName + "_" + f.format(new Date()) + ".png";
                run_command("cp " + dir + "/output.png " + filePath);
            } else {
                // 其他格式需要使用image magic进行转换，会额外消耗时间。但是为了方便，没有写到新线程上。
                // progress.setTitle(BUSY);
                if (format == 1) {
                    filePath = galleryPath + modelName + "_" + f.format(new Date()) + ".webp";
                    run20("./magick output.png " + filePath);
                } else if (format == 2) {
                    filePath = galleryPath + modelName + "_" + f.format(new Date()) + ".gif";
                    run20("./magick output.png " + filePath);
                } else if (format == 3) {
                    filePath = galleryPath + modelName + "_" + f.format(new Date()) + ".heic";
                    run20("./magick output.png " + filePath);
                } else {
                    filePath = galleryPath + modelName + "_" + f.format(new Date()) + ".jpg";
                    String q = formats[format].replaceAll("[a-zA-Z%\\s]+", "");
                    if (q.length() > 0) {
                        run20("./magick output.png -quality " + q + " " + filePath);
                    } else
                        run20("./magick output.png " + filePath);
                }
            }

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
            menuProgress.setTitle("");
            {
                stopCommand();
                StringBuffer cmd;

                if (selectCommand >= command.length) {

                    cmd = new StringBuffer(spinner.getSelectedItem().toString());
                    Log.w("btn_run.onClick", "select=" + selectCommand + ", length=" + command.length + " text=" + cmd);

                    if (run_fake_command(cmd.toString()))
                        return;
                } else {
                    cmd = new StringBuffer(command[selectCommand]);

                    if (command[selectCommand].matches("./(realsr|srmd|waifu2x|realcugan)-ncnn.+")) {
                        if (tileSize > 0)
                            cmd.append(" -t ").append(tileSize);
                        if (threadCount.length() > 0)
                            cmd.append(" -j ").append(threadCount);
                        if (useCPU && !cmd.toString().startsWith("./srmd"))
                            cmd.append(" -g -1");
                    }
                }

                outputFile.delete();
                if (keepScreen) {
                    view.setKeepScreenOn(true);
                }

                new Thread(() -> {

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

        if (menuProgress != null)
            menuProgress.setTitle("");
        else
            initProcess = true;


        readFileFromShare();
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

        if (cmd.startsWith("./realsr-ncnn")
                || cmd.startsWith("./srmd-ncnn")
                || cmd.startsWith("./realcugan-ncnn")
                || cmd.startsWith("./resize-ncnn")
                || cmd.startsWith("./waifu2x-ncnn")
                || cmd.startsWith("./magick input.png -")
        ) {
            runOnUiThread(() -> menuProgress.setTitle(BUSY));
            modelName = "Real-ESRGAN-anime";
            if (cmd.matches(".+\\s-m(\\s+)[^\\s]*models-.+")) {
                modelName = cmd.replaceFirst(".+\\s-m(\\s+)[^\\s]*models-([^\\s]+).*", "$2");
            }
            if (modelName.matches("(se|nose)")) {
                modelName = "Real-CUGAN-" + modelName;
            } else if (cmd.matches(".+\\s-m(\\s+)(bicubic|bilinear|nearest|avir|de-nearest).*")) {
                modelName = cmd.replaceFirst(".+\\s-m(\\s+)(bicubic|bilinear|nearest|lancir|avir|de-nearest).*", "Classical-$2");
            } else if (cmd.matches(".*waifu2x.+models-(cugan|cunet|upconv).*")) {
                modelName = cmd.replaceFirst(".*waifu2x.+models-(cugan|cunet|upconv_7_photo|upconv_7_anime).*", "Waifu2x-$1");
            } else if (cmd.startsWith("./magick input.png -")) {
                if (cmd.contains("-filter"))
                    modelName = cmd.replaceFirst(".*-filter\\s+(\\w+).+", "Magick-$1");
                else
                    modelName = "Magick";
            }

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

            if (cmd.startsWith("./magick")) {
                os.writeBytes("export LD_LIBRARY_PATH=" + dir + "\n");
                os.flush();
            }

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
                    if (line.contains("unused DT entry"))
                        continue;

                    result.append(line).append("\n");
                    boolean p = run_ncnn && line.matches("\\d([0-9.]*)%");
                    progressText = line;

                    runOnUiThread(() -> {
                        logTextView.setText(result);
                        if (p)
                            menuProgress.setTitle(progressText);
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
                    if (line.contains("unused DT entry"))
                        continue;

                    result.append(line).append("\n");

                    boolean p = run_ncnn && line.matches("\\d([0-9.]*)%");
                    progressText = line;

                    runOnUiThread(() -> {
                        logTextView.setText(result);
                        if (p)
                            menuProgress.setTitle(progressText);
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
                menuProgress.setTitle("");
            });
            return false;
        }


        result.append("\nfinish, use ").append((float) (System.currentTimeMillis() - timeStart) / 1000).append(" second");

        runOnUiThread(() -> {
            if (run_ncnn)
                logTextView.setText(result.append(", ").append(modelName));
            else
                logTextView.setText(result);
            menuProgress.setTitle(getResources().getString(R.string.done));
        });

        Log.i("run20", "finish");
        return true;
    }

    private void stopCommand() {
        if (process != null) {
            process.destroy();
            if (menuProgress != null)
                menuProgress.setTitle("");
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

            byte[] buffer = new byte[4112];
            int read;

            int match = -1;

            if ((read = in.read(buffer)) != -1) {
                if (prePng) {
                    match = PreprocessToPng.match(buffer);
                    if (match >= 0) {
                        file = new File(dir + "/tmp");
                        if (file.exists()) {
                            file.delete();
                        }
                    }
                }
            }

            file.createNewFile();
            OutputStream outStream = new FileOutputStream(file);
            outStream.write(buffer, 0, read);

            while ((read = in.read(buffer)) != -1) {
                outStream.write(buffer, 0, read);
            }

            outStream.flush();
            outStream.close();

            if (match >= 0) {
                if (PreprocessToPng.isHeif(match)) {
                    Bitmap bitmap = BitmapFactory.decodeFile(dir + "/tmp");

                    try {
                        FileOutputStream out = new FileOutputStream(dir + "/input.png");
                        bitmap.compress(Bitmap.CompressFormat.PNG, 100, out);
                        out.flush();
                        out.close();
                    } catch (FileNotFoundException e) {
                        e.printStackTrace();
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                } else
                    run20("./magick tmp input.png");
//                run_command("."+dir+"/magick " + dir + "/tmp "+dir + "/input.png");
            }

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

    private boolean run_fake_command(String q) {
        if (q == null)
            return true;
        if (q.isEmpty())
            return true;
        if (q.equals("help")) {
            logTextView.setText(getString(R.string.default_log));
        } else if (q.equals("in")) {
            File file = new File(dir + "/input.png");
            showImage(file, getString(R.string.lr));
        } else if (q.equals("out")) {
            File file = new File(dir + "/output.png");
            showImage(file, getString(R.string.hr));
        } else if (q.startsWith("show ")) {
            String path = q.replaceFirst("\\s*show\\s+([^\\s]+)\\s*", "$1");
            File file = new File(path);
            if (!file.exists()) {
                path = dir + "/" + path;
                file = new File(path);
            }
            showImage(file, getString(R.string.show) + path);

        } else
            return false;
        return true;
    }

    private void showImage(File file, String info) {
        if (file.exists()) {
            imageView.setVisibility(View.VISIBLE);
            imageView.setImage(ImageSource.uri(file.getAbsolutePath()));
            logTextView.setText(info);
        } else {
            imageView.setVisibility(View.GONE);
            logTextView.setText(getString(R.string.image_not_exists));
        }
    }
}


