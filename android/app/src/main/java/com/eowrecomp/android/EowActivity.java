package com.eowrecomp.android;

import android.content.ContentResolver;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;
import android.util.Log;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

// SDL's activity running libmain.so (the runtime: runtime/main.c is its SDL_main), plus what SDL doesn't do on
// Android: choosing the folder with the player's own game package and keys (the system's folder picker hands out a
// tree of documents, not a path). The package itself is never copied: the runtime reads it through a file descriptor
// (runtime/nxpkg.c, openDocument). After LASRecomp-Android-Plus's LasActivity (MIT).
public class EowActivity extends SDLActivity {
    private static final String TAG = "EoWRecomp";
    private static final int PICK_FOLDER = 4100;
    private static Uri sTree;  // the folder picked last (read permission kept across restarts)

    @Override
    protected String[] getLibraries() {
        return new String[] {"SDL3", "main"};
    }

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        String saved = getPreferences(MODE_PRIVATE).getString("tree", null);
        if (saved != null) sTree = Uri.parse(saved);
        immersive();
    }

    // SDL sets its fullscreen through the old system UI flags (Android 15+ ignores them): while in front, the bars are
    // checked every 3 s and hidden again (after a swipe that showed them too)
    private final Runnable mHideBars = new Runnable() {
        @Override public void run() {
            immersive();
            getWindow().getDecorView().postDelayed(this, 3000);
        }
    };

    @Override
    protected void onResume() {
        super.onResume();
        View d = getWindow().getDecorView();
        d.removeCallbacks(mHideBars);
        d.postDelayed(mHideBars, 1000);
    }

    @Override
    protected void onPause() {
        getWindow().getDecorView().removeCallbacks(mHideBars);
        super.onPause();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) immersive();
    }

    private void immersive() {
        WindowInsetsController c = getWindow().getInsetsController();
        if (c == null) return;
        c.hide(WindowInsets.Type.systemBars());
        c.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
    }

    // ---- called from native code (runtime/platform/android_jni.c)

    // Opens the system's folder picker; the choice comes back through nativeFolderPicked.
    public static void pickGameFolder() {
        final EowActivity a = (EowActivity) mSingleton;
        if (a == null) return;
        a.runOnUiThread(() -> {
            Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
            try {
                a.startActivityForResult(i, PICK_FOLDER);
            } catch (Exception e) {
                Log.e(TAG, "folder picker", e);
                nativeFolderPicked(null);
            }
        });
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        if (request != PICK_FOLDER) { super.onActivityResult(request, result, data); return; }
        Uri tree = result == RESULT_OK && data != null ? data.getData() : null;
        if (tree != null) {
            try {
                getContentResolver().takePersistableUriPermission(tree, Intent.FLAG_GRANT_READ_URI_PERMISSION);
            } catch (Exception e) {
                Log.w(TAG, "no persistable permission", e);
            }
            sTree = tree;
            getPreferences(MODE_PRIVATE).edit().putString("tree", tree.toString()).apply();
        }
        nativeFolderPicked(tree != null ? describe(tree) : null);
    }

    // The folder picked in an earlier session (its permission kept), or null
    public static String savedGameFolder() {
        return sTree != null ? describe(sTree) : null;
    }

    private static String describe(Uri tree) {
        String id = DocumentsContract.getTreeDocumentId(tree);  // e.g. "primary:Games/EoW"
        int colon = id.indexOf(':');
        return colon >= 0 ? id.substring(colon + 1) : id;
    }

    // The game package (.nsp / .xci, the biggest: the game rather than an update) and the keys in the picked folder or
    // a few levels below it. prod.keys (or keys.txt) and title.keys are copied into keysDir. Returns "uri:<the
    // package's document URI>", or "nofolder", "notfound", "nokeys", "error:<details>".
    public static String findGamePackage(String keysDir) {
        EowActivity a = (EowActivity) mSingleton;
        if (a == null || sTree == null) return "nofolder";
        ContentResolver cr = a.getContentResolver();
        try {
            Doc[] found = new Doc[3];  // package, prod.keys, title.keys
            find(cr, new Doc(DocumentsContract.getTreeDocumentId(sTree), describe(sTree), true, 0), 0, found);
            if (found[0] == null) return "notfound";
            if (found[1] == null) return "nokeys";
            File dir = new File(keysDir);
            if (!dir.isDirectory() && !dir.mkdirs()) return "error:mkdir " + dir;
            copy(cr, found[1], new File(dir, "prod.keys"));
            if (found[2] != null) copy(cr, found[2], new File(dir, "title.keys"));
            return "uri:" + DocumentsContract.buildDocumentUriUsingTree(sTree, found[0].id);
        } catch (Exception e) {
            Log.e(TAG, "find package", e);
            return "error:" + e.getMessage();
        }
    }

    // A document opened for reading: its file descriptor, handed over to native code (which closes it), or -1
    public static int openDocument(String uri) {
        EowActivity a = (EowActivity) mSingleton;
        if (a == null) return -1;
        try {
            ParcelFileDescriptor pfd = a.getContentResolver().openFileDescriptor(Uri.parse(uri), "r");
            return pfd != null ? pfd.detachFd() : -1;
        } catch (Exception e) {
            Log.e(TAG, "open " + uri, e);
            return -1;
        }
    }

    private static void copy(ContentResolver cr, Doc d, File out) throws Exception {
        try (InputStream in = cr.openInputStream(DocumentsContract.buildDocumentUriUsingTree(sTree, d.id));
             OutputStream os = new FileOutputStream(out)) {
            if (in == null) throw new Exception("can't read " + d.name);
            byte[] buf = new byte[1 << 16];
            for (int n; (n = in.read(buf)) > 0; ) os.write(buf, 0, n);
        }
    }

    private static final class Doc {
        final String id, name;
        final boolean dir;
        final long size;
        Doc(String id, String name, boolean dir, long size) { this.id = id; this.name = name; this.dir = dir; this.size = size; }
    }

    private static List<Doc> children(ContentResolver cr, Doc d) {
        List<Doc> out = new ArrayList<>();
        Uri u = DocumentsContract.buildChildDocumentsUriUsingTree(sTree, d.id);
        String[] cols = {Document.COLUMN_DOCUMENT_ID, Document.COLUMN_DISPLAY_NAME, Document.COLUMN_MIME_TYPE,
                         Document.COLUMN_SIZE};
        try (Cursor c = cr.query(u, cols, null, null, null)) {
            while (c != null && c.moveToNext()) {
                boolean dir = Document.MIME_TYPE_DIR.equals(c.getString(2));
                out.add(new Doc(c.getString(0), c.getString(1), dir, c.isNull(3) ? 0 : c.getLong(3)));
            }
        }
        return out;
    }

    // as runtime/ui/launcher.mm find_game
    private static void find(ContentResolver cr, Doc d, int depth, Doc[] found) {
        if (depth > 4) return;
        List<Doc> kids = children(cr, d);
        for (Doc k : kids) {
            String low = k.name.toLowerCase(Locale.ROOT);
            if (!k.dir && (low.endsWith(".nsp") || low.endsWith(".xci")) && (found[0] == null || k.size > found[0].size)) found[0] = k;
            if (!k.dir && (low.equals("prod.keys") || (low.equals("keys.txt") && found[1] == null))) found[1] = k;
            if (!k.dir && low.equals("title.keys")) found[2] = k;
        }
        for (Doc k : kids)
            if (k.dir && !k.name.startsWith(".")) find(cr, k, depth + 1, found);
    }

    // ---- native (runtime/platform/android_jni.c)
    public static native void nativeFolderPicked(String name);
}
