package com.tumuyan.ncnn.realsr;


import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.ParcelFileDescriptor;

import java.io.File;
import java.io.IOException;

public class PreprocessToPng {
// stb_image可以处理  JPG, PNG, TGA, BMP, PSD, HDR, PIC , GIF
//  但是实测发现gif需要使用不同的加载方式
// 为了简单，只处理png jpg webp bmp，其他格式全部预处理为png

    private static byte[] PNG = {(byte) 0X89, 0X50, 0X4E, 0X47, 0X0D, 0X0A, 0X1A, 0X0A};
    private static byte[] JPG = {(byte) 0XFF, (byte) 0XD8};
    private static byte[] WEBP = {0x52, 0x49, 0x46, 0x46};
    private static byte[] BMP = {0x42, 0x4D};
    private static byte[] HEIF = {0X00 ,0X00, 0X00, 0X18,0X66,0X74 ,0X79 ,0X70 ,0X68 ,0X65 ,0X69 ,0X63 ,0X00};
    private static byte[] GIF = {0x47,0x49,0x46,0x38};
    public static String[] suffix = {"png","heif"};

    public static boolean isHeif(int i){
       return  i ==1;
    }

    public static boolean isGIF(int i){
        return  i==2;
    }

    /**
     * 探测文件头部是否与预设文件类型匹配
     * @param filehead 文件头部
     * @return  返回值对应保存文件的后缀
     */
    public static int match(byte[] filehead) {
        // 文件太小无一定有问题，无需转换，由程序抛出错误即可
        if (filehead.length < 10)
            return -1;

        byte[] bytes = PNG;
        for (int i = 0; ; i++) {
            if (bytes[i] != filehead[i])
                break;
            if (i == bytes.length - 1)
                return -1;
        }

        bytes = JPG.clone();
        for (int i = 0; ; i++) {
            if (bytes[i] != filehead[i])
                break;
            if (i == bytes.length - 1)
                return -1;
        }

        bytes = WEBP.clone();
        for (int i = 0; ; i++) {
            if (bytes[i] != filehead[i])
                break;
            if (i == bytes.length - 1)
                return -1;
        }

        bytes = BMP.clone();
        for (int i = 0; ; i++) {
            if (bytes[i] != filehead[i])
                break;
            if (i == bytes.length - 1)
                return -1;
        }
        bytes = HEIF.clone();
        for (int i = 0; ; i++) {
            if (bytes[i] != filehead[i])
                break;
            if (i == bytes.length - 1)
                return 1;
        }

        bytes = GIF.clone();
        for (int i = 0; ; i++) {
            if (bytes[i] != filehead[i])
                break;
            if (i == bytes.length - 1)
                return 2;
        }


        return 0;
    }
/*

    "signs": [
            "0,89504E470D0A1A0A"
            ],
            "mime": "image/png"

            [

    {

        "signs": [
        "0,52494646"
],
        "mime": "image/webp"
    }
]

        "signs": [
        "0,FFD8",
        "0,FFD8",
        "0,FFD8",
        "0,FFD8"
        ],
        "mime": "image/jpeg"
}
[

        {

        "signs": [
        "0,424D"
        ],
        "mime": "image/bmp"
        }
        ]

"signs": [
        "0,474946383961"
        ],
        "mime": "image/gif"
        }
        ]*/
}
