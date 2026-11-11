// Exercise the production settings reader and language code served to both set/am.
#include "../runtime/settings.c"

bool bind_settings_load(const char *key, const char *value) { (void)key; (void)value; return false; }
void bind_settings_save(FILE *f) { (void)f; }

int main(int argc, char **argv) {
    if (argc != 3) return 2;
    setenv("EOWRECOMP_SETTINGS", argv[1], 1);
    settings_load();
    const char *actual = settings_language();
    bool ok = !strcmp(actual, argv[2]);
    printf("%s language: %s (want %s)\n", ok ? "PASS" : "FAIL", actual, argv[2]);
    return ok ? 0 : 1;
}
