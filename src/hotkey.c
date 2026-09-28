/* hotkey.c - Searchlight 98: hotkeys and the settings kept in the registry. */
#include "slight98.h"

static const char APP_KEY[]     = "Software\\Searchlight 98";

void BuildKeyList(void)
{
    int i;
    static const struct { UINT vk; const char *name; } fixed[] = {
        { VK_SPACE, "Space" }, { VK_RETURN, "Enter" }, { VK_TAB, "Tab" }, { VK_INSERT, "Insert" },
        { VK_HOME, "Home" }, { VK_END, "End" }, { VK_PRIOR, "Page Up" }, { VK_NEXT, "Page Down" },
        { VK_PAUSE, "Pause" }, { VK_SCROLL, "Scroll Lock" }
    };
    g_keyCount = 0;
    for (i = 0; i < (int)(sizeof(fixed) / sizeof(fixed[0])); i++) { g_keys[g_keyCount].vk = fixed[i].vk; lstrcpy(g_keys[g_keyCount].name, fixed[i].name); g_keyCount++; }
    for (i = 1; i <= 12; i++) { g_keys[g_keyCount].vk = VK_F1 + i - 1; wsprintf(g_keys[g_keyCount].name, "F%d", i); g_keyCount++; }
    for (i = 'A'; i <= 'Z'; i++) { g_keys[g_keyCount].vk = i; wsprintf(g_keys[g_keyCount].name, "%c", i); g_keyCount++; }
    for (i = '0'; i <= '9'; i++) { g_keys[g_keyCount].vk = i; wsprintf(g_keys[g_keyCount].name, "%c", i); g_keyCount++; }
}

void HotkeyText(UINT mods, UINT vk, char *out)
{
    int i;
    out[0] = 0;
    if (mods & MOD_CONTROL) lstrcat(out, "Ctrl+");
    if (mods & MOD_ALT) lstrcat(out, "Alt+");
    if (mods & MOD_SHIFT) lstrcat(out, "Shift+");
    for (i = 0; i < g_keyCount; i++) if (g_keys[i].vk == vk) { lstrcat(out, g_keys[i].name); return; }
    lstrcat(out, "?");
}

void LoadHotkey(void)
{
    HKEY k;
    DWORD v, type, size;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, APP_KEY, 0, KEY_READ, &k) != ERROR_SUCCESS) return;
    size = sizeof(v);
    if (RegQueryValueEx(k, "HotkeyModifiers", NULL, &type, (BYTE *)&v, &size) == ERROR_SUCCESS && type == REG_DWORD) g_mods = v;
    size = sizeof(v);
    if (RegQueryValueEx(k, "HotkeyKey", NULL, &type, (BYTE *)&v, &size) == ERROR_SUCCESS && type == REG_DWORD && v) g_vk = v;
    size = sizeof(v);
    if (RegQueryValueEx(k, "DimBackground", NULL, &type, (BYTE *)&v, &size) == ERROR_SUCCESS && type == REG_DWORD) g_dimOn = v != 0;
    size = sizeof(v);
    if (RegQueryValueEx(k, "RunningModifiers", NULL, &type, (BYTE *)&v, &size) == ERROR_SUCCESS && type == REG_DWORD) g_mods2 = v;
    size = sizeof(v);
    if (RegQueryValueEx(k, "RunningKey", NULL, &type, (BYTE *)&v, &size) == ERROR_SUCCESS && type == REG_DWORD) g_vk2 = v;   /* 0 = none */
    size = sizeof(v);
    if (RegQueryValueEx(k, "FavoriteHotkeys", NULL, &type, (BYTE *)&v, &size) == ERROR_SUCCESS && type == REG_DWORD) g_favKeys = v != 0;
    size = sizeof(v);
    if (RegQueryValueEx(k, "BackdropOpacity", NULL, &type, (BYTE *)&v, &size) == ERROR_SUCCESS && type == REG_DWORD && v <= 100) g_dimOpacity = (int)v;
    size = sizeof(v);
    if (RegQueryValueEx(k, "IconsPrograms", NULL, &type, (BYTE *)&v, &size) == ERROR_SUCCESS && type == REG_DWORD) g_iconsPrograms = v != 0;
    size = sizeof(v);
    if (RegQueryValueEx(k, "IconsRunning", NULL, &type, (BYTE *)&v, &size) == ERROR_SUCCESS && type == REG_DWORD) g_iconsRunning = v != 0;
    size = sizeof(v);
    if (RegQueryValueEx(k, "IconsStartup", NULL, &type, (BYTE *)&v, &size) == ERROR_SUCCESS && type == REG_DWORD) g_iconsStartup = v != 0;
    size = sizeof(v);
    if (RegQueryValueEx(k, "IconsWhenShown", NULL, &type, (BYTE *)&v, &size) == ERROR_SUCCESS && type == REG_DWORD) g_iconsLazy = v != 0;
    RegCloseKey(k);
}

void SaveHotkey(void)
{
    HKEY k; DWORD disp, v;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, APP_KEY, 0, NULL, 0, KEY_SET_VALUE, NULL, &k, &disp) != ERROR_SUCCESS) return;
    v = g_mods; RegSetValueEx(k, "HotkeyModifiers", 0, REG_DWORD, (BYTE *)&v, sizeof(v));
    v = g_vk;   RegSetValueEx(k, "HotkeyKey", 0, REG_DWORD, (BYTE *)&v, sizeof(v));
    v = g_dimOn; RegSetValueEx(k, "DimBackground", 0, REG_DWORD, (BYTE *)&v, sizeof(v));
    v = g_mods2; RegSetValueEx(k, "RunningModifiers", 0, REG_DWORD, (BYTE *)&v, sizeof(v));
    v = g_vk2;  RegSetValueEx(k, "RunningKey", 0, REG_DWORD, (BYTE *)&v, sizeof(v));
    v = g_favKeys; RegSetValueEx(k, "FavoriteHotkeys", 0, REG_DWORD, (BYTE *)&v, sizeof(v));
    v = g_dimOpacity; RegSetValueEx(k, "BackdropOpacity", 0, REG_DWORD, (BYTE *)&v, sizeof(v));
    v = g_iconsPrograms; RegSetValueEx(k, "IconsPrograms", 0, REG_DWORD, (BYTE *)&v, sizeof(v));
    v = g_iconsRunning; RegSetValueEx(k, "IconsRunning", 0, REG_DWORD, (BYTE *)&v, sizeof(v));
    v = g_iconsStartup; RegSetValueEx(k, "IconsStartup", 0, REG_DWORD, (BYTE *)&v, sizeof(v));
    v = g_iconsLazy; RegSetValueEx(k, "IconsWhenShown", 0, REG_DWORD, (BYTE *)&v, sizeof(v));
    RegCloseKey(k);
}

void SetHint(void)
{
    char key[48];
    HotkeyText(g_mods, g_vk, key);
    if (g_view == VIEW_RUNNING)
        lstrcpy(g_status, "Enter: switch to  |  Del: close  |  Shift+Del: end now  |  F1: details  |  Tab: startup  |  Esc: close");
    else if (g_view == VIEW_STARTUP)
        lstrcpy(g_status, "Space or Enter: switch off or on, from the next time Windows starts  |  Tab: programs  |  Esc: close");
    else
        wsprintf(g_status, "%s: open  |  Enter: start  |  Right-click: more  |  Drag: pin or move  |  Tab: running  |  F1: help", key);
}

int ApplyHotkey(UINT mods, UINT vk)
{
    char text[64], msg[128];
    UnregisterHotKey(g_main, 1);
    if (!RegisterHotKey(g_main, 1, mods, vk)) {
        HotkeyText(mods, vk, text);
        wsprintf(msg, "Could not register hotkey %s (error %lu)", text, GetLastError());
        Log(msg);
        g_hotkeyOk = 0;
        return 0;
    }
    g_mods = mods; g_vk = vk; g_hotkeyOk = 1;
    HotkeyText(mods, vk, text);
    wsprintf(msg, "Hotkey registered: %s", text);
    Log(msg);
    SetHint();
    UpdateTray(0);
    return 1;
}

/* The second hotkey opens the panel on the running programs. Key 0 = none. */
int ApplyHotkey2(UINT mods, UINT vk)
{
    char text[64], msg[128];
    UnregisterHotKey(g_main, 2);
    g_hotkey2Ok = 0;
    if (!vk) { g_mods2 = mods; g_vk2 = 0; Log("Hotkey for running programs: none"); return 1; }
    HotkeyText(mods, vk, text);
    if (!RegisterHotKey(g_main, 2, mods, vk)) {
        wsprintf(msg, "Could not register hotkey %s for running programs (error %lu)", text, GetLastError());
        Log(msg);
        return 0;
    }
    g_mods2 = mods; g_vk2 = vk; g_hotkey2Ok = 1;
    wsprintf(msg, "Hotkey for running programs registered: %s", text);
    Log(msg);
    return 1;
}

/* Ctrl+Alt+1 to 5 start the favorites. On keyboards where AltGr and a digit
 * type a character, such as @ on a Spanish one, that digit is left alone:
 * to Windows, AltGr is Ctrl and Alt together. */
void ApplyFavKeys(void)
{
    BYTE state[256];
    WORD ch;
    char msg[160];
    int i, taken = 0, typed = 0;
    for (i = 0; i < MAX_PINS; i++) UnregisterHotKey(g_main, HOTKEY_FAV + i);
    g_favKeysOn = 0;
    if (!g_favKeys) { Log("Hotkeys for favorites: off"); return; }
    ZeroMemory(state, sizeof(state));
    state[VK_CONTROL] = state[VK_MENU] = 0x80;
    for (i = 0; i < MAX_PINS; i++) {
        ch = 0;
        if (ToAscii('1' + i, MapVirtualKey('1' + i, 0), state, &ch, 0) > 0) { typed++; continue; }
        if (RegisterHotKey(g_main, HOTKEY_FAV + i, MOD_CONTROL | MOD_ALT, '1' + i)) g_favKeysOn |= 1 << i; else taken++;
    }
    wsprintf(msg, "Hotkeys for favorites: %d of 5 registered, %d type a character on this keyboard, %d taken by other programs",
             MAX_PINS - typed - taken, typed, taken);
    Log(msg);
}
