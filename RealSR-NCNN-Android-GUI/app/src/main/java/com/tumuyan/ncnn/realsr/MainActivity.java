package com.tumuyan.ncnn.realsr;

import androidx.annotation.WorkerThread;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.icu.text.SimpleDateFormat;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.provider.MediaStore;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ImageView;
import android.widget.SearchView;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import com.github.chrisbanes.photoview.PhotoView;

import java.io.BufferedReader;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.util.Date;
import java.util.Set;

public class MainActivity extends AppCompatActivity {
    private static final int SELECT_IMAGE = 1;
    private static final int MY_PERMISSIONS_REQUEST = 100;
    private int style_type = 0;
    private ImageView imageView;
    private Bitmap selectedImage = null;
    private Bitmap srImage = null;
    private TextView logTextView;
    private boolean newTast = false;
    private boolean TEST_PASS_BY = true;
    private String galleryPath= Environment.getExternalStorageDirectory()
            + File.separator + Environment.DIRECTORY_DCIM
            +File.separator+"RealSR4X"+File.separator;
    private String dir;
    // dir="/data/data/com.tumuyan.ncnn.realsr/cache/realsr";
    private SearchView searchView;


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        imageView = (PhotoView) findViewById(R.id.photo_view);
        logTextView = (TextView) findViewById(R.id.tv_log);

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
        editor.commit();

        dir = dir + "/realsr";

        run_command("chmod 777 " + dir + " -R");
        run_command("ls " + dir + " -l");

        Spinner spinner = (Spinner) findViewById(R.id.spinner);
        spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
                style_type = pos;
                Log.i("setOnItemSelectedListener","select "+pos);
            }
            @Override
            public void onNothingSelected(AdapterView<?> parent) {
            }
        });


        searchView.setOnQueryTextListener(new SearchView.OnQueryTextListener() {
            @Override
            public boolean onQueryTextSubmit(String query) {
                newTast = true;
                new Thread(new Runnable() {
                    @Override
                    public void run() {
                        run20(query);
                    }
                }).start();
                return false;
            }

            //用户输入字符时激发该方法
            @Override
            public boolean onQueryTextChange(String newText) {
                if (newText.trim().length()<2)
                    return true;
                if (imageView.getVisibility() == View.VISIBLE)
                    imageView.setVisibility(View.GONE);
                return true;
            }
        });
        findViewById(R.id.btn_open).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                Intent i = new Intent(Intent.ACTION_PICK);
                i.setType("image/*");
                startActivityForResult(i, SELECT_IMAGE);
            }
        });

        findViewById(R.id.btn_share).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {

                SimpleDateFormat f = new SimpleDateFormat("MMdd_HHmmss");
                String filePath = galleryPath + f.format(new Date()) + ".png";
                run20("cp output.png "+filePath);
                File file = new File(filePath);
                if(file.exists()){
                    Intent intent = new Intent(Intent.ACTION_MEDIA_SCANNER_SCAN_FILE);
                    Uri uri = Uri.fromFile(file);
                    intent.setData(uri);
                    sendBroadcast(intent);
                    Toast.makeText(getApplicationContext(), "Saved!", Toast.LENGTH_SHORT).show();
                }else{
                    Toast.makeText(getApplicationContext(), "Fail!", Toast.LENGTH_SHORT).show();
                }

/*                if (srImage != null) {
                    MediaStore.Images.Media.insertImage(getContentResolver(), srImage, "RealSR4X_" + f.format(new Date()), "" + style_type);
                    Toast.makeText(getApplicationContext(), "Saved!", Toast.LENGTH_SHORT).show();
                } else
                    Toast.makeText(getApplicationContext(), "No Result!", Toast.LENGTH_SHORT).show();
                */
            }
        });

        findViewById(R.id.btn_run).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                newTast = true;

                new Thread(new Runnable() {
                    @Override
                    public void run() {

                        switch (style_type) {
                            case 1:
                                run20("./realsr-ncnn -i input.png -o output.png  -m models-DF2K_ESRGAN");
                                break;
                            case 2:
                                run20("./realsr-ncnn -i input.png -o output.png  -m models-DF2K");
                                break;
                            default:
                                run20("./realsr-ncnn -i input.png -o output.png  -m models-DF2K_ESRGAN_anime");
                        }
                        runOnUiThread(
                                new Runnable() {
                                    @Override
                                    public void run() {
                                        srImage = BitmapFactory.decodeFile(dir + "/output.png");
                                        if(srImage!=null){
                                            imageView.setVisibility(View.VISIBLE);
//                                            int height = imageView.getWidth()*srImage.getHeight()/srImage.getWidth();
//                                            Log.i("setMaxHeight","width="+imageView.getWidth()+", maxHeight="+height);
//                                            imageView.setMaxHeight(height);
                                            imageView.setImageBitmap(srImage);
                                        }

                                    }
                                }
                        );

                    }
                }).start();
            }
        });
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
            if(file.isFile())
                file.delete();
            if(!file.exists())
                file.mkdir();
        }
    }


    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
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
        super.onActivityResult(requestCode, resultCode, data);

        if (resultCode == RESULT_OK && null != data) {
            Uri uir = data.getData();

            if (requestCode == SELECT_IMAGE) {
                InputStream in;

                try {
                    in = getContentResolver().openInputStream(uir);
                    saveImage(in);
/*                    new Thread(new Runnable() {
                        @Override
                        public void run() {
                            saveImage(in);
                        }
                    }).start();*/
                } catch (Exception e) {
                    e.printStackTrace();
                    return;
                }
            }

        }
    }

    public synchronized boolean run_command(String cmd) {
        //要执行的命令行
        String ret = cmd;
        if (null == cmd)
            ret = "." +
                    dir + "/realsr-ncnn -i "
                    + dir + "/input.png -o "
                    + dir + "/output.png  -m "
                    + dir + "/models-DF2K_ESRGAN_anime"
                    ;
        StringBuffer con = new StringBuffer();
        String result = "";
        Process p;

        srImage = null;
        newTast = false;

        try {
            p = Runtime.getRuntime().exec(ret);
            BufferedReader br = new BufferedReader(new InputStreamReader(p.getInputStream()));
            while ((result = br.readLine()) != null) {

                con.append(result);
                con.append('\n');

                if (newTast) {
                    con.append("break");
                    Log.d("run_command","command="+cmd + "; break; result="+con);
                    p.destroy();
                    return false;
                }
            }

        } catch (IOException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();

            Log.d("run_command","command="+cmd + "; crash; result="+con);
            return false;
        }

        Log.d("run_command","command="+cmd + "; finish; result="+con);
        return true;
    }

    public synchronized String run20(String cmd) {
        Log.i("run20","cmd = "+cmd);
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

            Log.i("run20","write cmd start");

            os.write(cmd.getBytes());
            os.writeBytes("\n");
            os.flush();

            Log.i("run20","write cmd finish");

            os.writeBytes("exit\n");
            os.flush();
            os.close();

            String line;
            Log.i("run20","process.getErrorStream() start");

            // 读取错误输出
            try {
                while ((line = errorResult.readLine()) != null) {
                    result.append(line).append("\n");
                    if (newTast) {
                        process.destroy();
                        result.append("break");
                        return result.toString();
                    }
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            logTextView.setText(result);
                        }
                    });

                    Log.d("run20 errorResult",line);
                }
            } catch (Exception e) {
                e.printStackTrace();
            }

            Log.i("run20","process.getErrorStream() finish");

            try {
                while ((line = successResult.readLine()) != null) {

                    result.append(line).append("\n");
                    if (newTast) {
                        process.destroy();
                        result.append("break");
                        return result.toString();
                    }
                        runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                logTextView.setText(result);
                            }
                        });

                        Log.d("run20 successResult",line);
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
            Log.i("run20","process.getSuccessStream() finish");

        } catch (Exception e) {
            e.printStackTrace();
            return e.getMessage();
        }

        try {
            if (process != null) {
                if (process.exitValue() != 0)
                    process.destroy();
            }

        } catch (Exception e) {
        }

        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                logTextView.setText(result.append("\nfinish"));
            }
        });

        Log.i("run20","finish");
        return result.toString();
    }


    private boolean saveImage(InputStream in) {

        Log.i("saveImage","start "+ (in==null));
        File file = new File(dir + "/input.png");

        if (file.exists()) {
            file.delete();
        }
        try {
            file.createNewFile();
            OutputStream outStream = new FileOutputStream(file);

/*
            if (in == null)
                selectedImage.compress(Bitmap.CompressFormat.PNG, 100, outStream);
            else {
                int temp = -1;
                while ((temp = in.read()) != -1) {
                    outStream.write(temp);
                }
            }
*/

            byte[] buffer = new byte[4112];
            int read;
            while((read = in.read(buffer)) != -1)
            {
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

        Log.i("saveImage","decodeFile");
        selectedImage = BitmapFactory.decodeFile(file.getAbsolutePath());

        Log.i("saveImage","runOnUiThread");
        runOnUiThread(
                new Runnable() {
                    @Override
                    public void run() {
                        srImage = null;
                        if(selectedImage!=null){
                            imageView.setVisibility(View.VISIBLE);
//                            int height = imageView.getWidth()*selectedImage.getHeight()/selectedImage.getWidth();
//                            Log.i("setMaxHeight","width="+imageView.getWidth()+", maxHeight="+height);
//                            imageView.setMaxHeight(height);
                            imageView.setImageBitmap(selectedImage);
                        }
                    }
                }
        );

        Log.i("saveImage","finish");
        return true;
    }
}


