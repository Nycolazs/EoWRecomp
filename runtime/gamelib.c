// Loads the recompiled game code library (runtime/gamelib.h).
#include "gamelib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#ifdef __ANDROID__
#include <SDL3/SDL.h>  // (SDL_GetAndroidInternalStoragePath)
#endif

#include "paths.h"
#include "rt.h"

static const GameLibTables *g_tables;
static char g_error[256];

// The library's id file (<library>.id, written next to it by the build): its GAMELIB_ID, read without loading it.
static bool id_matches(const char *lib) {
    char path[1100], id[128] = {0};
    snprintf(path, sizeof path, "%s.id", lib);
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    size_t n = fread(id, 1, sizeof id - 1, f);
    fclose(f);
    while (n && (id[n - 1] == '\n' || id[n - 1] == '\r')) id[--n] = 0;
    return !strcmp(id, GAMELIB_ID);
}

static bool exists(const char *p) {
    FILE *f = fopen(p, "rb");
    if (f) fclose(f);
    return f != NULL;
}

// The places to look, in order (gamelib.h). Returns how many were filled.
// Android: the app's internal storage (the data folder is on external storage, mounted without exec), then one packed
// into the APK itself, opened by its bare name (it lives inside the APK: no file to look at, its own id decides).
static int candidates(const char *game_dir, char out[3][1100]) {
    int n = 0;
    const char *env = getenv("EOWRECOMP_GAME_LIB");
    if (env && *env) snprintf(out[n++], 1100, "%s", env);
#ifdef __ANDROID__
    (void)game_dir;
    const char *internal = SDL_GetAndroidInternalStoragePath();
    if (internal) snprintf(out[n++], 1100, "%s/%s", internal, GAMELIB_NAME);
    snprintf(out[n++], 1100, "%s", GAMELIB_NAME);
    return n;
#endif
    char exe_side[1024];
    if (rt_resource(GAMELIB_NAME, NULL, exe_side, sizeof exe_side) && exists(exe_side))
        snprintf(out[n++], 1100, "%s", exe_side);
    if (game_dir) snprintf(out[n++], 1100, "%s/%s", game_dir, GAMELIB_NAME);
    return n;
}

static bool in_apk(const char *c) { return !strchr(c, '/'); }  // (Android: a bare name)

bool gamelib_present(const char *game_dir) {
    if (g_tables) return true;
    char c[3][1100];
    int n = candidates(game_dir, c);
    for (int i = 0; i < n; i++) {
        if (in_apk(c[i])) {  // (loading it is the only way to look: the library stays loaded for the game)
            char why[8];
            if (gamelib_load(NULL, why, sizeof why)) return true;
            continue;
        }
        if (exists(c[i]) && id_matches(c[i])) return true;
    }
    return false;
}

static const GameLibTables *open_lib(const char *path) {
#ifdef _WIN32
    HMODULE h = LoadLibraryA(path);
    if (!h) { snprintf(g_error, sizeof g_error, "can't load %s (error %lu)", path, GetLastError()); return NULL; }
    GameLibEntry entry = (GameLibEntry)(void *)GetProcAddress(h, GAMELIB_ENTRY);
#else
    void *h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!h) { snprintf(g_error, sizeof g_error, "%s", dlerror()); return NULL; }
    GameLibEntry entry = (GameLibEntry)dlsym(h, GAMELIB_ENTRY);
#endif
    if (!entry) { snprintf(g_error, sizeof g_error, "%s: not game code", path); return NULL; }
    const GameLibTables *t = entry();
    if (!t || t->abi != GAMELIB_ABI || !t->id || strcmp(t->id, GAMELIB_ID)) {
        snprintf(g_error, sizeof g_error, "%s was built for another version", path);
        return NULL;  // (left loaded: nothing of it runs)
    }
    return t;
}

bool gamelib_load(const char *game_dir, char *why, size_t why_size) {
    static bool tried;
    if (!tried) {
        tried = true;
        snprintf(g_error, sizeof g_error, "the game code hasn't been built: set up the game files on the start screen");
        char c[3][1100];
        int n = candidates(game_dir, c);
        const char *env = getenv("EOWRECOMP_GAME_LIB");
        for (int i = 0; i < n && !g_tables; i++) {
            if (!in_apk(c[i]) && !exists(c[i])) continue;
            // EOWRECOMP_GAME_LIB / the APK's own: loaded as given, the library's own id decides
            bool given = (i == 0 && env && *env) || in_apk(c[i]);
            if (!given && !id_matches(c[i])) {
                snprintf(g_error, sizeof g_error, "%s was built for another version", c[i]);
                continue;
            }
            g_tables = open_lib(c[i]);
            if (g_tables) {
                unsigned long long n = 0;
                for (uint32_t k = 0; k < g_tables->nmodules; k++) n += *g_tables->modules[k].count;
                LOG("[gamelib] %s (%u modules, %llu functions)\n", c[i], g_tables->nmodules, n);
            }
        }
    }
    if (!g_tables && why) snprintf(why, why_size, "%s", g_error);
    return g_tables != NULL;
}

const GameLibTables *gamelib_tables(void) { return g_tables; }
