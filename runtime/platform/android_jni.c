// The Java side of the Android app (android/app/src/main/java/com/eowrecomp/android/EowActivity.java): calls into it
// through SDL's JNI environment and activity, and its native methods. (After LASRecomp-Android-Plus, MIT.)
#include <SDL3/SDL.h>
#include <jni.h>
#include <stdio.h>
#include <string.h>

#include "android.h"

static void (*g_pick_cb)(const char *name);

// A static method of the activity's class (EowActivity: GetObjectClass works on any thread, FindClass of an app class
// only on Java's own)
static jmethodID activity_method(JNIEnv **env, jclass *cls, const char *name, const char *sig) {
    *env = (JNIEnv *)SDL_GetAndroidJNIEnv();
    jobject act = (jobject)SDL_GetAndroidActivity();
    if (!*env || !act) return NULL;
    *cls = (**env)->GetObjectClass(*env, act);
    (**env)->DeleteLocalRef(*env, act);
    jmethodID m = (**env)->GetStaticMethodID(*env, *cls, name, sig);
    if (!m) { (**env)->ExceptionClear(*env); fprintf(stderr, "[android] no method %s\n", name); }
    return m;
}

// a Java string into buf ("" for null); the local reference deleted
static void take_string(JNIEnv *env, jstring s, char *buf, size_t cap) {
    buf[0] = 0;
    if (!s) return;
    const char *c = (*env)->GetStringUTFChars(env, s, NULL);
    snprintf(buf, cap, "%s", c ? c : "");
    if (c) (*env)->ReleaseStringUTFChars(env, s, c);
    (*env)->DeleteLocalRef(env, s);
}

void android_pick_game_folder(void (*cb)(const char *name)) {
    g_pick_cb = cb;
    JNIEnv *env;
    jclass cls;
    jmethodID m = activity_method(&env, &cls, "pickGameFolder", "()V");
    if (!m) { if (cb) cb(NULL); return; }
    (*env)->CallStaticVoidMethod(env, cls, m);
    (*env)->DeleteLocalRef(env, cls);
}

const char *android_saved_game_folder(void) {
    static char name[512];
    JNIEnv *env;
    jclass cls;
    jmethodID m = activity_method(&env, &cls, "savedGameFolder", "()Ljava/lang/String;");
    if (!m) return NULL;
    jstring r = (jstring)(*env)->CallStaticObjectMethod(env, cls, m);
    (*env)->DeleteLocalRef(env, cls);
    take_string(env, r, name, sizeof name);
    return name[0] ? name : NULL;
}

const char *android_find_game_package(const char *keys_dir, char *uri, size_t uri_size) {
    static char result[512];
    uri[0] = 0;
    JNIEnv *env;
    jclass cls;
    jmethodID m = activity_method(&env, &cls, "findGamePackage", "(Ljava/lang/String;)Ljava/lang/String;");
    if (!m) return "error:no activity";
    jstring jdir = (*env)->NewStringUTF(env, keys_dir);
    jstring r = (jstring)(*env)->CallStaticObjectMethod(env, cls, m, jdir);
    (*env)->DeleteLocalRef(env, jdir);
    (*env)->DeleteLocalRef(env, cls);
    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return "error:exception"; }
    // "uri:<content uri>" when found, else the error
    take_string(env, r, result, sizeof result);
    if (!strncmp(result, "uri:", 4)) { snprintf(uri, uri_size, "%s", result + 4); return NULL; }
    return result[0] ? result : "error:?";
}

int android_open_document(const char *uri) {
    JNIEnv *env;
    jclass cls;
    jmethodID m = activity_method(&env, &cls, "openDocument", "(Ljava/lang/String;)I");
    if (!m) return -1;
    jstring ju = (*env)->NewStringUTF(env, uri);
    jint fd = (*env)->CallStaticIntMethod(env, cls, m, ju);
    (*env)->DeleteLocalRef(env, ju);
    (*env)->DeleteLocalRef(env, cls);
    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return -1; }
    return fd;
}

// ---- building the game code on the phone (EowActivity.buildTools / BuildService)
const char *android_build_tools(void) {
    static char dir[1024];
    JNIEnv *env;
    jclass cls;
    jmethodID m = activity_method(&env, &cls, "buildTools", "()Ljava/lang/String;");
    if (!m) return NULL;
    jstring r = (jstring)(*env)->CallStaticObjectMethod(env, cls, m);
    (*env)->DeleteLocalRef(env, cls);
    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return NULL; }
    take_string(env, r, dir, sizeof dir);
    return dir[0] ? dir : NULL;
}

void android_build_service(bool on, const char *title) {
    JNIEnv *env;
    jclass cls;
    jmethodID m = activity_method(&env, &cls, "buildService", "(ZLjava/lang/String;)V");
    if (!m) return;
    jstring t = (*env)->NewStringUTF(env, title ? title : "");
    (*env)->CallStaticVoidMethod(env, cls, m, (jboolean)on, t);
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    (*env)->DeleteLocalRef(env, t);
    (*env)->DeleteLocalRef(env, cls);
}

void android_build_progress(int percent, const char *text) {
    JNIEnv *env;
    jclass cls;
    jmethodID m = activity_method(&env, &cls, "buildProgress", "(ILjava/lang/String;)V");
    if (!m) return;
    jstring t = (*env)->NewStringUTF(env, text ? text : "");
    (*env)->CallStaticVoidMethod(env, cls, m, (jint)percent, t);
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    (*env)->DeleteLocalRef(env, t);
    (*env)->DeleteLocalRef(env, cls);
}

JNIEXPORT void JNICALL Java_com_eowrecomp_android_EowActivity_nativeFolderPicked(JNIEnv *env, jclass cls, jstring name) {
    (void)cls;
    const char *s = name ? (*env)->GetStringUTFChars(env, name, NULL) : NULL;
    if (g_pick_cb) g_pick_cb(s);
    if (s) (*env)->ReleaseStringUTFChars(env, name, s);
}
