// Entry of the game code library (runtime/gamelib.h), compiled into it with the generated code: the function
// tables, found by name when the app loads the library. Everything else in it stays hidden.
#include "gamelib.h"

#define DECLARE(name, base) \
    extern const uint64_t name##_fn_addrs[], name##_fn_count; \
    extern const GuestFn name##_fn_ptrs[];
GAME_MODULES(DECLARE)

#define ENTRY(name, base) {#name, name##_fn_addrs, name##_fn_ptrs, &name##_fn_count},
static const GameLibModule modules[] = {GAME_MODULES(ENTRY)};

GAMELIB_EXPORT const GameLibTables *eowgame_tables(void) {
    static const GameLibTables t = {GAMELIB_ABI, GAMELIB_ID, GAME_NMODULES, modules};
    return &t;
}
