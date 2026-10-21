// What only the Android app has (runtime/platform/android_jni.c, with EowActivity.java): the system's folder picker
// and the player's game package read through the documents it hands out (no file paths there: file descriptors).
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opens the folder picker; cb gets the picked folder's name (shown on the start screen), or NULL if none, on the UI
// thread.
void android_pick_game_folder(void (*cb)(const char *name));
// The folder picked in an earlier session (still readable), or NULL
const char *android_saved_game_folder(void);
// In the picked folder (or a few levels below it): the game package (.nsp / .xci, the biggest) and the keys. The keys
// files (prod.keys, title.keys) are copied into keys_dir; the package's document URI goes into uri. NULL = found,
// else what is missing: "nofolder", "notfound" (no package), "nokeys", "error:<details>".
const char *android_find_game_package(const char *keys_dir, char *uri, size_t uri_size);
// A document (content:// URI from android_find_game_package) opened for reading: a file descriptor of this process
// (the caller closes it), or -1
int android_open_document(const char *uri);

// ---- building the game code on the phone (runtime/gamebuild.c)
// The generator, its Python and the compiler's NDK pieces unpacked from the APK's assets into the app's internal
// storage (once per app version; blocking): that folder, or NULL (an APK without them, or no space)
const char *android_build_tools(void);
// The build's foreground service (keeps the app running with the screen off: wake lock, a notification); on = false
// ends it. text: what the notification says (any thread).
void android_build_service(bool on, const char *title);
void android_build_progress(int percent, const char *text);

#ifdef __cplusplus
}
#endif
