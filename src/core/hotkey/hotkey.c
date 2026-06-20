#include "hotkey.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

static int mod_bit(const char *tok, size_t len)
{
    if (len == 3 && strncmp(tok, "cmd", 3) == 0) {
        return MINISHOT_MOD_CMD;
    }
    if (len == 5 && strncmp(tok, "shift", 5) == 0) {
        return MINISHOT_MOD_SHIFT;
    }
    if (len == 3 && strncmp(tok, "alt", 3) == 0) {
        return MINISHOT_MOD_ALT;
    }
    if (len == 4 && strncmp(tok, "ctrl", 4) == 0) {
        return MINISHOT_MOD_CTRL;
    }
    return 0;
}

struct ms_hotkey ms_hotkey_parse(const char *s)
{
    struct ms_hotkey hk = { 0, 0, 0 };
    if (s == NULL || *s == '\0') {
        return hk;
    }

    unsigned mods = 0;
    const char *p = s;
    for (;;) {
        const char *plus = strchr(p, '+');
        size_t len = plus ? (size_t)(plus - p) : strlen(p);

        if (plus) {
            // Non-final token must be a known modifier.
            int bit = mod_bit(p, len);
            if (bit == 0) {
                return hk;
            }
            mods |= (unsigned)bit;
            p = plus + 1;
            continue;
        }

        // Final token must be a single non-modifier character.
        if (len != 1) {
            return hk;
        }
        if (mod_bit(p, len) != 0) {
            return hk;  // (never matches len==1, but explicit)
        }
        hk.mods = mods;
        hk.key = (unsigned char)tolower((unsigned char)p[0]);
        hk.ok = 1;
        return hk;
    }
}

#ifdef __APPLE__
#include <Carbon/Carbon.h>

static void (*g_hk_cb)(void *ud) = NULL;
static void *g_hk_ud = NULL;

static OSStatus hk_handler(EventHandlerCallRef next, EventRef ev, void *ud)
{
    (void)next;
    (void)ev;
    (void)ud;
    if (g_hk_cb) {
        g_hk_cb(g_hk_ud);
    }
    return noErr;
}

// Map an ASCII letter/digit to a US-ANSI virtual keycode, or -1.
static int ascii_to_vk(unsigned c)
{
    switch (c) {
        case 'a':
            return kVK_ANSI_A;
        case 'b':
            return kVK_ANSI_B;
        case 'c':
            return kVK_ANSI_C;
        case 'd':
            return kVK_ANSI_D;
        case 'e':
            return kVK_ANSI_E;
        case 'f':
            return kVK_ANSI_F;
        case 'g':
            return kVK_ANSI_G;
        case 'h':
            return kVK_ANSI_H;
        case 'i':
            return kVK_ANSI_I;
        case 'j':
            return kVK_ANSI_J;
        case 'k':
            return kVK_ANSI_K;
        case 'l':
            return kVK_ANSI_L;
        case 'm':
            return kVK_ANSI_M;
        case 'n':
            return kVK_ANSI_N;
        case 'o':
            return kVK_ANSI_O;
        case 'p':
            return kVK_ANSI_P;
        case 'q':
            return kVK_ANSI_Q;
        case 'r':
            return kVK_ANSI_R;
        case 's':
            return kVK_ANSI_S;
        case 't':
            return kVK_ANSI_T;
        case 'u':
            return kVK_ANSI_U;
        case 'v':
            return kVK_ANSI_V;
        case 'w':
            return kVK_ANSI_W;
        case 'x':
            return kVK_ANSI_X;
        case 'y':
            return kVK_ANSI_Y;
        case 'z':
            return kVK_ANSI_Z;
        case '0':
            return kVK_ANSI_0;
        case '1':
            return kVK_ANSI_1;
        case '2':
            return kVK_ANSI_2;
        case '3':
            return kVK_ANSI_3;
        case '4':
            return kVK_ANSI_4;
        case '5':
            return kVK_ANSI_5;
        case '6':
            return kVK_ANSI_6;
        case '7':
            return kVK_ANSI_7;
        case '8':
            return kVK_ANSI_8;
        case '9':
            return kVK_ANSI_9;
        default:
            return -1;
    }
}

int ms_hotkey_register(struct ms_hotkey hk, void (*cb)(void *ud), void *ud)
{
    if (!hk.ok) {
        return -1;
    }
    int vk = ascii_to_vk(hk.key);
    if (vk < 0) {
        return -1;
    }

    UInt32 mods = 0;
    if (hk.mods & MINISHOT_MOD_CMD) {
        mods |= cmdKey;
    }
    if (hk.mods & MINISHOT_MOD_SHIFT) {
        mods |= shiftKey;
    }
    if (hk.mods & MINISHOT_MOD_ALT) {
        mods |= optionKey;
    }
    if (hk.mods & MINISHOT_MOD_CTRL) {
        mods |= controlKey;
    }

    g_hk_cb = cb;
    g_hk_ud = ud;

    EventTypeSpec spec = { kEventClassKeyboard, kEventHotKeyPressed };
    InstallApplicationEventHandler(&hk_handler, 1, &spec, NULL, NULL);

    EventHotKeyID hkid = { 'mnsh', 1 };
    EventHotKeyRef ref;
    OSStatus st = RegisterEventHotKey((UInt32)vk, mods, hkid,
                                      GetApplicationEventTarget(), 0, &ref);
    return st == noErr ? 0 : -1;
}
#else
int ms_hotkey_register(struct ms_hotkey hk, void (*cb)(void *ud), void *ud)
{
    (void)hk;
    (void)cb;
    (void)ud;
    return -1;
}
#endif
