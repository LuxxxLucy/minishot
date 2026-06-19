#ifndef MINISHOT_HOTKEY_H
#define MINISHOT_HOTKEY_H

enum {
    MINISHOT_MOD_CMD = 1,
    MINISHOT_MOD_SHIFT = 2,
    MINISHOT_MOD_ALT = 4,
    MINISHOT_MOD_CTRL = 8,
};

// key = ASCII of the final non-modifier token, lowercased.
// ok = 0 on parse failure.
typedef struct {
    unsigned mods;
    unsigned key;
    int ok;
} ms_hotkey;

// Parse a hotkey string such as "cmd+shift+a".
// "cmd+shift+a" -> { MINISHOT_MOD_CMD|MINISHOT_MOD_SHIFT, 'a', ok=1 }.
// Unknown token or empty string -> ok=0. PURE.
ms_hotkey ms_hotkey_parse(const char *s);

// Register the hotkey via Carbon RegisterEventHotKey. Compile-only here.
// Returns 0 on success, -1 on failure.
int ms_hotkey_register(ms_hotkey hk, void (*cb)(void *ud), void *ud);

#endif
