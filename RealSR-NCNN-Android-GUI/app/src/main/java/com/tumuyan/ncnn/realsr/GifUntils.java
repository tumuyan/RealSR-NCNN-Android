package com.tumuyan.ncnn.realsr;


import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;

public class GifUntils {

    public static int getGifFrameCount(String filePath) {
        try (FileInputStream fis = new FileInputStream(new File(filePath));
             BufferedInputStream bis = new BufferedInputStream(fis)) {
            // 检查GIF文件头
            byte[] header = new byte[6];
            if (bis.read(header) != header.length || !isGifHeader(header)) {
                return -1;
            }

            // 读取GIF文件的逻辑屏幕描述符
            byte[] lsd = new byte[7];
            if (bis.read(lsd) != lsd.length) {
                return -1;
            }

            // 获取帧数
            int width = getLowByte(lsd[0]) | (getHighByte(lsd[0]) << 8);
            int height = getLowByte(lsd[1]) | (getHighByte(lsd[1]) << 8);
            int flags = lsd[3];
            boolean hasGlobalColorTable = (flags & 0x80) != 0;
            int globalColorTableSize = hasGlobalColorTable ? (int) Math.pow(2, (flags & 0x07) + 1) : 0;

            // 跳过全局颜色表
            bis.skip(globalColorTableSize * 3);

            int frameCount = 0;
            while (true) {
                int block = bis.read();
                if (block == 0x2C) { // 图像数据块
                    frameCount++;
                    // 解析图像数据块
                    byte[] imageDescriptor = new byte[9];
                    if (bis.read(imageDescriptor) != imageDescriptor.length) {
                        return frameCount;
                    }
                    int localColorTableSize = (imageDescriptor[3] & 0x07) + 1;
                    bis.skip(localColorTableSize * 3); // 跳过局部颜色表
                    skipSubBlocks(bis);
                } else if (block == 0x21) { // 扩展块
                    int extensionLabel = bis.read();
                    if (extensionLabel == 0xF9) { // 图形控制扩展块
                        bis.skip(4); // 跳过图形控制扩展块
                        skipSubBlocks(bis);
                    } else {
                        skipSubBlocks(bis);
                    }
                } else if (block == 0x3B) { // 文件尾
                    return frameCount;
                } else {
                    return -1; // 未知块类型
                }
            }
        } catch (IOException e) {
            e.printStackTrace();
            return -1;
        }
    }

    public static boolean isGifAnimation(String filePath) {
        try (FileInputStream fis = new FileInputStream(new File(filePath));
             BufferedInputStream bis = new BufferedInputStream(fis)) {
            // 检查GIF文件头
            byte[] header = new byte[6];
            if (bis.read(header) != header.length || !isGifHeader(header)) {
                return false;
            }

            // 读取GIF文件的逻辑屏幕描述符
            byte[] lsd = new byte[7];
            if (bis.read(lsd) != lsd.length) {
                return false;
            }

            // 获取帧数
            int width = getLowByte(lsd[0]) | (getHighByte(lsd[0]) << 8);
            int height = getLowByte(lsd[1]) | (getHighByte(lsd[1]) << 8);
            int flags = lsd[3];
            boolean hasGlobalColorTable = (flags & 0x80) != 0;
            int globalColorTableSize = hasGlobalColorTable ? (int) Math.pow(2, (flags & 0x07) + 1) : 0;

            // 跳过全局颜色表
            bis.skip(globalColorTableSize * 3);

            int frameCount = 0;
            while (true) {
                int block = bis.read();
                if (block == 0x2C) { // 图像数据块
                    frameCount++;
                    if(frameCount>1)
                        return true;
                    // 解析图像数据块
                    byte[] imageDescriptor = new byte[9];
                    if (bis.read(imageDescriptor) != imageDescriptor.length) {
                        return frameCount>1;
                    }
                    int localColorTableSize = (imageDescriptor[3] & 0x07) + 1;
                    bis.skip(localColorTableSize * 3); // 跳过局部颜色表
                    skipSubBlocks(bis);
                } else if (block == 0x21) { // 扩展块
                    int extensionLabel = bis.read();
                    if (extensionLabel == 0xF9) { // 图形控制扩展块
                        bis.skip(4); // 跳过图形控制扩展块
                        skipSubBlocks(bis);
                    } else {
                        skipSubBlocks(bis);
                    }
                } else if (block == 0x3B) { // 文件尾
                    return frameCount>1;
                } else {
                    return false; // 未知块类型
                }
            }
        } catch (IOException e) {
            e.printStackTrace();
            return false;
        }
    }

    private static boolean isGifHeader(byte[] header) {
        return header[0] == 'G' && header[1] == 'I' && header[2] == 'F';
    }

    private static int getLowByte(byte b) {
        return b & 0xFF;
    }

    private static int getHighByte(byte b) {
        return b & 0xFF;
    }

    private static void skipSubBlocks(BufferedInputStream bis) throws IOException {
        int blockSize;
        do {
            blockSize = bis.read();
            bis.skip(blockSize);
        } while (blockSize > 0);
    }
}
