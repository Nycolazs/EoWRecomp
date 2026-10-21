package com.eowrecomp.android;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.IBinder;
import android.os.PowerManager;

// While the game code is built on the phone (runtime/gamebuild.c, started by the start screen: runtime/ui/launcher.mm),
// the app is a foreground service: Android keeps it running with the screen off or another app in front, a partial wake
// lock keeps the CPU awake, and a notification shows the progress. The build itself runs in the app's own threads and
// child processes; this only keeps the process alive. Ended when the build ends (or fails). (From LASRecomp-Android-Plus,
// MIT.)
public class BuildService extends Service {
    private static final String CHANNEL = "build";
    private static final int ID = 4200;
    private static BuildService sRunning;
    private static String sTitle = "", sText = "";
    private static int sPercent = -1;
    private PowerManager.WakeLock mLock;

    static void start(Context c, String title) {
        sTitle = title;
        sText = "";
        sPercent = -1;
        Intent i = new Intent(c, BuildService.class);
        try {
            c.startForegroundService(i);
        } catch (Exception e) {  // (started from the background on Android 12+: the build goes on without it)
            android.util.Log.w("EoWRecomp", "build service", e);
        }
    }

    static void stop(Context c) {
        c.stopService(new Intent(c, BuildService.class));
    }

    static void progress(int percent, String text) {
        sPercent = percent;
        sText = text;
        BuildService s = sRunning;
        if (s != null) s.notifyProgress();
    }

    @Override
    public void onCreate() {
        super.onCreate();
        NotificationManager nm = getSystemService(NotificationManager.class);
        NotificationChannel ch = new NotificationChannel(CHANNEL, getString(R.string.build_channel),
                                                         NotificationManager.IMPORTANCE_LOW);
        ch.setShowBadge(false);
        nm.createNotificationChannel(ch);
        PowerManager pm = getSystemService(PowerManager.class);
        mLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "EoWRecomp:gamebuild");
        mLock.setReferenceCounted(false);
        mLock.acquire(6 * 60 * 60 * 1000L);  // (a ceiling: released at the end of the build)
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (Build.VERSION.SDK_INT >= 29)
            startForeground(ID, build(), ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC);
        else
            startForeground(ID, build());
        sRunning = this;
        return START_NOT_STICKY;  // (the build lives in the app's process: nothing to restart without it)
    }

    @Override
    public void onDestroy() {
        sRunning = null;
        if (mLock != null && mLock.isHeld()) mLock.release();
        stopForeground(STOP_FOREGROUND_REMOVE);
        super.onDestroy();
    }

    @Override
    public void onTimeout(int startId, int fgsType) {  // (Android 15: dataSync's daily limit - the build goes on)
        stopSelf();
    }

    @Override
    public IBinder onBind(Intent intent) { return null; }

    private Notification build() {
        Intent open = new Intent(this, EowActivity.class).addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
        PendingIntent pi = PendingIntent.getActivity(this, 0, open, PendingIntent.FLAG_IMMUTABLE);
        Notification.Builder b = new Notification.Builder(this, CHANNEL)
            .setSmallIcon(android.R.drawable.stat_sys_download)
            .setContentTitle(sTitle.isEmpty() ? getString(R.string.build_title) : sTitle)
            .setContentText(sText)
            .setOnlyAlertOnce(true)
            .setOngoing(true)
            .setContentIntent(pi)
            .setCategory(Notification.CATEGORY_PROGRESS);
        if (sPercent >= 0) b.setProgress(100, sPercent, false);
        else b.setProgress(0, 0, true);
        if (Build.VERSION.SDK_INT >= 31) b.setForegroundServiceBehavior(Notification.FOREGROUND_SERVICE_IMMEDIATE);
        return b.build();
    }

    private void notifyProgress() {
        getSystemService(NotificationManager.class).notify(ID, build());
    }
}
