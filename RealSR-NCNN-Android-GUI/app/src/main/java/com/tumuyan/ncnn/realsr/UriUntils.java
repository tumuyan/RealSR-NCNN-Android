package com.tumuyan.ncnn.realsr;

import android.content.ContentUris;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ProviderInfo;
import android.database.Cursor;
import android.net.Uri;
import android.os.Environment;
import android.provider.DocumentsContract;
import android.provider.MediaStore;
import android.provider.OpenableColumns;
import android.util.Log;

import androidx.core.content.FileProvider;

import java.io.File;
import java.lang.reflect.Method;
import java.util.List;

public class UriUntils {

    public static String getFileName(final Uri uri, Context context) {

        String path = getPathFromUri(uri, context);

        if (path == null) {
            String fileName;
            try {
                Cursor cursor = context.getContentResolver().query(uri, null, null, null, null);
                if(cursor !=null) {
                    int nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                    cursor.moveToFirst();
                    fileName = cursor.getString(nameIndex);
                    cursor.close();
                    return fileName;
                }

            } catch (Exception e) {
                e.printStackTrace();
            }
        } else {
            Log.e("path is real", "uri-" + uri + "-path-" + path);
            return new File(path).getName();
        }
        return null;
    }

    //------------------------强无敌并不是真的强无敌--增加了raw---------------------

    public static String getPathFromUri(final Uri uri, Context context) {
        if (uri == null) {
            return null;
        }

        if (DocumentsContract.isDocumentUri(context, uri)) {
            Log.w("documentUri", "" + uri);
            // 如果是Android 4.4之後的版本，而且屬於文件URI
            final String authority = uri.getAuthority();
            // 判斷Authority是否為本地端檔案所使用的
            if ("com.android.externalstorage.documents".equals(authority)) {
                // 外部儲存空間
                final String docId = DocumentsContract.getDocumentId(uri);
                final String[] divide = docId.split(":");
                final String type = divide[0];
                if ("primary".equals(type)) {
                    return Environment.getExternalStorageDirectory().getAbsolutePath().concat("/").concat(divide[1]);
                }
            } else if ("com.android.providers.downloads.documents".equals(authority)) {
                // 下載目錄
                final String docId = DocumentsContract.getDocumentId(uri);
                if (docId.startsWith("raw:")) {
                    return docId.replaceFirst("raw:", "");
                }
                final Uri downloadUri = ContentUris.withAppendedId(
                        Uri.parse("content://downloads/public_downloads")
                        , Long.parseLong(docId.replaceFirst("^(msf):", "")
                        ));
                return queryAbsolutePath(context, downloadUri);
            } else if ("com.android.providers.media.documents".equals(authority)) {
                // 圖片、影音檔案
                final String docId = DocumentsContract.getDocumentId(uri);
                final String[] divide = docId.split(":");
                final String type = divide[0];
                Uri mediaUri = null;
                if ("image".equals(type)) {
                    mediaUri = MediaStore.Images.Media.EXTERNAL_CONTENT_URI;
                } else if ("video".equals(type)) {
                    mediaUri = MediaStore.Video.Media.EXTERNAL_CONTENT_URI;
                } else if ("audio".equals(type)) {
                    mediaUri = MediaStore.Audio.Media.EXTERNAL_CONTENT_URI;

                } else {
                    return null;
                }
                mediaUri = ContentUris.withAppendedId(mediaUri, Long.parseLong(divide[1]));
                return queryAbsolutePath(context, mediaUri);
            }
        } else {

            final String scheme = uri.getScheme();
            final String Authority = uri.getAuthority();
            final String uripath = uri.getPath();

            // 如果是一般的URI
            String path = null;
            if ("content".equals(scheme)) {
                // 內容URI
                path = queryAbsolutePath(context, uri);
                if (path != null) {
                    //        } else if (!path.matches("^/storage/")) {
                } else {//通配content://********.provider/  不一定有效
                    if (Authority != null && uripath != null) {
                        if (Authority.length() > 1 && uripath.length() > 1) {
                            Log.w("fileProvider", "last.");
                            return getFPUriToPath(uri, context);
                        }
                    }
                }
            } else if ("file".equals(scheme)) {
                // 檔案URI
                path = uri.getPath();
            }
            return path;
        }
        return null;
    }


    public static String queryAbsolutePath(final Context context, final Uri uri) {
        final String[] projection = {MediaStore.MediaColumns.DATA};
        Cursor cursor = null;
        try {
            cursor = context.getContentResolver().query(uri, projection, null, null, null);
            if (cursor != null && cursor.moveToFirst()) {
                final int index = cursor.getColumnIndexOrThrow(MediaStore.MediaColumns.DATA);
                return cursor.getString(index);
            }
        } catch (final Exception ex) {
            ex.printStackTrace();
            if (cursor != null) {
                cursor.close();
            }
        }
        return null;
    }


//---------------从fileProvider获取------

    public static String getFPUriToPath(Uri uri, Context context) {
        try {
            List<PackageInfo> packs = context.getPackageManager().getInstalledPackages(PackageManager.GET_PROVIDERS);
            if (packs != null) {
                String fileProviderClassName = FileProvider.class.getName();
                for (PackageInfo pack : packs) {
                    ProviderInfo[] providers = pack.providers;
                    if (providers != null) {
                        for (ProviderInfo provider : providers) {
                            if (uri.getAuthority().equals(provider.authority)) {
                                if (provider.name.equalsIgnoreCase(fileProviderClassName)) {
                                    Log.w("uri-get provider", "" + uri);
                                    Class<FileProvider> fileProviderClass = FileProvider.class;
                                    try {
                                        Method getPathStrategy = fileProviderClass.getDeclaredMethod("getPathStrategy", Context.class, String.class);
                                        getPathStrategy.setAccessible(true);
                                        Object invoke = getPathStrategy.invoke(null, context, uri.getAuthority());
                                        if (invoke != null) {
                                            String PathStrategyStringClass = FileProvider.class.getName() + "$PathStrategy";
                                            Class<?> PathStrategy = Class.forName(PathStrategyStringClass);
                                            Method getFileForUri = PathStrategy.getDeclaredMethod("getFileForUri", Uri.class);
                                            getFileForUri.setAccessible(true);
                                            Object invoke1 = getFileForUri.invoke(invoke, uri);
                                            if (invoke1 instanceof File) {
                                                String filePath = ((File) invoke1).getAbsolutePath();
                                                // 避免如下错误: content://com.hzhu.m.fileprovider/haohaozhu/sharemore.png
                                                //  返回path:            /data/data/com.tumuyan.filemagic/files/haohaozhu/sharemore.png
                                                Log.w("get-uri-path", filePath);
                                                if (!filePath.contains("com.tumuyan.filemagic"))
                                                    //&& !filePath.contains(uri.getAuthority())
                                                    return filePath;
                                            }
                                        }
                                    } catch (Exception e) {
                                        e.printStackTrace();
                                    }
                                    break;
                                }
                                break;
                            }
                        }
                    }
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        return null;
    }

}
