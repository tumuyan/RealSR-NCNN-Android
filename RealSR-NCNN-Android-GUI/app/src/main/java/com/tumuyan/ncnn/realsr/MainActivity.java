package com.tumuyan.ncnn.realsr;


import static com.tumuyan.ncnn.realsr.UriUntils.getFileName;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.app.NotificationCompat;
import androidx.core.content.ContextCompat;
import androidx.core.content.FileProvider;

import android.Manifest;
import android.app.Activity;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.icu.text.SimpleDateFormat;
import android.media.MediaScannerConnection;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
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
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Date;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;


public class MainActivity extends AppCompatActivity {
    private static final int SELECT_IMAGE = 1;
    private static final int MY_PERMISSIONS_REQUEST = 100;

    private static String CMD_RESET_CACHE = "echo Cache has been reset.;ls";
    private int selectCommand = 0;
    private String threadCount = "";
    private SubsamplingScaleImageView imageView;
    private TextView logTextView;
    private boolean initProcess;
    private final String galleryPath =
            Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DCIM)
                    + File.separator + "RealSR";
    private File outputFile, outputGif, inputFile, titleFile;
    private String dir, cache_dir;
    // dir="/data/data/com.tumuyan.ncnn.realsr/cache/realsr";
    private String modelName = "SR";
    private SearchView searchView;
    private MenuItem menuProgress;
    private Spinner spinner;
    private Process process;
    private boolean newTask;
    private int format, name, name2, notify;
    private String BUSY, ERR, DONE;
    private String outputSavePath = "";
    private String inputFileName = "";

    private String[] formats;

    private String[] command = null;
    private String log = "";

    private final String[] bench_mark_commands = new String[]{
            "./realsr-ncnn -c 46 -i img/PM5544.jpeg -o input.png  -m models-Real-ESRGAN",
            "./realsr-ncnn -c 46 -i input.png -o output.png  -m models-Real-ESRGANv3-anime -s 4"
    };

    private final String[] command_0 = new String[]{
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGAN-anime",
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGAN",
            "./realsr-ncnn -i input.png -o output.png  -m models-RealeSR-general-v3 -s 4",
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGANv3-anime -s 2",
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGANv3-anime -s 3",
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGANv3-anime -s 4",
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGANv2-anime -s 2",
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGANv2-anime -s 4",
            "./realsr-ncnn -i input.png -o output.png  -m models-ESRGAN-Nomos8kSC -s 4",
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGAN-SourceBook -s 2",
            "./realcugan-ncnn -i input.png -o output.png  -m models-nose -s 2  -n 0",
            "./realcugan-ncnn -i input.png -o output.png  -m models-se -s 2  -n -1",
            "./realcugan-ncnn -i input.png -o output.png  -m models-se -s 2  -n 0",
            "./realcugan-ncnn -i input.png -o output.png  -m models-se -s 2  -n 1",
            "./realcugan-ncnn -i input.png -o output.png  -m models-se -s 2  -n 2",
            "./realcugan-ncnn -i input.png -o output.png  -m models-se -s 2  -n 3",
            "./realcugan-ncnn -i input.png -o output.png  -m models-se -s 4  -n -1",
            "./realcugan-ncnn -i input.png -o output.png  -m models-se -s 4  -n 0",
            "./realcugan-ncnn -i input.png -o output.png  -m models-se -s 4  -n 3",
            "./realcugan-ncnn -i input.png -o output.png  -m models-pro -s 2  -n -1",
            "./realcugan-ncnn -i input.png -o output.png  -m models-pro -s 2  -n 0",
            "./realcugan-ncnn -i input.png -o output.png  -m models-pro -s 2  -n 3",
            "./realcugan-ncnn -i input.png -o output.png  -m models-pro -s 3  -n -1",
            "./realcugan-ncnn -i input.png -o output.png  -m models-pro -s 3  -n 0",
            "./realcugan-ncnn -i input.png -o output.png  -m models-pro -s 3  -n 3",
            "./Anime4k -i input.png -o output.png -z 2 -A",
            "./Anime4k -i input.png -o output.png -z 2 -A -a -e 48",
            "./Anime4k -i input.png -o output.png -z 2 -A -b -r 48",
            "./Anime4k -i input.png -o output.png -z 2 -A -w",
            "./Anime4k -i input.png -o output.png -z 2 -A -w -H",
            "./Anime4k -i input.png -o output.png -z 4 -A ",
            "./Anime4k -i input.png -o output.png -z 4 -A -a -e 40",
            "./Anime4k -i input.png -o output.png -z 4 -A -b -r 40",
            "./Anime4k -i input.png -o output.png -z 4 -A -w",
            "./Anime4k -i input.png -o output.png -z 4 -A -w -H",
    };
    private int tileSize;
    private boolean useCPU;
    private boolean keepScreen;
    private boolean prePng;
    private boolean preFrame;
    private boolean autoSave;
    private boolean showSearchView;
    private String savePath = galleryPath;
    private static final int NOTIFY_ID = 1;
    private static final String CHANNEL_NAME_RESULT = "channel_result";
    private static final String CHANNEL_NAME_PROGRESS = "channel_progress";


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
        boolean bench_mark_mode = false;
        int v = item.getItemId();
        if (v == R.id.progress) {
            stopCommand();
            return false;
        } else if (v == R.id.menu_share) {
            if (inputIsGifAnimation)
                shareImage("output.gif");
            else
                shareImage("output.png");
            return false;
        } else if (v == R.id.menu_avir2) {
            q = "./resize-ncnn -i input.png -o output.png  -m avir -s 0.5";
        } else if (v == R.id.menu_nearest4) {
            q = "./resize-ncnn -i input.png -o output.png  -m nearest -s 4";
        } else if (v == R.id.menu_de_nearest) {
            q = "./resize-ncnn -i input.png -o output.png  -m de-nearest";
        } else if (v == R.id.menu_magick2) {
            q = "./magick input.png -resize 50% output.png";
        } else if (v == R.id.menu_magick3) {
            q = "./magick input.png -resize 33.33% output.png";
        } else if (v == R.id.menu_magick4) {
            q = "./magick input.png -resize 25% output.png";
        } else if (v == R.id.menu_out2in) {
            if (inputIsGifAnimation) {
                Toast.makeText(this, R.string.not_support_animation, Toast.LENGTH_SHORT);
                return false;
            } else {
                q = "cp output.png input.png";
                imageName = "/input.png";
            }
        } else if (v == R.id.menu_in) {
            q = "in";
        } else if (v == R.id.menu_out) {
            q = "out";
        } else if (v == R.id.menu_help) {
            q = "help";
        } else if (v == R.id.menu_reset_cache) {
            q = CMD_RESET_CACHE;
            imageName = "";
        } else if (v == R.id.menu_bench_mark) {
            String append_param = "";
            if (tileSize > 0)
                append_param = " -t " + tileSize;
            if (useCPU)
                append_param += (" -g -1");

            append_param += ";";
            q = "rm -rf *.png; ls *.png; " + bench_mark_commands[0] + append_param + bench_mark_commands[1] + append_param;

            imageName = "/img/realsr.png";
            bench_mark_mode = true;
            imageView.setVisibility(View.GONE);
            if (keepScreen) {
                logTextView.setKeepScreenOn(true);
            }
        } else
            q = "";

        if (!run_fake_command(q)) {
            stopCommand();
            String finalImageName = imageName;
            boolean final_bench_mark_mode = bench_mark_mode;
            new Thread(() -> {
                if (q == CMD_RESET_CACHE) {
                    AssetsCopyer.releaseAssets(this, "realsr", cache_dir, false);
                }
                run20(q, final_bench_mark_mode, false);
                final File finalfile = new File(dir + finalImageName);
                if (finalfile.exists() && (!finalfile.isDirectory())) {
                    runOnUiThread(() -> {
                        imageView.setVisibility(View.VISIBLE);
                        imageView.setImage(ImageSource.uri(finalfile.getAbsolutePath()));
                        logTextView.setKeepScreenOn(false);
                    });
                } else {
                    runOnUiThread(() -> imageView.setVisibility(View.GONE));
                }
            }).start();
        }

        return super.onOptionsItemSelected(item);
    }

    // 删除文件或者目录
    public static void deleteFile(File f) {
        if (f.isDirectory()) {
            //获取目录下所有文件和目录
            File[] files = f.listFiles();
            for (File file : files) {
                if (file.isDirectory()) {
                    deleteFile(file);
                } else {
                    file.delete();
                }
            }
        }
        f.delete();
    }


    public void shareImage(String path) {
        Intent share_intent = new Intent();

        Uri contentUri = null;
        File file = null;
        if (!outputSavePath.isEmpty()) {
            file = new File(outputSavePath);
            if (file.exists()) {
                contentUri = FileProvider.getUriForFile(this,
                        BuildConfig.APPLICATION_ID + ".fileprovider",
                        file);
            }
        }

        if (contentUri == null) {
            file = new File(dir, path);
            if (file.exists()) {
                contentUri = FileProvider.getUriForFile(this,
                        BuildConfig.APPLICATION_ID + ".fileprovider",
                        file);
            }
        }

        if (contentUri != null) {
            String suffix = file.getName().replaceFirst(".+\\.([^.]+)$", "$1").toLowerCase(Locale.ROOT);
            switch (suffix) {
                case "png":
                    share_intent.setType("image/png");
                    break;
                case "jpg":
                    share_intent.setType("image/jpg");
                    break;
                case "webp":
                    share_intent.setType("image/webp");
                    break;
                case "heif":
                    share_intent.setType("image/heif");
                    break;
                case "gif":
                    share_intent.setType("image/gif");
                    break;
                default:
                    share_intent.setType("image/*");
                    break;
            }

            share_intent.setAction(Intent.ACTION_SEND);//设置分享行为
            share_intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            share_intent.putExtra(Intent.EXTRA_STREAM, contentUri);
            Log.i("shareImage()", "uri = " + contentUri);
            startActivity(Intent.createChooser(share_intent, "Share"));

        } else {
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
        ERR = getString(R.string.error);
        DONE = getString(R.string.done);

        SharedPreferences mySharePerferences = getSharedPreferences("config", Activity.MODE_PRIVATE);
        tileSize = mySharePerferences.getInt("tileSize", 0);
        threadCount = mySharePerferences.getString("threadCount", "");
        keepScreen = mySharePerferences.getBoolean("keepScreen", false);
        prePng = mySharePerferences.getBoolean("PrePng", true);
        preFrame = mySharePerferences.getBoolean("PreFrame", true);
        useCPU = mySharePerferences.getBoolean("useCPU", false);
        autoSave = mySharePerferences.getBoolean("autoSave", false);
        showSearchView = mySharePerferences.getBoolean("showSearchView", false);
        if (showSearchView)
            searchView.setVisibility(View.VISIBLE);
        else
            searchView.setVisibility(View.GONE);

        notify = mySharePerferences.getInt("notify", 0);

        format = mySharePerferences.getInt("format", 0);
        name = mySharePerferences.getInt("name", 0);
        name2 = mySharePerferences.getInt("name2", 0);
        List<String> extraCmd = getExtraCommands(
                mySharePerferences.getString("extraPath", "").trim()
                , mySharePerferences.getString("extraCommand", "").trim()
                , mySharePerferences.getString("classicalFilters", getString(R.string.default_classical_filters)).split("\\s+")
                , mySharePerferences.getString("magickFilters", getString(R.string.default_magick_filters)).split("\\s+")
        );

        if (extraCmd.size() > 0) {
            String[] presetCommand = getResources().getStringArray(R.array.style_array);
            extraCmd.addAll(0, Arrays.asList(presetCommand));
            ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_list_item_1, extraCmd);
            spinner.setAdapter(adapter);
        }

        spinner.setSelection(selectCommand);

        savePath = mySharePerferences.getString("savePath", "");
        if (savePath.isEmpty())
            savePath = galleryPath;
        try {
            File file = new File(savePath);
            if (file.isFile())
                file.delete();
            if (!file.exists())
                file.mkdirs();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }


    public boolean readFileFromShare() {
        Intent intent = getIntent();
        String action = intent.getAction();

        if (Intent.ACTION_SEND.equals(action)) {
            deleteFile(inputFile);
            Uri uri = intent.getParcelableExtra(Intent.EXTRA_STREAM);
            inputFileName = getFileName(uri, this).replaceFirst("\\.[^\\.]+$", "");
            Log.i("input file name", inputFileName);
            whiteFileFromUri(uri, "");

        } else if (Intent.ACTION_SEND_MULTIPLE.equals(action)) {
            ArrayList<Uri> imageUris = intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM);
            deleteFile(inputFile);
            inputFile.mkdirs();
            outputFile.delete();

            SimpleDateFormat f = new SimpleDateFormat("MMdd_HHmmss");
            String time = f.format(new Date());
            int k = 0;
            for (int i = 0; i < imageUris.size(); i++) {
                Uri uri = imageUris.get(i);
                inputFileName = getFileName(uri, this).replaceFirst("\\.[^\\.]+$", "");
//                if (inputFileName.isEmpty())
//                    inputFileName = "" + i;
                switch (name2) {
                    case 0:
                        inputFileName = String.format("%s_%s", inputFileName, time);
                        break;
                    case 1:
                        inputFileName = String.format("%s_%d", inputFileName, i);
                        break;
                    case 2:
                        inputFileName = String.format("%s_%d", time, i);
                        break;
                    case 3:
                        inputFileName = time + "_" + inputFileName;
                        break;
                }
                String inputFilePath = String.format("%s/input.png/%s.png", dir, inputFileName);
                int j = 0;
                while (new File(inputFilePath).exists()) {
                    j++;
                    inputFilePath = dir + "/input.png/" + inputFileName + "_" + j + ".png";
                }
                k++;
                whiteFileFromUri(uri, inputFilePath);
            }
            int inputFileSize = inputFile.listFiles().length;
            logTextView.setText(String.format(getString(R.string.input_file_size), inputFileSize));
        }
        return false;
    }

    private boolean whiteFileFromUri(Uri uri, String path) {
        if (uri != null) {
            try {
                InputStream in = getContentResolver().openInputStream(uri);
                if (null != in)
                    saveInputImage(in, path);
                else
                    Toast.makeText(this, R.string.share_is_null, Toast.LENGTH_SHORT).show();
                return true;
            } catch (IOException e) {
                e.printStackTrace();
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

    /**
     * 生成用户自定义命令. 自定义模型路径的命令与App预设命令有一样的外观和特性
     *
     * @param extraPath        自定义模型路径
     * @param extraCommand     用户预设命令
     * @param classicalFilters 经典插值算法列表（resize-ncnn）
     * @param magickFilters    Magick算法列表
     * @return
     */
    private List<String> getExtraCommands(String extraPath, String extraCommand, String[] classicalFilters, String[] magickFilters) {

        // 解析结果，包含模型目录、用户自定义命令（命令列表）
        List<String> cmdList = new ArrayList<>();

        // 解析模型目录的结果（下拉列表中的label）
        List<String> cmdLabel = new ArrayList<>();

        String[] classicalResize = {"2", "4", "10"};

        for (String f : classicalFilters) {
            for (String s : classicalResize) {
                cmdList.add("./resize-ncnn -i input.png -o output.png  -m " + f + " -s " + s);
                cmdLabel.add("Classical-" + f + "-x" + s);
            }
        }

        String[] magickResize = {"200%", "400%", "1000%"};

        for (String f : magickFilters) {
            for (String s : magickResize) {
                cmdList.add("./magick input.png -filter " + f + " -resize " + s + " output.png ");
                cmdLabel.add("Magick-" + f + "-x" + s.replaceFirst("(\\d+)00%", "$1"));
            }
        }

        if (!extraPath.isEmpty()) {
            File[] folders = new File(extraPath).listFiles();
            if (folders == null)
                Log.e("getExtraCommands", "extraPath folders is null");
            else {
                Arrays.sort(folders, Comparator.comparing(a -> ((File) a).getName()));
                for (File folder : folders) {
                    String name = folder.getName();
                    if (folder.isDirectory() && name.startsWith("models")) {
                        // 默认匹配realsr/real-esrgan模型目录
                        String model = name.replace("models-", "");
                        String scaleMatcher = ".*x(\\d+).*";
                        String noiseMatcher = "";
                        String command = "./realsr-ncnn -i input.png -o output.png  -m " + folder.getAbsolutePath() + " -s ";

                        if (name.matches("models-(cugan|cunet|upconv).*")) {
                            // 匹配waifu2x模型目录
                            model = name.replace("models-", "Waifu2x-");
                            scaleMatcher = ".*scale(\\d+).*";
                            command = "./waifu2x-ncnn -i input.png -o output.png  -m " + folder.getAbsolutePath() + " -s ";
                            noiseMatcher = "noise(\\d+).*";
                        } else if (name.matches("models-srmd.*")) {
                            // 匹配srmd模型目录
                            if (name.equals("models-srmd"))
                                model = "SRMD";
                            else
                                model = name.replace("models-srmd", "SRMD-");
                            command = "./srmd-ncnn -i input.png -o output.png  -m " + folder.getAbsolutePath() + " -s ";
                        } else if (name.startsWith("models-DF2K")) {
                            // 匹配realsr模型目录
                            model = name.replace("models-", "RealSR-");
                        }

                        List<String> suffix = genCmdFromModel(folder, scaleMatcher, noiseMatcher);
                        for (String s : suffix) {
                            cmdList.add(command + s);
                            cmdLabel.add(model + "-x" + s.replace(" -n ", "-noise"));
                        }
                    } else if (name.endsWith(".mnn")) {
                        String model = name.replace("models-", "").replace(".mnn", "");
                        String scaleMatcher = "([xX]\\d+|\\d+[xX])";
                        String m ="[-_.\s]+";
                        String[] fileTags = model.split(m);
                        String s = "4";
                        for (String tag : fileTags) {
                            if (tag.matches(scaleMatcher))
                                s = tag;
                        }
                        int scale = Integer.parseInt((s.replaceFirst("[xX]", "")));
                        if (scale < 1)
                            scale = 1;
                        String command = "./mnnsr-ncnn -i input.png -o output.png  -m " + folder.getAbsolutePath() + " -s " + scale;
                        cmdList.add(command);
                        cmdLabel.add("MNN-"+model.replaceFirst("(^"+s+m+"|"+m+s+"$)","") + "-x" + scale);
                    }
                }
            }
        }


        // 模型目录显示label
        int l = command_0.length;
        command = new String[cmdList.size() + l];

        System.arraycopy(command_0, 0, command, 0, l);
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
     * @return 模型名称的列表
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
        prePng = mySharePerferences.getBoolean("PrePng", true);
        preFrame = mySharePerferences.getBoolean("PreFrame", true);

        int version = mySharePerferences.getInt("version", 0);
        String defaultCommand = mySharePerferences.getString("defaultCommand", "");
        searchView.setQuery(defaultCommand, false);

        cache_dir = this.getCacheDir().getAbsolutePath();
        AssetsCopyer.releaseAssets(this, "realsr", cache_dir, version == BuildConfig.VERSION_CODE);

        SharedPreferences.Editor editor = mySharePerferences.edit();
        editor.putInt("version", BuildConfig.VERSION_CODE);
        editor.apply();


        int orientation = mySharePerferences.getInt("ORIENTATION", 0);
        if (orientation == 1) {
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR);
        } else if (orientation == 2)
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        else if (orientation == 3) {
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        }


        dir = cache_dir + "/realsr";

        outputFile = new File(dir, "output.png");
        outputGif = new File(dir, "output.gif");
        inputFile = new File(dir, "input.png");
        titleFile = new File(dir, "img/realsr.png");
        showImage(titleFile, getString(R.string.default_log));

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

        searchView.setOnQueryTextListener(new SearchView.OnQueryTextListener() {
            @Override
            public boolean onQueryTextSubmit(String query) {

                String q = searchView.getQuery().toString().trim();

                if (!run_fake_command(q)) {
                    stopCommand();
                    new Thread(() -> run20(query, false, true)).start();
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
                    File f = inputIsGifAnimation ? outputGif : outputFile;

                    if (!f.exists()) {
                        Toast.makeText(this, R.string.output_not_exits, Toast.LENGTH_SHORT).show();
                        return;
                    } else if (f.isDirectory()) {
                        File[] files = f.listFiles();
                        if (files.length < 1) {
                            Toast.makeText(this, R.string.output_not_exits, Toast.LENGTH_SHORT).show();
                        } else {
                            Toast.makeText(this, R.string.output_is_dir, Toast.LENGTH_SHORT).show();
                        }
                        return;
                    }
                    run_command(saveOutputCmd());
                    checkSaveOutput();
                }
        );

        findViewById(R.id.btn_run).setOnClickListener(view -> {
            menuProgress.setTitle("");
            {
                stopCommand();
                log = "";
                StringBuffer cmd;

                if (selectCommand >= command.length) {

                    cmd = new StringBuffer(spinner.getSelectedItem().toString());
                    Log.w("btn_run.onClick", "select=" + selectCommand + ", length=" + command.length + " text=" + cmd);

                    if (run_fake_command(cmd.toString()))
                        return;
                } else {
                    cmd = new StringBuffer(command[selectCommand]);

                    if (command[selectCommand].matches("./(realsr|srmd|waifu2x|realcugan|mnn)-ncnn.+")) {
                        if (tileSize > 0)
                            cmd.append(" -t ").append(tileSize);
                        if (threadCount.length() > 0)
                            cmd.append(" -j ").append(threadCount);
                        if (useCPU && !cmd.toString().startsWith("./srmd"))
                            cmd.append(" -g -1");
                    }
                }

                deleteFile(outputFile);
                if (inputIsGifAnimation) {
                    outputGif.delete();
                    outputFile.mkdir();
                }
                if (keepScreen) {
                    logTextView.setKeepScreenOn(true);
                }

                new Thread(() -> {

                    if (run20(cmd.toString(), false, true)) {
                        if (inputFile.isDirectory()) {
                            if (inputIsGifAnimation)
                                scanFiles(new String[]{outputSavePath});
                            else {
                                File[] files = inputFile.listFiles();

                                Log.i("befor scanFiles()", "inputFile size=" + files.length);
                                List<String> outputPaths = new ArrayList<>();
                                for (File file : files) {
                                    outputPaths.add(savePath + File.separator + file.getName());
                                }
                                scanFiles(outputPaths.toArray(new String[0]));
                            }
                        }

                        boolean showImgView = (cmd.toString().contains("output.png"));
                        if (showImgView) {
                            if (outputFile.exists() && outputFile.isFile()) {
                                updateImage(dir + "/output.png", String.format("%s\n%s", getString(R.string.hr), log), false);
                            } else if (inputIsGifAnimation && outputFile.exists() && outputFile.isDirectory() && outputFile.listFiles().length > 1) {
                                updateImage(outputFile.listFiles()[0].getPath(), String.format("%s\n%s", getString(R.string.hr), log), false);
                            } else {
                                updateImage(dir + "/input.png", String.format("%s\n%s", getString(R.string.lr), log), false);
                            }
                        }
                        if (!showImgView)
                            runOnUiThread(
                                    () -> imageView.setVisibility(View.GONE)
                            );
                    }
                }).start();
            }
        });

        findViewById(R.id.btn_setting).setOnClickListener(view -> {
            Intent intent = new Intent(this, SettingActivity.class);
            this.startActivity(intent);
            overridePendingTransition(0, android.R.anim.slide_out_right);
        });


        requirePremision();

        if (menuProgress != null) menuProgress.setTitle("");
        else initProcess = true;

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
            File file = new File(savePath);
            if (file.isFile())
                file.delete();
            if (!file.exists())
                file.mkdirs();
        }
    }


    private void sendNotification(Context mContext, String text) {
        if (notify == 0)
            return;
        NotificationManager notificationManager =
                (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);

        if (text == null) {
            notificationManager.cancel(NOTIFY_ID);   //取消Notification
            return;
        }

        String channel_name;
        if ((text.equals(ERR) || text.equals(DONE))) {
            channel_name = CHANNEL_NAME_RESULT;
        } else if (notify == 2) {
            return;
        } else {
            channel_name = CHANNEL_NAME_PROGRESS;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                    channel_name,
                    channel_name,
                    NotificationManager.IMPORTANCE_HIGH);
            notificationManager.createNotificationChannel(channel);
        }

        NotificationCompat.Builder mBuilder = new NotificationCompat.Builder(mContext, channel_name);
        mBuilder.setContentTitle(getString(R.string.app_name))                        //标题
                .setContentText(text)
                .setWhen(System.currentTimeMillis())
                .setSmallIcon(R.mipmap.ic_launcher)
                .setDefaults(Notification.DEFAULT_SOUND);
//                .setDefaults(Notification.FLAG_ONGOING_EVENT)
//                .setAutoCancel(true);                           //设置点击后取消Notification

        Notification notification = mBuilder.build();
        notificationManager.notify(NOTIFY_ID, notification);
    }


    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        if (requestCode == MY_PERMISSIONS_REQUEST) {
            if (grantResults[0] != PackageManager.PERMISSION_GRANTED) {
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
                deleteFile(inputFile);
                inputFileName = getFileName(url, this).replaceFirst("\\.[^\\.]+$", "");
                Log.i("input file name", inputFileName);
                InputStream in;

                try {
                    in = getContentResolver().openInputStream(url);
                    if (null != in)
                        saveInputImage(in, "");
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
    public int get_gif_frame_delay(@NonNull String path) {

        String[] cmd = new String[]{"/bin/sh", "-c", "cd " + dir + "; export LD_LIBRARY_PATH=" + dir + " ; " + "./magick  identify  -format  \"%T \" " + path + " "};

        StringBuilder con = new StringBuilder();
        String result;

        try {
            Process process = Runtime.getRuntime().exec(cmd);
            BufferedReader br = new BufferedReader(new InputStreamReader(process.getInputStream()));
            while ((result = br.readLine()) != null) {
                con.append(result);
                con.append('\n');
            }

        } catch (Exception e) {
            // TODO Auto-generated catch block
            e.printStackTrace();

            Log.d("get_gif_frame_delay()", "crash; result=" + con);
            return -1;
        }

        String[] data = con.toString().strip().split("\s");
        if (data.length < 2)
            return 0;

        int avg = Integer.parseInt(data[1]);
        int dif = 0;
        for (String s : data) {
            dif += (Integer.parseInt(s) - avg);
        }
        avg = avg + dif / data.length;

        Log.d("get_gif_frame_delay()", "finish; result=" + con);
        return avg;
    }

    // 在主进程执行命令但是不刷新UI，也不被打断
    public boolean run_command(@NonNull String command) {

        if (command.trim().length() < 1) {
            Log.d("run_command", "command=" + command + "; break");
            return false;
        }
        String[] cmd;
        if (command.startsWith("./magick")) {
            cmd = new String[]{"/bin/sh", "-c", "cd " + dir + "; export LD_LIBRARY_PATH=" + dir + " ; " + command};
        } else cmd = new String[]{"/bin/sh", "-c", command};

        StringBuilder con = new StringBuilder();
        String result;

        try {
            Process process = Runtime.getRuntime().exec(cmd);
            BufferedReader br = new BufferedReader(new InputStreamReader(process.getInputStream()));
            while ((result = br.readLine()) != null) {
                con.append(result);
                con.append('\n');
            }

        } catch (Exception e) {
            // TODO Auto-generated catch block
            e.printStackTrace();

            Log.d("run_command", "command=" + command + "; crash; result=" + con);
            return false;
        }

        Log.d("run_command", "command=" + command + "; finish; result=" + con);
        return true;
    }

    private String progressText = "";


    // 主要的运行命令的方式
    public synchronized boolean run20(@NonNull String cmd, boolean bench_mark_mode, boolean sr) {
        newTask = false;
        Log.i("run20", "cmd = " + cmd);
        final long timeStart = System.currentTimeMillis();
        boolean export_dir = false;

        if (cmd.startsWith("./realsr-ncnn")
                || cmd.startsWith("./mnn-ncnn")
                || cmd.startsWith("./srmd-ncnn")
                || cmd.startsWith("./realcugan-ncnn")
                || cmd.startsWith("./resize-ncnn")
                || cmd.startsWith("./waifu2x-ncnn")
                || cmd.startsWith("./magick input")
                || cmd.startsWith("./Anime4k")
        ) {
            if (cmd.contains(" input.png ") && cmd.contains(" output.png ")) {
                if (inputFile.isDirectory() && !inputIsGifAnimation) {
                    export_dir = true;
                    cmd = cmd.replace(" output.png ", " '" + savePath + "' ");
                }
            }

            runOnUiThread(() -> {
                menuProgress.setTitle(BUSY);
                sendNotification(this, BUSY);
            });
            modelName = "Real-ESRGAN-anime";
            if (cmd.matches(".+\\s-m(\\s+)[^\\s]*models-.+")) {
                modelName = cmd.replaceFirst(".+\\s-m(\\s+)[^\\s]*models-([^\\s]+).*", "$2");
            }
            if (cmd.startsWith("./Anime4k")) {
                modelName = "Anime4k";
                if (cmd.contains("-w"))
                    modelName += "-ACNet";
                if (cmd.contains("-H"))
                    modelName += "-HDN";
            } else if (modelName.matches("(se|nose|pro)")) {
                modelName = "Real-CUGAN-" + modelName;
            } else if (cmd.matches(".+\\s-m(\\s+)(bicubic|bilinear|nearest|avir|de-nearest).*")) {
                modelName = cmd.replaceFirst(".+\\s-m(\\s+)(bicubic|bilinear|nearest|lancir|avir|de-nearest).*", "Classical-$2");
            } else if (cmd.matches(".*waifu2x.+models-(cugan|cunet|upconv).*")) {
                modelName = cmd.replaceFirst(".*waifu2x.+models-(cugan|cunet|upconv_7_photo|upconv_7_anime).*", "Waifu2x-$1");
            } else if (cmd.startsWith("./magick input")) {
                if (cmd.contains("-filter"))
                    modelName = cmd.replaceFirst(".*-filter\\s+(\\w+).+", "Magick-$1");
                else
                    modelName = "Magick";
            }
        } else
            modelName = "SR";

        // 保存的执行结果
        StringBuilder result = new StringBuilder();
        HashSet<String> results = new HashSet<>();
        boolean result_fail = false;

        final boolean run_ncnn = bench_mark_mode || !modelName.equals("SR");
        boolean export_one_file = run_ncnn && (autoSave || (inputFile.isDirectory() && inputIsGifAnimation)) && cmd.contains("output.png");
        if (bench_mark_mode) {
            export_one_file = false;
            runOnUiThread(() -> {
                menuProgress.setTitle(BUSY);
                sendNotification(this, BUSY);
            });
        }
        // 根据设置把结果自动导出
        final boolean save = export_one_file;

        try {
            ProcessBuilder processBuilder = new ProcessBuilder("sh");
            processBuilder.redirectErrorStream(true); // 合并标准输出和标准错误
            process = processBuilder.start();
        } catch (Exception e) {
            e.printStackTrace();
            return false;
        }

        BufferedReader outputReader = new BufferedReader(new InputStreamReader(process.getInputStream()));
        OutputStream os = process.getOutputStream();


        try {
            // 写入要执行的命令
            os.flush();

            os.write(("cd " + dir + "; chmod 777 *ncnn; export LD_LIBRARY_PATH=" + dir + "\n").getBytes());
            os.flush();

            if (save) {
                String export_cmd = saveOutputCmd();
                if (inputIsGifAnimation)
                    cmd = cmd + ";./magick convert -delay " + inputGifDelay + " output.png/* -loop 0 '" + outputSavePath + "'";
                else
                    cmd = cmd + ";" + export_cmd;
            } else {
                outputSavePath = "";
            }

            Log.i("run20", "write cmd start; final cmd: " + cmd + " [end]");
            os.write((cmd + "\n").getBytes());
            os.flush();

            Log.i("run20", "write cmd finish");
            os.write("exit\n".getBytes());
            os.flush();
            os.close();

            String line;
            Log.i("run20", "process.getErrorStream() start");

            // 读取输出
            try {
                while ((line = outputReader.readLine()) != null) {
                    if (line.contains("unused DT entry"))
                        continue;

                    Log.d("run20 errorResult", line);

                    boolean p = run_ncnn && line.matches("\\s*\\d([0-9.]*)%(\\s.+)?");
                    progressText = line.trim().split("\\s")[0];

                    if (!p) {
                        if (line.equals("save result...") || line.equals("busy...") || line.equals("check result...")) {

                        } else if (result.toString().contains("vkQueueSubmit")) {
                            result_fail = true;
                        } else if (bench_mark_mode && results.contains(line)) {
                            line = "";
                        } else {
                            if (bench_mark_mode)
                                results.add(line);
                            result.append(line).append("\n");
                            line = "";
                        }
                    }

                    String finalLine = line;
                    runOnUiThread(() -> {
                        logTextView.setText(result + finalLine);
                        if (p) {
                            menuProgress.setTitle(progressText);
                            sendNotification(this, finalLine);
                        }
                    });


                }
            } catch (Exception e) {
                e.printStackTrace();
            } finally {
                try {
                    outputReader.close();
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }

            Log.i("run20", "process output stream finish");
        } catch (Exception e) {
            e.printStackTrace();
            sendNotification(this, ERR);
            return false;
        }
        Log.d("run_20", "finish, process " + (process != null));

        try {
            Log.d("run_20", "finish, exitValue " + process.exitValue());
            if (process.exitValue() != 0) process.destroy();
        } catch (Exception e) {
//            e.printStackTrace();
        }

        if (newTask || process == null) {
            log = result.append("\nbreak").toString();
            runOnUiThread(() -> {
                logTextView.setText(log);
                menuProgress.setTitle("");
            });

            return false;
        }

//TM
        if (result_fail)
            result.append("\nfail, use ").append((float) (System.currentTimeMillis() - timeStart) / 1000).append(" second");
        else
            result.append("\nfinish, use ").append((float) (System.currentTimeMillis() - timeStart) / 1000).append(" second");

        if (bench_mark_mode) {
            result.append(String.format(", Benchmark run on %s\n%s", DeviceInfo.getConfigStr(useCPU, tileSize), DeviceInfo.getInfo(this)));
            Log.i("run20 finish", "Benchmark, ..." + result.substring(Math.max(result.length() - 100, 0)));
        } else if (run_ncnn) {
            result.append(", ").append(modelName + "\n");
            Log.i("run20 finish'", "run_ncnn=" + run_ncnn + ", modelName=" + modelName + ", ..." + result.substring(Math.max(result.length() - 100, 0)));
        } else Log.i("run20 finish", "run_ncnn=false");


        boolean final_export_dir = export_dir; // toast save succeed
        log = result.toString();
        runOnUiThread(() -> {

            logTextView.setText(log);
            menuProgress.setTitle(DONE);
            sendNotification(this, DONE);

            if (save) {
                if (!outputFile.exists()) {
                    Toast.makeText(getApplicationContext(), R.string.output_not_exits, Toast.LENGTH_SHORT).show();
                } else {
                    checkSaveOutput();
                }
            } else if (final_export_dir) {
                Toast.makeText(getApplicationContext(), R.string.save_succeed, Toast.LENGTH_SHORT).show();
            }
        });


        Log.i("run20", "finish");
        return true;
    }

    private void stopCommand() {
        if (process != null) {
            process.destroy();
            if (menuProgress != null) menuProgress.setTitle("");
        }
        newTask = true;
    }

    private boolean inputIsGifAnimation;
    private int inputGifDelay;

    /**
     * 保存文件
     *
     * @param in   输出的文件流
     * @param path 输出的文件路径，路径为空时保存为input.png
     * @return 是否保存成功
     */
    private boolean saveInputImage(@NonNull InputStream in, String path) {

        Log.i("saveInputImage", "start ");
        inputIsGifAnimation = false;
        boolean inputOneImage = false;
        if (path.isEmpty()) {
            inputOneImage = true;
            path = dir + "/input.png";
        }
        File file = new File(path);

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
                        FileOutputStream out = new FileOutputStream(path);
                        bitmap.compress(Bitmap.CompressFormat.PNG, 100, out);
                        out.flush();
                        out.close();
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                } else if (preFrame && inputOneImage && PreprocessToPng.isGIF(match)) {
                    // 如果输入一个文件，且文件为多帧gif，则预处理为多个图片
                    inputGifDelay = get_gif_frame_delay(dir + "/tmp");
                    inputIsGifAnimation = inputGifDelay > 0;
                    Log.i("inputGifDelay", "" + inputGifDelay + ", " + inputIsGifAnimation);
                    if (inputIsGifAnimation) {
                        deleteFile(inputFile);
                        inputFile.mkdirs();
                        run_command("./magick tmp -coalesce -delay 0 input.png/%04d.png");
                    } else
                        run_command("./magick tmp '" + path + "'");
                } else {
                    run_command("./magick tmp '" + path + "'");
                }
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
        updateImage(dir + "/input.png", getString(R.string.lr), false);
        return true;
    }

    private void updateImage(final String path, String text, boolean keepScreen) {
        Log.i("saveInputImage", "runOnUiThread");
        File file = new File(path);
        runOnUiThread(() -> {
            if (file.exists()) {
                if (file.isDirectory()) {
                    if (file.listFiles().length > 0) {
                        imageView.setVisibility(View.VISIBLE);
                        imageView.setImage(ImageSource.uri(file.listFiles()[0].getPath()));
                        Log.i("saveInputImage", "finish, directory");
                    } else {
                        imageView.setVisibility(View.GONE);
                        Log.i("saveInputImage", "finish, empty directory");
                    }
                    logTextView.setText(text);
                } else {
                    imageView.setVisibility(View.VISIBLE);
                    imageView.setImage(ImageSource.uri(path));
                    logTextView.setText(text);
                    Log.i("saveInputImage", "finish, file");
                }

            } else Log.i("saveInputImage", "skip");
            if (keepScreen) {
                logTextView.setKeepScreenOn(false);
            }

        });
    }

    private boolean run_fake_command(String q) {
        if (q == null) return true;
        if (q.isEmpty()) return true;
        if (q.equals("help")) {
            showImage(titleFile, getString(R.string.default_log));
        } else if (q.equals("in")) {
            showImage(inputFile, getString(R.string.lr));
        } else if (q.equals("out")) {
            showImage(outputFile, getString(R.string.hr));
        } else if (q.startsWith("show ")) {
            String path = q.replaceFirst("\\s*show\\s+([^\\s]+)\\s*", "$1");
            File file = new File(path);
            if (!file.exists()) {
                path = dir + "/" + path;
                file = new File(path);
            }
            showImage(file, getString(R.string.show) + path);

        } else if (q.equals("none")) {
            showImage(null, getString(R.string.menu_reset_cache));
        } else if (q.equals(CMD_RESET_CACHE)) {
            showImage(null, getString(R.string.menu_reset_cache) + "...");
            return false;
        } else return false;
        return true;
    }

    private void showImage(File file, String info) {
        if (file == null) {
            imageView.setVisibility(View.GONE);
            logTextView.setText(info);
        } else if (file.exists() && (!file.isDirectory())) {
            imageView.setVisibility(View.VISIBLE);
            imageView.setImage(ImageSource.uri(file.getAbsolutePath()));
            logTextView.setText(info);
        } else if (file.isDirectory()) {
            imageView.setVisibility(View.GONE);
            File[] files = file.listFiles();
            if (files.length < 1) {
                logTextView.setText(getString(R.string.image_not_exists));
            } else logTextView.setText(getString(R.string.image_is_directory));
        } else {
            imageView.setVisibility(View.GONE);
            logTextView.setText(getString(R.string.image_not_exists));
        }
    }


    /**
     * 通知android媒体库更新文件
     */
    private void scanFiles(String[] filePath) {
        Log.i("scanFiles()", "length=" + filePath.length);
        try {
            MediaScannerConnection.scanFile(getApplicationContext(), filePath, null,
                    (path, uri) -> {
                        Log.i("TAG", "Scanned " + path + ":");
                        Log.i("TAG", "-> uri=" + uri);
                    });
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void checkSaveOutput() {
        File file = new File(outputSavePath);
        if (file.exists()) {
            Intent intent = new Intent(Intent.ACTION_MEDIA_SCANNER_SCAN_FILE);
            Uri uri = Uri.fromFile(file);
            intent.setData(uri);
            sendBroadcast(intent);
            Toast.makeText(getApplicationContext(), R.string.save_succeed, Toast.LENGTH_SHORT).show();
        } else {
            Toast.makeText(getApplicationContext(), R.string.save_fail, Toast.LENGTH_SHORT).show();
        }
    }

    //    生成输出的图片的命令
    private String saveOutputCmd() {

        SimpleDateFormat f = new SimpleDateFormat("MMdd_HHmmss");
        outputSavePath = savePath + File.separator;
        switch (name) {
            case 0:
                outputSavePath += modelName + "_" + f.format(new Date());
                break;
            case 1:
                outputSavePath += inputFileName + "_" + modelName + "_" + f.format(new Date());
                break;
            case 2:
                outputSavePath += inputFileName + "_" + modelName;
                break;
            case 3:
                outputSavePath += inputFileName + "_" + f.format(new Date());
                break;
            case 4:
                outputSavePath += inputFileName;
                break;
            default:
                outputSavePath += "output";
        }

        String cmd;
        if (inputIsGifAnimation) {
            outputSavePath += ".gif";
            cmd = ("cp " + dir + "/output.gif");
        } else if (format == 0) {
            outputSavePath += ".png";
            cmd = ("cp " + dir + "/output.png");
        } else {
            // 其他格式需要使用image magic进行转换，会额外消耗时间。但是为了方便，没有写到新线程上。
            // progress.setTitle(BUSY);
            if (format == 1) {
                outputSavePath += ".webp";
                cmd = ("./magick output.png");
            } else if (format == 2) {
                outputSavePath += ".gif";
                cmd = ("./magick output.png");
            } else if (format == 3) {
                outputSavePath += ".heic";
                cmd = ("./magick output.png");
            } else {
                outputSavePath += ".jpg";
                String q = formats[format].replaceAll("[a-zA-Z%\\s]+", "");
                if (q.length() > 0) {
                    cmd = ("./magick output.png -quality " + q);
                } else cmd = ("./magick output.png");
            }
        }

        return cmd + " '" + outputSavePath + "'";
    }

}


