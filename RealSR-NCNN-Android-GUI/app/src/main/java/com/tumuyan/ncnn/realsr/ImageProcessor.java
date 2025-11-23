package com.tumuyan.ncnn.realsr;

import android.util.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;

/**
 * 图像处理器类，负责管理后台任务和进程执行。
 * 使用 ExecutorService 替代原始线程，提供更好的并发控制。
 */
public class ImageProcessor {
    private static final String TAG = "ImageProcessor";
    private final ExecutorService executorService;
    private Process currentProcess;
    private Future<?> currentTask;

    public interface ProcessCallback {
        void onProgress(String line);
        void onCompleted(String result, boolean success);
        void onError(String error);
    }

    public ImageProcessor() {
        // 使用单线程执行器，确保任务按顺序执行（如果需要并发可以改为 newFixedThreadPool）
        this.executorService = Executors.newSingleThreadExecutor();
    }

    public void executeCommand(String command, String workingDir, ProcessCallback callback) {
        // 取消当前正在运行的任务（如果有）
        cancelCurrentTask();

        currentTask = executorService.submit(() -> {
            runProcess(command, workingDir, callback);
        });
    }

    private void runProcess(String command, String workingDir, ProcessCallback callback) {
        StringBuilder resultBuilder = new StringBuilder();
        boolean success = false;

        try {
            Log.d(TAG, "Executing command: " + command);
            ProcessBuilder processBuilder = new ProcessBuilder("sh");
            processBuilder.directory(new File(workingDir));
            processBuilder.redirectErrorStream(true);
            
            synchronized (this) {
                currentProcess = processBuilder.start();
            }

            OutputStream os = currentProcess.getOutputStream();
            // 设置环境变量和权限
            String setupCmd = "cd " + workingDir + "; chmod 777 *ncnn; export LD_LIBRARY_PATH=" + workingDir + "\n";
            os.write(setupCmd.getBytes());
            os.write((command + "\n").getBytes());
            os.write("exit\n".getBytes());
            os.flush();
            os.close();

            BufferedReader reader = new BufferedReader(new InputStreamReader(currentProcess.getInputStream()));
            String line;
            while ((line = reader.readLine()) != null) {
                if (Thread.currentThread().isInterrupted()) {
                    throw new InterruptedException("Task interrupted");
                }
                // 过滤无用日志
                if (line.contains("unused DT entry")) continue;
                
                callback.onProgress(line);
                resultBuilder.append(line).append("\n");
            }
            
            int exitCode = currentProcess.waitFor();
            success = (exitCode == 0);
            Log.d(TAG, "Process finished with exit code: " + exitCode);

        } catch (InterruptedException e) {
            Log.w(TAG, "Process interrupted");
            callback.onError("Process interrupted");
        } catch (Exception e) {
            Log.e(TAG, "Error executing process", e);
            callback.onError(e.getMessage());
        } finally {
            synchronized (this) {
                if (currentProcess != null) {
                    currentProcess.destroy();
                    currentProcess = null;
                }
            }
            callback.onCompleted(resultBuilder.toString(), success);
        }
    }

    public void cancelCurrentTask() {
        if (currentTask != null && !currentTask.isDone()) {
            currentTask.cancel(true);
        }
        synchronized (this) {
            if (currentProcess != null) {
                currentProcess.destroy();
                currentProcess = null;
            }
        }
    }

    public void shutdown() {
        executorService.shutdownNow();
    }
}
