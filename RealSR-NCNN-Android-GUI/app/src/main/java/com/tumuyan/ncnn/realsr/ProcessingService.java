package com.tumuyan.ncnn.realsr;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Binder;
import android.os.Build;
import android.os.IBinder;
import android.util.Log;

import androidx.core.app.NotificationCompat;

public class ProcessingService extends Service {
    private static final String TAG = "ProcessingService";
    private static final String CHANNEL_ID_PROGRESS = "channel_progress";
    private static final String CHANNEL_ID_ALIVE = "channel_service_alive";
    private static final int NOTIFICATION_ID = 1;
    public static final String ACTION_STOP_TASK = "com.tumuyan.ncnn.realsr.ACTION_STOP_TASK";

    private final IBinder binder = new LocalBinder();
    private ImageProcessor imageProcessor;
    private NotificationManager notificationManager;
    private int notifySetting = 2; // 0: Silent, 1: Result Only, 2: Detailed

    public class LocalBinder extends Binder {
        public ProcessingService getService() {
            return ProcessingService.this;
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        imageProcessor = new ImageProcessor();
        notificationManager = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        createNotificationChannels();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null && ACTION_STOP_TASK.equals(intent.getAction())) {
            Log.i(TAG, "Received stop action");
            if (imageProcessor != null) {
                imageProcessor.cancelCurrentTask();
            }
            stopForeground(true);
            stopSelf();
        }
        return START_NOT_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return binder;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (imageProcessor != null) {
            imageProcessor.shutdown();
        }
    }

    private void createNotificationChannels() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // Channel for detailed progress updates (notify=2)
            NotificationChannel progressChannel = new NotificationChannel(
                    CHANNEL_ID_PROGRESS,
                    getString(R.string.notification_channel_progress),
                    NotificationManager.IMPORTANCE_LOW); // Low to avoid sound on every update
            progressChannel.setDescription("Shows detailed progress of image processing tasks");

            // Channel for alive status (notify=0 or 1)
            NotificationChannel aliveChannel = new NotificationChannel(
                    CHANNEL_ID_ALIVE,
                    getString(R.string.notification_channel_alive),
                    NotificationManager.IMPORTANCE_LOW); // Low for silent running
            aliveChannel.setDescription("Keeps the service alive in background");

            notificationManager.createNotificationChannel(progressChannel);
            notificationManager.createNotificationChannel(aliveChannel);
        }
    }

    public void startTask(String command, String workingDir, int notifySetting,
            ImageProcessor.ProcessCallback callback) {
        this.notifySetting = notifySetting;
        // Initial notification
        startForeground(NOTIFICATION_ID, createNotification(getString(R.string.notification_processing)));

        imageProcessor.executeCommand(command, workingDir, new ImageProcessor.ProcessCallback() {
            @Override
            public void onProgress(String line) {
                updateNotification(line);
                if (callback != null) {
                    callback.onProgress(line);
                }
            }

            @Override
            public void onCompleted(String result, boolean success) {
                // If notifySetting is 0 (Silent) or 3 (Auto Dismiss), remove notification.
                // Otherwise (1 or 2), keep it so MainActivity can update it with result.
                boolean removeNotification = (notifySetting == 0 || notifySetting == 3);
                stopForeground(removeNotification);

                if (callback != null) {
                    callback.onCompleted(result, success);
                }
            }

            @Override
            public void onError(String error) {
                // Same logic for error
                boolean removeNotification = (notifySetting == 0 || notifySetting == 3);
                stopForeground(removeNotification);

                if (callback != null) {
                    callback.onError(error);
                }
            }
        });
    }

    public void cancelTask() {
        if (imageProcessor != null) {
            imageProcessor.cancelCurrentTask();
        }
    }

    private void updateNotification(String text) {
        // Only update notification if notifySetting is 2 (Detailed) or 3 (Detailed
        // AutoDismiss)
        if (notifySetting == 2 || notifySetting == 3) {
            notificationManager.notify(NOTIFICATION_ID, createNotification(text));
        }
    }

    private Notification createNotification(String text) {
        // Use PROGRESS channel for Detailed modes (2, 3), ALIVE for others (0, 1)
        String channelId = (notifySetting == 2 || notifySetting == 3) ? CHANNEL_ID_PROGRESS : CHANNEL_ID_ALIVE;
        String title = getString(R.string.app_name);

        // Intent to open MainActivity
        Intent intent = new Intent(this, MainActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
        PendingIntent pendingIntent = PendingIntent.getActivity(this, 0, intent,
                PendingIntent.FLAG_UPDATE_CURRENT
                        | (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M ? PendingIntent.FLAG_IMMUTABLE : 0));

        NotificationCompat.Builder builder = new NotificationCompat.Builder(this, channelId)
                .setContentTitle(title)
                .setContentText(text)
                .setSmallIcon(R.mipmap.ic_launcher)
                .setContentIntent(pendingIntent)
                .setOnlyAlertOnce(true)
                .setOngoing(true);

        return builder.build();
    }
}
