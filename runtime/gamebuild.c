// Builds the game code library on the player's machine (runtime/gamebuild.h). Phones: the same, with the generator's
// Python and zig's clang carried in the APK (docs/BUILD-android.md, tools/android/ondevice/; after
// LASRecomp-Android-Plus, MIT).
#if defined(__ANDROID__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE  // (sched_setaffinity, cpu_set_t)
#endif
#include "gamebuild.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "gamelib.h"
#include "paths.h"
#include "platform.h"
#include "rt.h"
#ifdef __ANDROID__
#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>

#include "platform/android.h"
#endif

// From the Makefile: the flags the game code is compiled and linked with (as `make` builds it in a source checkout)
#ifndef GAMELIB_CFLAGS
#define GAMELIB_CFLAGS "-O1 -fno-strict-aliasing -fvisibility=hidden"
#endif
#ifndef GAMELIB_SHARED
#define GAMELIB_SHARED "-shared"
#endif

#ifdef _WIN32
#define SEP "\\"
#else
#define SEP "/"
#endif

// ------------------------------------------------------------------ child processes
// Every one running, so that quitting mid-build ends them too (atexit) instead of leaving compilers behind.
#define MAX_PROCS 80
static pthread_mutex_t g_plock = PTHREAD_MUTEX_INITIALIZER;
static PlatProc g_procs[MAX_PROCS];
static int g_nprocs;
static bool g_quitting;

static void end_all(void) {
    pthread_mutex_lock(&g_plock);
    g_quitting = true;
    for (int i = 0; i < g_nprocs; i++) plat_proc_kill(g_procs[i]);
    g_nprocs = 0;
    pthread_mutex_unlock(&g_plock);
}

static PlatProc spawn(char **argv, const char *log) {
    pthread_mutex_lock(&g_plock);
    PlatProc p = g_quitting ? 0 : plat_spawn((const char *const *)argv, log, false);
    if (p && g_nprocs < MAX_PROCS) g_procs[g_nprocs++] = p;
    pthread_mutex_unlock(&g_plock);
    return p;
}

// plat_proc_wait without blocking, then forgotten once it has ended
static int finished(PlatProc p) {
    pthread_mutex_lock(&g_plock);
    int r = plat_proc_wait(p, false);
    if (r >= 0)
        for (int i = 0; i < g_nprocs; i++)
            if (g_procs[i] == p) { g_procs[i] = g_procs[--g_nprocs]; break; }
    pthread_mutex_unlock(&g_plock);
    return r;
}

static void stop(PlatProc p) {  // ends one that is still running, then forgotten
    pthread_mutex_lock(&g_plock);
    bool ours = false;
    for (int i = 0; i < g_nprocs; i++)
        if (g_procs[i] == p) { g_procs[i] = g_procs[--g_nprocs]; ours = true; break; }
    pthread_mutex_unlock(&g_plock);
    if (ours) plat_proc_kill(p);
}

static void sleep_ms(int ms) { plat_sleep_until_ns(plat_now_ns() + (uint64_t)ms * 1000000ull); }

static int wait_for(PlatProc p) {
    int r;
    while ((r = finished(p)) < 0) sleep_ms(20);
    return r;
}

// ------------------------------------------------------------------ argument lists
typedef struct { char **v; int n, cap; } Args;
static void add(Args *a, const char *s) {
    if (a->n + 2 > a->cap) {
        a->cap = a->cap ? a->cap * 2 : 32;
        a->v = (char **)realloc(a->v, sizeof *a->v * a->cap);
    }
    a->v[a->n++] = strdup(s);
    a->v[a->n] = NULL;
}
static void add_split(Args *a, const char *flags) {  // (space-separated, no quoting)
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", flags);
    for (char *t = strtok(buf, " "); t; t = strtok(NULL, " ")) add(a, t);
}
static void args_free(Args *a) {
    for (int i = 0; i < a->n; i++) free(a->v[i]);
    free(a->v);
    memset(a, 0, sizeof *a);
}

// ------------------------------------------------------------------ the tools
typedef struct {
    char gen[1100], gen_script[1100];  // generator: eowgen, or (source checkout) the venv's python + tools/gen.py
    char cc[1100];                     // compiler: zig (as `zig cc`), or EOWRECOMP_CC / clang
    bool zig;
    char inc[1100], entry[1100], hle[1100];  // headers the game code includes, tools/gamelib_entry.c, HLE names
#ifdef _WIN32
    char implib[1100];  // the .exe's import library (the runtime functions the game code calls)
    char zig_rt[1100];  // the release's zig: its runtime built ahead of time (zig/rt, tools/zig_slim.sh), "" = none
#endif
#ifdef __ANDROID__
    // the app's native library folder (programs run only from there: libeowpython.so, libzig.so; and libmain.so, to
    // link against) and the NDK pieces unpacked from the APK's assets (files/ondevice/cckit: tools/android/ondevice/)
    char nlib[1100], kit[1100], ondev[1100];
#endif
} Tools;

static bool file_exists(const char *p) {
    struct stat st;
    return stat(p, &st) == 0;
}

#ifdef __ANDROID__
// The folder libmain.so was loaded from: the native library folder when the APK's libraries are extracted at install
// (android:extractNativeLibs, the on-phone build's packaging); from inside the APK ("base.apk!/lib/...") there are
// no programs to run.
static bool native_lib_dir(char *out, size_t n) {
    Dl_info di;
    if (!dladdr((void *)&native_lib_dir, &di) || !di.dli_fname || strchr(di.dli_fname, '!')) return false;
    snprintf(out, n, "%s", di.dli_fname);
    char *s = strrchr(out, '/');
    if (!s) return false;
    *s = 0;
    return true;
}

// Phones: the generator is libeowpython.so running tools/gen.py, the compiler zig's clang and ld.lld (libzig.so) with
// the NDK pieces (cckit/), all from the on-phone build's packaging. ondev: where EowActivity.buildTools unpacked the
// asset archive; NULL = not asked (only checks that the programs are there).
static bool find_tools_android(Tools *t, const char *ondev) {
    memset(t, 0, sizeof *t);
    if (!native_lib_dir(t->nlib, sizeof t->nlib)) return false;
    snprintf(t->gen, sizeof t->gen, "%s/libeowpython.so", t->nlib);
    snprintf(t->cc, sizeof t->cc, "%s/libzig.so", t->nlib);
    t->zig = true;
    if (!file_exists(t->gen) || !file_exists(t->cc)) return false;
    if (!ondev) return true;
    snprintf(t->ondev, sizeof t->ondev, "%s", ondev);
    snprintf(t->gen_script, sizeof t->gen_script, "%s/tools/gen.py", ondev);
    snprintf(t->kit, sizeof t->kit, "%s/cckit", ondev);
    snprintf(t->inc, sizeof t->inc, "%s/gamelib-src", t->kit);
    snprintf(t->entry, sizeof t->entry, "%s/gamelib_entry.c", t->inc);
    snprintf(t->hle, sizeof t->hle, "%s/hle_symbols.txt", t->inc);
    return file_exists(t->gen_script) && file_exists(t->entry) && file_exists(t->hle);
}

// MemAvailable in MB (/proc/meminfo), -1 if unknown
static long android_mem_available_mb(void) {
    FILE *f = fopen("/proc/meminfo", "rb");
    if (!f) return -1;
    char line[128];
    long kb = -1;
    while (fgets(line, sizeof line, f))
        if (sscanf(line, "MemAvailable: %ld kB", &kb) == 1) break;
    fclose(f);
    return kb < 0 ? -1 : kb / 1024;
}

static bool copy_file(const char *from, const char *to) {
    FILE *in = fopen(from, "rb"), *out = in ? fopen(to, "wb") : NULL;
    bool ok = in && out;
    static char buf[1 << 20];
    size_t n;
    while (ok && (n = fread(buf, 1, sizeof buf, in)) > 0) ok = fwrite(buf, 1, n, out) == n;
    if (in) ok = ok && !ferror(in), fclose(in);
    if (out && fclose(out) != 0) ok = false;
    return ok;
}

// The library goes where runtime/gamelib.c looks on a phone: the app's internal storage (the game folder is on the
// shared storage, mounted without exec). ondev = <internal>/ondevice. The external copy is removed.
static bool install_internal(const char *game_dir, const char *ondev, char *why, size_t why_size) {
    char internal[1100], from[1200], to[1200], tmp[1300];
    snprintf(internal, sizeof internal, "%s", ondev);
    char *s = strrchr(internal, '/');
    if (s) *s = 0;
    char a[PATH_MAX], b[PATH_MAX];
    if (realpath(game_dir, a) && realpath(internal, b) && !strcmp(a, b)) return true;  // (the same folder)
    static const char *const ext[2] = {"", ".id"};  // (the library first: a matching .id says it's complete)
    for (int i = 0; i < 2; i++) {
        snprintf(from, sizeof from, "%s/%s%s", game_dir, GAMELIB_NAME, ext[i]);
        snprintf(to, sizeof to, "%s/%s%s", internal, GAMELIB_NAME, ext[i]);
        snprintf(tmp, sizeof tmp, "%s.tmp", to);
        if (i == 0) {
            char id[1200];  // (an old pair: its id goes first, so a half-copied library never looks current)
            snprintf(id, sizeof id, "%s/%s.id", internal, GAMELIB_NAME);
            remove(id);
        }
        if (!copy_file(from, tmp) || rename(tmp, to) != 0) {
            remove(tmp);
            LOG("[gamebuild] installing %s: %s\n", to, strerror(errno));
            snprintf(why, why_size, "the game code couldn't be installed (storage full?)");
            return false;
        }
    }
    snprintf(from, sizeof from, "%s/%s", game_dir, GAMELIB_NAME);
    remove(from);
    LOG("[gamebuild] installed %s/%s\n", internal, GAMELIB_NAME);
    return true;
}
#endif

static bool find_tools(Tools *t) {
#ifdef __ANDROID__
    return find_tools_android(t, NULL);
#endif
    memset(t, 0, sizeof *t);
    const char *e = getenv("EOWRECOMP_GEN");
    if (e && *e) snprintf(t->gen, sizeof t->gen, "%s", e);
    else if (!rt_resource("eowgen" SEP "eowgen" PLAT_EXE_SUFFIX, NULL, t->gen, sizeof t->gen) &&
             !(rt_resource(NULL, "tools/" PLAT_VENV_PYTHON, t->gen, sizeof t->gen) &&
               rt_resource(NULL, "tools/gen.py", t->gen_script, sizeof t->gen_script)))
        return false;
    e = getenv("EOWRECOMP_CC");
    if (e && *e) snprintf(t->cc, sizeof t->cc, "%s", e);
    else if (rt_resource("zig" SEP "zig" PLAT_EXE_SUFFIX, NULL, t->cc, sizeof t->cc)) t->zig = true;
#ifndef _WIN32
    else if (rt_dev_mode() && file_exists("/usr/bin/clang")) snprintf(t->cc, sizeof t->cc, "/usr/bin/clang");
#endif
    else return false;
    if (!rt_resource("gamelib-src" SEP "cpu.h", "runtime/cpu.h", t->inc, sizeof t->inc) ||
        !rt_resource("gamelib-src" SEP "gamelib_entry.c", "tools/gamelib_entry.c", t->entry, sizeof t->entry) ||
        !rt_resource("gamelib-src" SEP "hle_symbols.txt", "runtime/hle/hle_symbols.txt", t->hle, sizeof t->hle))
        return false;
    char *s = strrchr(t->inc, '/'), *b = strrchr(t->inc, '\\');
    if (b > s) s = b;
    if (s) *s = 0;
#ifdef _WIN32
    if (!rt_resource("eowrecomp.dll.a", "build/eowrecomp.dll.a", t->implib, sizeof t->implib)) return false;
    t->zig_rt[0] = 0;
    if (t->zig && rt_resource("zig" SEP "rt" SEP "link.rsp", NULL, t->zig_rt, sizeof t->zig_rt)) {
        char *s = strrchr(t->zig_rt, '/'), *b = strrchr(t->zig_rt, '\\');
        if (b > s) s = b;
        if (s) *s = 0;
        else t->zig_rt[0] = 0;
    }
#endif
    return true;
}

bool gamebuild_available(void) {
    Tools t;
    return find_tools(&t);
}

static void compiler(Args *a, const Tools *t) {
    add(a, t->cc);
#ifdef __ANDROID__
    // zig's own clang driver (not `zig cc`: zig has no libc for Android and would build its compiler_rt from source),
    // the headers from the kit (bionic's few + this clang's own: tools/android/ondevice/build_cckit.sh)
    static const char *const sub[] = {"/clang/include", "/sysroot/usr/include", "/sysroot/usr/include/aarch64-linux-android"};
    add(a, "clang");
    add(a, "-target");
    add(a, "aarch64-linux-android30");
    add(a, "-nostdinc");
    for (int i = 0; i < 3; i++) {
        char inc[1200];
        snprintf(inc, sizeof inc, "%s%s", t->kit, sub[i]);
        add(a, "-isystem");
        add(a, inc);
    }
    return;
#endif
    if (t->zig) add(a, "cc");
}

// ------------------------------------------------------------------ generator progress (its log)
typedef struct {
    const char *name;
    char log[1100], out[1100];
    PlatProc p;
    int exit;                // -1 while running
    int files;               // chunk files written so far (complete)
    unsigned long long ci, n;  // functions written / all
} Gen;

static void read_gen_log(Gen *g) {
    FILE *f = fopen(g->log, "rb");
    if (!f) return;
    static char buf[1 << 16];
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    long from = size > (long)sizeof buf - 1 ? size - (long)sizeof buf + 1 : 0;
    fseek(f, from, SEEK_SET);
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = 0;
    const char *last = NULL;
    for (const char *p = strstr(buf, ": progress "); p; p = strstr(p + 1, ": progress ")) last = p;
    unsigned long long ci, all;
    int files;
    if (last && sscanf(last, ": progress %llu/%llu files %d", &ci, &all, &files) == 3) {
        g->ci = ci, g->n = all;
        if (files > g->files) g->files = files;
    }
}

static void log_tail(const char *path, char *out, size_t n) {  // the last line of a log (an error message)
    out[0] = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return;
    char buf[4096];
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, size > (long)sizeof buf - 1 ? size - (long)sizeof buf + 1 : 0, SEEK_SET);
    size_t k = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[k] = 0;
    while (k && (buf[k - 1] == '\n' || buf[k - 1] == '\r')) buf[--k] = 0;
    char *nl = strrchr(buf, '\n');
    snprintf(out, n, "%s", nl ? nl + 1 : buf);
}

// ------------------------------------------------------------------ compiling
typedef struct {
    char src[1100], obj[1100], log[1100];
    unsigned long long bytes;
} Unit;

typedef struct {
    Unit *u;
    int n, cap;
} Units;

static void add_unit(Units *us, const char *src, const char *work, const char *stem) {
    if (us->n == us->cap) {
        us->cap = us->cap ? us->cap * 2 : 64;
        us->u = (Unit *)realloc(us->u, sizeof *us->u * us->cap);
    }
    Unit *u = &us->u[us->n++];
    snprintf(u->src, sizeof u->src, "%s", src);
    snprintf(u->obj, sizeof u->obj, "%s" SEP "%s.o", work, stem);
    snprintf(u->log, sizeof u->log, "%s" SEP "%s.log", work, stem);
    struct stat st;
    u->bytes = stat(src, &st) == 0 ? (unsigned long long)st.st_size : 0;
}

// The flags the game code was built with (GAMELIB_CFLAGS, part of its id). zig cc reads -march=<cpu> with its own CPU
// names (x86_64_v3) and takes "-<feature>" off the end, so x86-64-v3 would be CPU "x86": the same CPU as -mcpu there.
static void add_cflags(Args *a, const Tools *t) {
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", GAMELIB_CFLAGS);
    for (char *f = strtok(buf, " "); f; f = strtok(NULL, " ")) {
#ifdef __ANDROID__
        if (!strcmp(f, "-Iruntime")) continue;  // (gamelib_id.sh's, for a computer's checkout: the kit's -I instead)
        add(a, f);
        continue;
#endif
        if (t->zig && !strncmp(f, "-march=", 7)) {
            char cpu[128];
            snprintf(cpu, sizeof cpu, "-mcpu=%s", f + 7);
            for (char *c = cpu + 6; *c; c++) if (*c == '-') *c = '_';
            add(a, cpu);
        } else {
            add(a, f);
        }
    }
}

static PlatProc start_compile(const Tools *t, const Unit *u) {
    Args a = {0};
    compiler(&a, t);
    add_cflags(&a, t);
    char inc[1200];
    snprintf(inc, sizeof inc, "-I%s", t->inc);
    add(&a, inc);
    add(&a, "-DGAMELIB_ID=\"" GAMELIB_ID "\"");
    add(&a, "-c");
    add(&a, u->src);
    add(&a, "-o");
    add(&a, u->obj);
    PlatProc p = spawn(a.v, u->log);
    args_free(&a);
    return p;
}

// One module's generator: tools/gen.py <name> <nso> <base> <out> --hle <names> [--shift] (base: runtime/modules.h's
// C literal)
static bool start_gen(const Tools *t, Gen *g, const char *game_dir, const char *base) {
    char nso[1100], b[32];
    snprintf(nso, sizeof nso, "%s" SEP "exefs" SEP "%s", game_dir, g->name);
    snprintf(b, sizeof b, "%.*s", (int)strcspn(base, "u"), base);  // (the C literal without its ull)
    Args a = {0};
    add(&a, t->gen);
    if (t->gen_script[0]) add(&a, t->gen_script);
    add(&a, g->name);
    add(&a, nso);
    add(&a, b);
    add(&a, g->out);
    add(&a, "--hle");
    add(&a, t->hle);
    if (GUEST_SHIFT) {  // (Android: the layout runtime/cpu.h moves)
        char sh[32];
        snprintf(sh, sizeof sh, "0x%llx", (unsigned long long)GUEST_SHIFT);
        add(&a, "--shift");
        add(&a, sh);
    }
    g->p = spawn(a.v, g->log);
    args_free(&a);
    return g->p != 0;
}

// ------------------------------------------------------------------ the build
bool gamebuild_run(const char *game_dir, const GameBuildOpts *o, char *why, size_t why_size) {
    uint64_t t0 = plat_now_ns();
    static bool registered;
    if (!registered) registered = true, atexit(end_all);
    Tools t;
#ifdef __ANDROID__
    // the generator and the NDK pieces out of the APK's assets (once per app version: EowActivity.buildTools)
    if (o->progress) o->progress(0, "Preparing the build tools...");
    // (test: EOWRECOMP_ONDEVICE_DIR=<an unpacked ondevice.zip>, without the app's Java side)
    const char *ondev = getenv("EOWRECOMP_ONDEVICE_DIR");
    if (!ondev || !*ondev) ondev = android_build_tools();
    if (!ondev || !find_tools_android(&t, ondev)) {
#else
    if (!find_tools(&t)) {
#endif
        snprintf(why, why_size, "the game code builder is missing from this copy of EoWRecomp");
        return false;
    }
#ifdef __ANDROID__
    int android_jobs;
    {
        // A phone stays usable: the compilers (they inherit this thread's niceness and CPUs) at a lower priority, one
        // per core, the little ones too (the build is heat-bound: little cores add throughput for little heat; the
        // biggest files go first). The generator's Python and its modules' libraries (ctypes) from the kit.
        setpriority(PRIO_PROCESS, (id_t)gettid(), 10);
        long n = sysconf(_SC_NPROCESSORS_CONF);
        int ncpu = n < 1 ? 1 : n > 64 ? 64 : (int)n;
        cpu_set_t set;
        CPU_ZERO(&set);
        for (int i = 0; i < ncpu; i++) CPU_SET(i, &set);
        sched_setaffinity(0, sizeof set, &set);
        android_jobs = ncpu;
        char v[2400];
        snprintf(v, sizeof v, "%s/pyhome", ondev);
        setenv("EOWPY_HOME", v, 1);
        snprintf(v, sizeof v, "%s/tools:%s/pyhome/lib/python3.14/site-packages", ondev, ondev);
        setenv("EOWPY_PATH", v, 1);
        setenv("LIBCAPSTONE_PATH", t.nlib, 1);
        setenv("LIBUNICORN_PATH", t.nlib, 1);
        setenv("TMPDIR", ondev, 1);
    }
#endif
    char work[1100], lib[1100], tmp[1100];
    snprintf(work, sizeof work, "%s" SEP ".gamebuild", game_dir);
    snprintf(lib, sizeof lib, "%s" SEP "%s", game_dir, GAMELIB_NAME);
    snprintf(tmp, sizeof tmp, "%s" SEP "%s.tmp", work, GAMELIB_NAME);
    rt_remove_game_file(game_dir, ".gamebuild");
    rt_remove_game_file(game_dir, GAMELIB_NAME);
    rt_remove_game_file(game_dir, GAMELIB_NAME ".id");
    mkdir(work, 0755);
    if (t.zig) {  // its caches in the work folder too (deleted below), not ~/.cache: hundreds of MB, used once
        char zc[1200];
        snprintf(zc, sizeof zc, "%s" SEP "zig-cache", work);
#ifdef _WIN32
        _putenv_s("ZIG_GLOBAL_CACHE_DIR", zc);
        _putenv_s("ZIG_LOCAL_CACHE_DIR", zc);
#else
        setenv("ZIG_GLOBAL_CACHE_DIR", zc, 1);
        setenv("ZIG_LOCAL_CACHE_DIR", zc, 1);
#endif
    }
    LOG("[gamebuild] generator %s%s%s, compiler %s%s, %d jobs\n", t.gen, t.gen_script[0] ? " " : "", t.gen_script,
        t.cc, t.zig ? " cc" : "", o->jobs);

    // 1. the generator, every module at once (phones: one after the other - the generator of main alone takes GBs);
    // their finished files are compiled while it goes on
#define GEN_MOD(mod, b) {.name = #mod},
#define GEN_BASE(mod, b) #b,
    Gen gens[GAME_NMODULES] = {GAME_MODULES(GEN_MOD)};
    static const char *const base[GAME_NMODULES] = {GAME_MODULES(GEN_BASE)};
#ifdef __ANDROID__
    const bool one_at_a_time = true;
#else
    const bool one_at_a_time = false;
#endif
    bool ok = true;
    for (int i = 0; i < GAME_NMODULES; i++) {
        gens[i].exit = -1;
        snprintf(gens[i].log, sizeof gens[i].log, "%s" SEP "gen_%s.log", work, gens[i].name);
        snprintf(gens[i].out, sizeof gens[i].out, "%s" SEP "%s", work, gens[i].name);
    }
    int gen_next = 0;  // the next module whose generator starts
    for (; gen_next < GAME_NMODULES && ok && (!one_at_a_time || gen_next == 0); gen_next++)
        if (!start_gen(&t, &gens[gen_next], game_dir, base[gen_next])) {
            snprintf(why, why_size, "the game code builder didn't start");
            ok = false;
        }

    Units units = {0};
    int queued[GAME_NMODULES] = {0};  // chunk files of each module put in units so far
    bool tables[GAME_NMODULES] = {false};
    int next = 0, jobs = o->jobs < 1 ? 1 : o->jobs > 64 ? 64 : o->jobs;
#ifdef __ANDROID__
    if (android_jobs > jobs) jobs = android_jobs;
    uint64_t last_start = 0;  // (memory: see below)
    long low_mem = -1;        // the lowest MemAvailable seen while starting compilers (the log, for tuning)
    int held = 0;             // compiler starts put off for memory
#endif
    {   // (testing / tuning: EOWRECOMP_BUILD_JOBS)
        const char *e = getenv("EOWRECOMP_BUILD_JOBS");
        if (e && atoi(e) > 0) jobs = atoi(e) > 64 ? 64 : atoi(e);
    }
    struct { PlatProc p; int unit; } run[64];
    int nrun = 0;
    unsigned long long done_bytes = 0;
    double shown = 0;
    {
        add_unit(&units, t.entry, work, "gamelib_entry");
    }
    while (ok) {
        // generator progress, newly finished files
        bool gens_done = true;
        double gen_frac = 0;
        unsigned long long written = 0;
        if (one_at_a_time && gen_next < GAME_NMODULES && gens[gen_next - 1].exit == 0) {  // (the previous one done)
            if (!start_gen(&t, &gens[gen_next], game_dir, base[gen_next])) {
                snprintf(why, why_size, "the game code builder didn't start");
                ok = false;
                break;
            }
            gen_next++;
        }
        for (int i = 0; i < GAME_NMODULES; i++) {
            Gen *g = &gens[i];
            if (g->exit < 0 && g->p) {  // (not a waiting one: no process yet)
                read_gen_log(g);
                int r = finished(g->p);
                if (r >= 0) {
                    g->exit = r;
                    read_gen_log(g);
                    if (r != 0) {
                        char last[300];
                        log_tail(g->log, last, sizeof last);
                        LOG("[gamebuild] generator (%s) failed: %s\n", g->name, last);
                        snprintf(why, why_size, "the game code couldn't be prepared");
                        ok = false;
                    }
                }
            }
            if (g->exit != 0) gens_done = false;
            for (; queued[i] < g->files; queued[i]++) {
                char src[1100], stem[64];
                snprintf(stem, sizeof stem, "%s_%03d", g->name, queued[i]);
                snprintf(src, sizeof src, "%s" SEP "%s.c", g->out, stem);
                add_unit(&units, src, work, stem);
            }
            if (g->exit == 0 && !tables[i]) {
                char src[1100], stem[64];
                snprintf(stem, sizeof stem, "%s_table", g->name);
                snprintf(src, sizeof src, "%s" SEP "%s.c", g->out, stem);
                add_unit(&units, src, work, stem);
                tables[i] = true;
            }
            gen_frac += (g->exit == 0 ? 1.0 : g->n ? (double)g->ci / (double)g->n : 0) / GAME_NMODULES;
        }
        if (!ok) break;
        for (int k = 0; k < units.n; k++) written += units.u[k].bytes;

        // compiles
        while (nrun < jobs && next < units.n) {
#ifdef __ANDROID__
            // Memory: the generators take GBs and the biggest compiles hundreds of MB each, on phones with 6-8 GB.
            // Below 1.5 GB available a new compiler starts at most once a second (its memory shows in MemAvailable by
            // then), below 700 MB not at all while two still run - the build slows down instead of the system
            // killing it.
            if (nrun >= 2) {
                long avail = android_mem_available_mb();
                uint64_t now = plat_now_ns();
                if (avail >= 0 && (low_mem < 0 || avail < low_mem)) low_mem = avail;
                if (avail >= 0 && (avail < 700 || (avail < 1500 && now - last_start < 1000000000ull))) { ++held; break; }
            }
#endif
            PlatProc p = start_compile(&t, &units.u[next]);
            if (!p) { snprintf(why, why_size, "the compiler didn't start"); ok = false; break; }
            run[nrun].p = p, run[nrun].unit = next++;
            nrun++;
#ifdef __ANDROID__
            last_start = plat_now_ns();
#endif
        }
        for (int k = 0; k < nrun; k++) {
            int r = finished(run[k].p);
            if (r < 0) continue;
            Unit *u = &units.u[run[k].unit];
            if (r != 0) {
                char last[300];
                log_tail(u->log, last, sizeof last);
                LOG("[gamebuild] compiling %s failed: %s\n", u->src, last);
                snprintf(why, why_size, "the game code didn't compile");
                ok = false;
            }
            done_bytes += u->bytes;
            if (run[k].unit > 0) remove(u->src);  // (generated C, done with: not kept. Unit 0 is the shipped entry file)
            run[k--] = run[--nrun];
        }
        if (!ok) break;
        if (gens_done && next == units.n && nrun == 0) {
#ifdef __ANDROID__
            LOG("[gamebuild] lowest MemAvailable %ld MB, %d compiler starts put off for memory\n", low_mem, held);
#endif
            break;
        }

        // progress: the generator's share (its functions written), the compiler's (bytes of C compiled, of an
        // estimate of all of it while the generator still writes)
        double est_total = gens_done ? (double)written : gen_frac > 0.02 ? (double)written / gen_frac : 0;
        double comp_frac = est_total > 0 ? (double)done_bytes / est_total : 0;
        if (comp_frac > 1) comp_frac = 1;
        double f = 0.2 * gen_frac + 0.75 * comp_frac;
        double secs = (double)(plat_now_ns() - t0) / 1e9;
        if (gen_frac == 0) f = 0.05 * (secs / (secs + 20));  // (the generator's first pass: no count yet)
        if (f > shown) shown = f;
        if (o->progress) o->progress(shown, gen_frac < 1 && !gens_done ? "Reading the game code..." : "Recompiling...");
        sleep_ms(50);
    }
    for (int k = 0; k < nrun; k++) stop(run[k].p);  // (a failure: the rest is of no use - the failure shows at once)
    for (int i = 0; i < GAME_NMODULES; i++)
        if (gens[i].p && gens[i].exit < 0) stop(gens[i].p);

    // 2. link
    if (ok) {
        if (o->progress) o->progress(shown > 0.95 ? shown : 0.95, "Finishing...");
        char rsp[1100], log[1100];
        snprintf(rsp, sizeof rsp, "%s" SEP "link.rsp", work);
        snprintf(log, sizeof log, "%s" SEP "link.log", work);
        FILE *f = fopen(rsp, "wb");
        if (f) {
            for (int k = 0; k < units.n; k++) {  // (forward slashes: the response file's backslashes are escapes)
                fputc('"', f);
                for (const char *c = units.u[k].obj; *c; c++) fputc(*c == '\\' ? '/' : *c, f);
                fputs("\"\n", f);
            }
            fclose(f);
        }
        Args a = {0};
        char at[1200];
        snprintf(at, sizeof at, "@%s", rsp);
#ifdef __ANDROID__
        {   // zig's ld.lld as NDK clang runs it (cckit/link.rsp: tools/android/ondevice/build_cckit.sh), against the
            // app's own libmain.so (the runtime functions the game code calls: it is loaded first)
            char opt[1200];
            static const char *const sys = "/sysroot/usr/lib/aarch64-linux-android/30";
            add(&a, t.cc);
            add(&a, "ld.lld");
            snprintf(opt, sizeof opt, "@%s/link.rsp", t.kit);
            add(&a, opt);
            add(&a, "-o");
            add(&a, tmp);
            snprintf(opt, sizeof opt, "%s%s/crtbegin_so.o", t.kit, sys);
            add(&a, opt);
            add(&a, at);
            snprintf(opt, sizeof opt, "-L%s", t.nlib);
            add(&a, opt);
            add(&a, "-lmain");
            snprintf(opt, sizeof opt, "-L%s%s", t.kit, sys);
            add(&a, opt);
            snprintf(opt, sizeof opt, "%s/clang/lib/libclang_rt.builtins-aarch64-android.a", t.kit);
            add(&a, opt);
            add(&a, "-ldl");
            add(&a, "-lc");
            add(&a, opt);
            snprintf(opt, sizeof opt, "%s%s/crtend_so.o", t.kit, sys);
            add(&a, opt);
        }
        if (0)
#endif
#ifdef _WIN32
        if (t.zig_rt[0]) {  // the linker itself, with the runtime built ahead of time (its flags and libraries: link.rsp)
            char opt[1200];
            add(&a, t.cc);
            add(&a, "lld-link");
            snprintf(opt, sizeof opt, "@%s" SEP "link.rsp", t.zig_rt);
            add(&a, opt);
            snprintf(opt, sizeof opt, "-LIBPATH:%s", t.zig_rt);
            add(&a, opt);
            snprintf(opt, sizeof opt, "-OUT:%s", tmp);
            add(&a, opt);
            add(&a, at);
            add(&a, t.implib);
        } else
#endif
        {
            compiler(&a, &t);
            add_split(&a, GAMELIB_SHARED);
            add(&a, "-o");
            add(&a, tmp);
            add(&a, at);
#ifdef _WIN32
            add(&a, t.implib);
#endif
        }
        PlatProc p = f ? spawn(a.v, log) : 0;
        args_free(&a);
        int r = p ? wait_for(p) : 255;
        if (r != 0) {
            char last[300];
            log_tail(log, last, sizeof last);
            LOG("[gamebuild] linking failed: %s\n", last);
            snprintf(why, why_size, "the game code didn't link");
            ok = false;
        }
    }
    if (ok) {
        remove(lib);
        char idp[1100];
        snprintf(idp, sizeof idp, "%s.id", lib);
        FILE *f = NULL;
        if (rename(tmp, lib) != 0 || !(f = fopen(idp, "wb")) || fprintf(f, "%s\n", GAMELIB_ID) < 0) {
            snprintf(why, why_size, "the game code couldn't be saved");
            ok = false;
        }
        if (f) fclose(f);
    }
#ifdef __ANDROID__
    if (ok) ok = install_internal(game_dir, t.ondev, why, why_size);
#endif
    free(units.u);
    rt_remove_game_file(game_dir, ".gamebuild");  // the C, the objects, the logs
    LOG("[gamebuild] %s in %.0fs\n", ok ? "done" : "failed", (double)(plat_now_ns() - t0) / 1e9);
    if (ok && o->progress) o->progress(1, "Finishing...");
    return ok;
}
