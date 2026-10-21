// tools/android/ondevice/pylauncher.c: the generator's Python on the phone (docs/BUILD-android.md). CPython's
// Android release is an embeddable libpython3.14.so without a `python` program; this is that program, shipped as
// libeowpython.so in jniLibs (Android runs programs only from the app's native library folder).
//   libeowpython.so <script> [args]   - as `python -I <script> [args]`
// Its home (the standard library: <home>/lib/python3.14, lib-dynload/) is $EOWPY_HOME; $EOWPY_PATH (':'-separated) is
// put in front of sys.path (tools/ and the site-packages holding capstone, unicorn, lz4). The hash seed is fixed (0) as
// in the desktop helper (tools/eowgen.spec hash_seed=0): the same generated C on every run.
#include <Python.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(PyConfig *c, PyStatus s) {
    PyConfig_Clear(c);
    if (PyStatus_IsExit(s)) return s.exitcode;
    Py_ExitStatusException(s);
    return 1;
}

int main(int argc, char **argv) {
    PyConfig c;
    PyConfig_InitIsolatedConfig(&c);  // (no PYTHON* variables, no user site, no script folder magic beyond argv[0]'s)
    c.use_hash_seed = 1;
    c.hash_seed = 0;
    c.parse_argv = 1;
    c.install_signal_handlers = 1;
    c.buffered_stdio = 0;  // (progress lines reach the log as they are printed: runtime/gamebuild.c read_gen_log)
    PyStatus s;
    const char *home = getenv("EOWPY_HOME");
    if (home && *home) {
        s = PyConfig_SetBytesString(&c, &c.home, home);
        if (PyStatus_Exception(s)) return fail(&c, s);
    }
    s = PyConfig_SetBytesArgv(&c, argc, argv);
    if (PyStatus_Exception(s)) return fail(&c, s);
    s = PyConfig_Read(&c);
    if (PyStatus_Exception(s)) return fail(&c, s);
    // sys.path, all of it (set here, Python computes none): $EOWPY_PATH, then the standard library and its modules
    const char *extra = getenv("EOWPY_PATH");
    char std[2][1200], paths[4096];
    snprintf(std[0], sizeof std[0], "%s/lib/python3.14", home ? home : ".");
    snprintf(std[1], sizeof std[1], "%s/lib/python3.14/lib-dynload", home ? home : ".");
    snprintf(paths, sizeof paths, "%s%s%s:%s", extra ? extra : "", extra && *extra ? ":" : "", std[0], std[1]);
    char *save = NULL;
    for (char *p = strtok_r(paths, ":", &save); p; p = strtok_r(NULL, ":", &save)) {
        wchar_t *w = Py_DecodeLocale(p, NULL);
        if (!w) continue;
        s = PyWideStringList_Append(&c.module_search_paths, w);
        PyMem_RawFree(w);
        if (PyStatus_Exception(s)) return fail(&c, s);
    }
    c.module_search_paths_set = 1;
    s = Py_InitializeFromConfig(&c);
    if (PyStatus_Exception(s)) return fail(&c, s);
    PyConfig_Clear(&c);
    return Py_RunMain();
}
