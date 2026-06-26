#ifndef MINISHOT_HOTKEY_MODIFIER_H
#define MINISHOT_HOTKEY_MODIFIER_H

enum {
    MINISHOT_HOTKEY_MODIFIER_CMD = 1,
    MINISHOT_HOTKEY_MODIFIER_SHIFT = 2,
    MINISHOT_HOTKEY_MODIFIER_ALT = 4,
    MINISHOT_HOTKEY_MODIFIER_CTRL = 8,
};

// key = ASCII of the final non-modifier token, lowercased.
// ok = 0 on parse failure.
struct ms_hotkey {
    unsigned mods;
    unsigned key;
    int ok;
};

// Parse a hotkey string such as "cmd+shift+a", unknown would fail (ok=0)
// example:
//      "cmd+shift+a" -> {
//      MINISHOT_HOTKEY_MODIFIER_CMD|MINISHOT_HOTKEY_MODIFIER_SHIFT, 'a', ok=1
//      }.
struct ms_hotkey ms_hotkey_parse(const char *s);

// Register the hotkey via Carbon RegisterEventHotKey.
// Returns, 0 on sucess, -1 for failure
int ms_hotkey_register(struct ms_hotkey hk, void (*cb)(void *ud), void *ud);

#endif
