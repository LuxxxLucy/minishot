#include <string.h>
#include "test.h"
#include "hotkey.h"

int main(void)
{
    // --- canonical example from the contract ---
    ms_hotkey h = ms_hotkey_parse("cmd+shift+a");
    CHECK(h.ok);
    CHECK_EQ(h.mods, MINISHOT_MOD_CMD | MINISHOT_MOD_SHIFT);
    CHECK_EQ(h.key, 'a');

    // --- single non-modifier key, no modifiers ---
    h = ms_hotkey_parse("a");
    CHECK(h.ok);
    CHECK_EQ(h.mods, 0u);
    CHECK_EQ(h.key, 'a');

    // --- each modifier individually maps to the right bit ---
    h = ms_hotkey_parse("cmd+a");
    CHECK(h.ok);
    CHECK_EQ(h.mods, MINISHOT_MOD_CMD);
    CHECK_EQ(h.key, 'a');

    h = ms_hotkey_parse("shift+a");
    CHECK(h.ok);
    CHECK_EQ(h.mods, MINISHOT_MOD_SHIFT);
    CHECK_EQ(h.key, 'a');

    h = ms_hotkey_parse("alt+a");
    CHECK(h.ok);
    CHECK_EQ(h.mods, MINISHOT_MOD_ALT);
    CHECK_EQ(h.key, 'a');

    h = ms_hotkey_parse("ctrl+a");
    CHECK(h.ok);
    CHECK_EQ(h.mods, MINISHOT_MOD_CTRL);
    CHECK_EQ(h.key, 'a');

    // --- all four modifiers at once ---
    h = ms_hotkey_parse("cmd+shift+alt+ctrl+z");
    CHECK(h.ok);
    CHECK_EQ(h.mods, MINISHOT_MOD_CMD | MINISHOT_MOD_SHIFT | MINISHOT_MOD_ALT |
                         MINISHOT_MOD_CTRL);
    CHECK_EQ(h.key, 'z');

    // --- final token is lowercased ---
    h = ms_hotkey_parse("cmd+A");
    CHECK(h.ok);
    CHECK_EQ(h.mods, MINISHOT_MOD_CMD);
    CHECK_EQ(h.key, 'a');

    h = ms_hotkey_parse("Z");
    CHECK(h.ok);
    CHECK_EQ(h.mods, 0u);
    CHECK_EQ(h.key, 'z');

    // --- a digit is a valid final token ---
    h = ms_hotkey_parse("cmd+4");
    CHECK(h.ok);
    CHECK_EQ(h.mods, MINISHOT_MOD_CMD);
    CHECK_EQ(h.key, '4');

    // --- modifier order does not matter; bits accumulate ---
    h = ms_hotkey_parse("shift+cmd+a");
    CHECK(h.ok);
    CHECK_EQ(h.mods, MINISHOT_MOD_CMD | MINISHOT_MOD_SHIFT);
    CHECK_EQ(h.key, 'a');

    // --- the SAME modifier twice still sets one bit (idempotent OR) ---
    h = ms_hotkey_parse("cmd+cmd+a");
    CHECK(h.ok);
    CHECK_EQ(h.mods, MINISHOT_MOD_CMD);
    CHECK_EQ(h.key, 'a');

    // --- failure cases: ok must be 0 ---

    // empty string
    h = ms_hotkey_parse("");
    CHECK_EQ(h.ok, 0);

    // unknown token
    h = ms_hotkey_parse("foo+a");
    CHECK_EQ(h.ok, 0);

    // only a modifier, no final key
    h = ms_hotkey_parse("cmd");
    CHECK_EQ(h.ok, 0);

    h = ms_hotkey_parse("cmd+shift");
    CHECK_EQ(h.ok, 0);

    // a modifier name appearing where the final key should be is not a key
    h = ms_hotkey_parse("a+cmd");
    CHECK_EQ(h.ok, 0);

    TEST_REPORT();
}
