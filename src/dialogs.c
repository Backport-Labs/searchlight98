/* dialogs.c - Searchlight 98: the Hotkeys, Options and About boxes. */
#include "slight98.h"

/* ------------------------------------------------------------------------
 * Change Hotkey box
 * --------------------------------------------------------------------- */

static HWND Control(HWND parent, const char *cls, const char *text, DWORD style, int x, int y, int w, int h, int id)
{
    HWND c = CreateWindowEx(0, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, parent, (HMENU)id, g_inst, NULL);
    SendMessage(c, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);
    return c;
}

/* The box has the same four controls twice. "second" is 0 for the hotkey that
 * opens Searchlight and ID_SECOND for the one that opens the running programs,
 * whose list of keys starts with "(none)". */
static void FillSettings(HWND dlg, int second, UINT mods, UINT vk)
{
    int i, first = second ? 1 : 0;
    CheckDlgButton(dlg, ID_CTRL + second, (mods & MOD_CONTROL) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dlg, ID_ALT + second, (mods & MOD_ALT) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dlg, ID_SHIFT + second, (mods & MOD_SHIFT) ? BST_CHECKED : BST_UNCHECKED);
    if (second) SendMessage(GetDlgItem(dlg, ID_KEY + second), CB_SETCURSEL, 0, 0);
    for (i = 0; i < g_keyCount && vk; i++)
        if (g_keys[i].vk == vk) SendMessage(GetDlgItem(dlg, ID_KEY + second), CB_SETCURSEL, i + first, 0);
}

/* Reads one hotkey from the box. Returns 0 after telling what is wrong with it. */
static int ReadSettings(HWND dlg, int second, UINT *mods, UINT *vk)
{
    int sel = (int)SendMessage(GetDlgItem(dlg, ID_KEY + second), CB_GETCURSEL, 0, 0);
    *mods = 0; *vk = 0;
    if (IsDlgButtonChecked(dlg, ID_CTRL + second)) *mods |= MOD_CONTROL;
    if (IsDlgButtonChecked(dlg, ID_ALT + second)) *mods |= MOD_ALT;
    if (IsDlgButtonChecked(dlg, ID_SHIFT + second)) *mods |= MOD_SHIFT;
    if (sel < 0) return 0;
    if (second && sel == 0) return 1;                   /* (none) */
    *vk = g_keys[sel - (second ? 1 : 0)].vk;
    if (!*mods && !(*vk >= VK_F1 && *vk <= VK_F12) && *vk != VK_PAUSE && *vk != VK_SCROLL) {
        MessageBox(dlg, "Please tick Ctrl, Alt or Shift.\nA letter or Space on its own would stop you typing it anywhere else.", APP_NAME, MB_OK | MB_ICONINFORMATION);
        return 0;
    }
    return 1;
}

static void SaveFromSettings(HWND dlg)
{
    UINT mods, vk, mods2, vk2, oldMods = g_mods, oldVk = g_vk, oldMods2 = g_mods2, oldVk2 = g_vk2;
    char text[64], msg[200];
    if (!ReadSettings(dlg, 0, &mods, &vk) || !ReadSettings(dlg, ID_SECOND, &mods2, &vk2)) return;
    if (vk2 && vk2 == vk && mods2 == mods) {
        MessageBox(dlg, "Both hotkeys are the same.\nPlease choose a different one for the running programs, or (none).", APP_NAME, MB_OK | MB_ICONINFORMATION);
        return;
    }
    UnregisterHotKey(g_main, 2);
    if (!ApplyHotkey(mods, vk) || !ApplyHotkey2(mods2, vk2)) {
        HotkeyText(g_hotkeyOk ? mods2 : mods, g_hotkeyOk ? vk2 : vk, text);
        wsprintf(msg, "%s is already used by Windows or another program.\nPlease choose a different combination.", text);
        MessageBox(dlg, msg, APP_NAME, MB_OK | MB_ICONEXCLAMATION);
        ApplyHotkey(oldMods, oldVk);
        ApplyHotkey2(oldMods2, oldVk2);
        return;
    }
    SaveHotkey();
    DestroyWindow(dlg);
}

/* One group of the box: a line of text, three tick boxes and the list of keys. */
static void HotkeyGroup(HWND hwnd, int second, int y, const char *title, const char *text)
{
    HWND combo;
    int i;
    Control(hwnd, "BUTTON", title, BS_GROUPBOX, 10, y, 314, 104, 0);
    Control(hwnd, "STATIC", text, 0, 22, y + 20, 290, 28, 0);
    Control(hwnd, "BUTTON", "Ctrl", BS_AUTOCHECKBOX | WS_TABSTOP, 22, y + 58, 50, 20, ID_CTRL + second);
    Control(hwnd, "BUTTON", "Alt", BS_AUTOCHECKBOX | WS_TABSTOP, 76, y + 58, 44, 20, ID_ALT + second);
    Control(hwnd, "BUTTON", "Shift", BS_AUTOCHECKBOX | WS_TABSTOP, 124, y + 58, 52, 20, ID_SHIFT + second);
    Control(hwnd, "STATIC", "+", 0, 180, y + 61, 10, 16, 0);
    combo = Control(hwnd, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 196, y + 57, 116, 200, ID_KEY + second);
    if (second) SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)"(none)");
    for (i = 0; i < g_keyCount; i++) SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)g_keys[i].name);
}

LRESULT CALLBACK SettingsProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        HotkeyGroup(hwnd, 0, 8, "Open Searchlight",
                    "Press this key combination anywhere in Windows to open Searchlight 98:");
        HotkeyGroup(hwnd, ID_SECOND, 118, "Running programs",
                    "This one opens the running programs. Press it again to step to the next one:");
        Control(hwnd, "BUTTON", "&Default", BS_PUSHBUTTON | WS_TABSTOP, 10, 234, 75, 23, ID_DEFAULT);
        Control(hwnd, "BUTTON", "OK", BS_DEFPUSHBUTTON | WS_TABSTOP, 168, 234, 75, 23, IDOK);
        Control(hwnd, "BUTTON", "Cancel", BS_PUSHBUTTON | WS_TABSTOP, 249, 234, 75, 23, IDCANCEL);
        FillSettings(hwnd, 0, g_mods, g_vk);
        FillSettings(hwnd, ID_SECOND, g_mods2, g_vk2);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDOK:       SaveFromSettings(hwnd); return 0;
        case IDCANCEL:   DestroyWindow(hwnd); return 0;
        case ID_DEFAULT:
            FillSettings(hwnd, 0, MOD_CONTROL, VK_SPACE);
            FillSettings(hwnd, ID_SECOND, MOD_CONTROL | MOD_SHIFT, VK_SPACE);
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        g_settings = NULL;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ------------------------------------------------------------------------
 * About box
 *
 * Shows the version, the registered owner of this copy of Windows, the
 * physical memory and the free system resources.
 * --------------------------------------------------------------------- */

static void RegText(const char *value, char *out, int size)
{
    HKEY k;
    DWORD type, len = size - 1;
    out[0] = 0;
    if (g_testMode) { if (strcmp(value, "RegisteredOwner") == 0) lstrcpyn(out, "A Windows User", size); return; }
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion", 0, KEY_READ, &k) != ERROR_SUCCESS) return;
    if (RegQueryValueEx(k, value, NULL, &type, (BYTE *)out, &len) != ERROR_SUCCESS || type != REG_SZ) out[0] = 0;
    out[size - 1] = 0;
    RegCloseKey(k);
}

/* 1048576 becomes 1,048,576 */
static void Thousands(DWORD n, char *out)
{
    char raw[16];
    int len, i, o = 0;
    wsprintf(raw, "%lu", n);
    len = lstrlen(raw);
    for (i = 0; i < len; i++) {
        if (i && (len - i) % 3 == 0) out[o++] = ',';
        out[o++] = raw[i];
    }
    out[o] = 0;
}

static void AboutLine(HDC dc, int x, int *y, const char *text)
{
    RECT r;
    SetRect(&r, x, *y, ABOUT_W - 14, *y + 16);
    DrawText(dc, text, -1, &r, DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    *y += 16;
}

static void PaintAbout(HDC dc)
{
    RECT r;
    char text[200], who[100], firm[100], number[24];
    int y = 74;
    if (!g_appIconLarge) g_appIconLarge = EmbeddedIcon(32);
    SetRect(&r, 0, 0, ABOUT_W, 62);
    Gradient(dc, &r);
    DrawIconEx(dc, 16, 15, g_appIconLarge, 32, 32, 0, NULL, DI_NORMAL);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    SelectObject(dc, g_fontEdit);
    SetRect(&r, 62, 10, ABOUT_W - 10, 34);
    DrawText(dc, APP_NAME, -1, &r, DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, g_font);
    SetRect(&r, 63, 36, ABOUT_W - 10, 54);
    DrawText(dc, "Find and start any program on your computer. Fast.", -1, &r, DT_SINGLELINE | DT_NOPREFIX);

    SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    AboutLine(dc, 16, &y, "Version " APP_VERSION " for Windows 95 and Windows 98");
    AboutLine(dc, 16, &y, "Copyright (C) 2026 Backport Labs");
    AboutLine(dc, 16, &y, "Free and open source software, released under the MIT License.");
    y += 10;
    AboutLine(dc, 16, &y, "This product is licensed to:");
    RegText("RegisteredOwner", who, sizeof(who));
    RegText("RegisteredOrganization", firm, sizeof(firm));
    AboutLine(dc, 32, &y, who[0] ? who : "You");
    if (firm[0]) AboutLine(dc, 32, &y, firm);
    y += 8;
    SetRect(&r, 16, y, ABOUT_W - 16, y + 2);
    DrawEdge(dc, &r, EDGE_ETCHED, BF_TOP);
    y += 10;
    Thousands(g_memTotalK, number);
    wsprintf(text, "Physical memory available to Windows:    %s KB", number);
    if (g_memTotalK) AboutLine(dc, 16, &y, text);
    wsprintf(text, "System resources:    %d%% free", g_resFree);
    if (g_resFree >= 0) AboutLine(dc, 16, &y, text);
}

LRESULT CALLBACK AboutProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        Control(hwnd, "BUTTON", "OK", BS_DEFPUSHBUTTON | WS_TABSTOP, ABOUT_W - 91, ABOUT_H - 35, 75, 23, IDOK);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        PaintAbout(dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_PRINTCLIENT:
        PaintAbout((HDC)wp);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL) DestroyWindow(hwnd);
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        g_about = NULL;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* Makes the Hotkeys box or the About box. Test mode keeps it off the screen. */
HWND MakeBox(const char *cls, const char *title, int cw, int ch)
{
    RECT rc;
    int w, h;
    SetRect(&rc, 0, 0, cw, ch);
    AdjustWindowRect(&rc, WS_CAPTION | WS_SYSMENU, FALSE);
    w = rc.right - rc.left; h = rc.bottom - rc.top;
    if (g_testMode)
        return CreateWindowEx(WS_EX_DLGMODALFRAME, cls, title, WS_CAPTION | WS_SYSMENU, -4000, -3000, w, h, NULL, NULL, g_inst, NULL);
    return CreateWindowEx(WS_EX_DLGMODALFRAME, cls, title, WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        (GetSystemMetrics(SM_CXSCREEN) - w) / 2, (GetSystemMetrics(SM_CYSCREEN) - h) / 2, w, h, NULL, NULL, g_inst, NULL);
}

void About(void)
{
    if (g_about) { SetForegroundWindow(g_about); return; }
    HidePanel();
    ReadStats();
    g_about = MakeBox(CLASS_ABOUT, "About " APP_NAME, ABOUT_W, ABOUT_H);
    if (g_about && !g_testMode) SetForegroundWindow(g_about);
}

/* ------------------------------------------------------------------------
 * Options box
 * --------------------------------------------------------------------- */

static void ShowOpacity(HWND dlg, int percent)
{
    char text[16];
    wsprintf(text, "%d%%", percent);
    SetDlgItemText(dlg, ID_O_PERCENT, text);
    SetScrollPos(GetDlgItem(dlg, ID_O_OPACITY), SB_CTL, percent, TRUE);
}

static void FillOptions(HWND dlg, int iconsP, int iconsR, int iconsS, int lazy, int dim, int opacity, int favKeys)
{
    CheckDlgButton(dlg, ID_O_ICONP, iconsP ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dlg, ID_O_ICONR, iconsR ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dlg, ID_O_ICONS, iconsS ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dlg, ID_O_LAZY, lazy ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dlg, ID_O_DIM, dim ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dlg, ID_O_FAVKEYS, favKeys ? BST_CHECKED : BST_UNCHECKED);
    ShowOpacity(dlg, opacity);
    EnableWindow(GetDlgItem(dlg, ID_O_OPACITY), dim);
    EnableWindow(GetDlgItem(dlg, ID_O_LAZY), iconsP);
}

static void SaveFromOptions(HWND dlg)
{
    int hadIcons = g_iconsPrograms;
    g_iconsPrograms = IsDlgButtonChecked(dlg, ID_O_ICONP) == BST_CHECKED;
    g_iconsRunning = IsDlgButtonChecked(dlg, ID_O_ICONR) == BST_CHECKED;
    g_iconsStartup = IsDlgButtonChecked(dlg, ID_O_ICONS) == BST_CHECKED;
    g_iconsLazy = IsDlgButtonChecked(dlg, ID_O_LAZY) == BST_CHECKED;
    g_dimOn = IsDlgButtonChecked(dlg, ID_O_DIM) == BST_CHECKED;
    g_dimOpacity = GetScrollPos(GetDlgItem(dlg, ID_O_OPACITY), SB_CTL);
    g_favKeys = IsDlgButtonChecked(dlg, ID_O_FAVKEYS) == BST_CHECKED;
    g_dimSlow = 0;
    if (hadIcons && !g_iconsPrograms) FreeIcons(g_items, g_count);      /* the memory they took is given back */
    ApplyFavKeys();
    SaveHotkey();
    StartEager();
    DestroyWindow(dlg);
}

LRESULT CALLBACK OptionsProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE: {
        HWND bar;
        Control(hwnd, "BUTTON", "Icons", BS_GROUPBOX, 10, 8, 340, 118, 0);
        Control(hwnd, "BUTTON", "Show icons in the &programs and results", BS_AUTOCHECKBOX | WS_TABSTOP, 22, 28, 310, 18, ID_O_ICONP);
        Control(hwnd, "BUTTON", "Show icons in the &running programs", BS_AUTOCHECKBOX | WS_TABSTOP, 22, 48, 310, 18, ID_O_ICONR);
        Control(hwnd, "BUTTON", "Show icons in the &startup programs", BS_AUTOCHECKBOX | WS_TABSTOP, 22, 68, 310, 18, ID_O_ICONS);
        Control(hwnd, "BUTTON", "&Load icons only when they come into view (uses less memory)", BS_AUTOCHECKBOX | WS_TABSTOP, 22, 96, 320, 18, ID_O_LAZY);
        Control(hwnd, "BUTTON", "Backdrop", BS_GROUPBOX, 10, 132, 340, 100, 0);
        Control(hwnd, "BUTTON", "&Darken the rest of the screen while Searchlight is open", BS_AUTOCHECKBOX | WS_TABSTOP, 22, 152, 320, 18, ID_O_DIM);
        Control(hwnd, "STATIC", "&Opacity:", 0, 22, 181, 56, 16, 0);
        bar = Control(hwnd, "SCROLLBAR", "", SBS_HORZ | WS_TABSTOP, 80, 179, 210, 18, ID_O_OPACITY);
        SetScrollRange(bar, SB_CTL, 0, 100, FALSE);
        Control(hwnd, "STATIC", "", SS_RIGHT, 296, 181, 42, 16, ID_O_PERCENT);
        Control(hwnd, "STATIC", "0% leaves the screen as it is. 100% makes it black.", 0, 22, 206, 320, 16, 0);
        Control(hwnd, "BUTTON", "Favorites", BS_GROUPBOX, 10, 238, 340, 48, 0);
        Control(hwnd, "BUTTON", "&Ctrl+Alt+1 to 5 start the favorites from any program", BS_AUTOCHECKBOX | WS_TABSTOP, 22, 258, 320, 18, ID_O_FAVKEYS);
        Control(hwnd, "BUTTON", "De&fault", BS_PUSHBUTTON | WS_TABSTOP, 10, OPT_H - 33, 75, 23, ID_DEFAULT);
        Control(hwnd, "BUTTON", "OK", BS_DEFPUSHBUTTON | WS_TABSTOP, OPT_W - 166, OPT_H - 33, 75, 23, IDOK);
        Control(hwnd, "BUTTON", "Cancel", BS_PUSHBUTTON | WS_TABSTOP, OPT_W - 85, OPT_H - 33, 75, 23, IDCANCEL);
        FillOptions(hwnd, g_iconsPrograms, g_iconsRunning, g_iconsStartup, g_iconsLazy, g_dimOn, g_dimOpacity, g_favKeys);
        return 0;
    }
    case WM_HSCROLL: {
        HWND bar = (HWND)lp;
        int at = GetScrollPos(bar, SB_CTL);
        switch (LOWORD(wp)) {
        case SB_LINELEFT:      at -= 1; break;
        case SB_LINERIGHT:     at += 1; break;
        case SB_PAGELEFT:      at -= 10; break;
        case SB_PAGERIGHT:     at += 10; break;
        case SB_LEFT:          at = 0; break;
        case SB_RIGHT:         at = 100; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: at = (short)HIWORD(wp); break;
        }
        if (at < 0) at = 0;
        if (at > 100) at = 100;
        ShowOpacity(hwnd, at);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDOK:       SaveFromOptions(hwnd); return 0;
        case IDCANCEL:   DestroyWindow(hwnd); return 0;
        case ID_DEFAULT: FillOptions(hwnd, 1, 1, 1, 1, 1, DIM_DEFAULT, 1); return 0;
        case ID_O_DIM:   EnableWindow(GetDlgItem(hwnd, ID_O_OPACITY), IsDlgButtonChecked(hwnd, ID_O_DIM) == BST_CHECKED); return 0;
        case ID_O_ICONP: EnableWindow(GetDlgItem(hwnd, ID_O_LAZY), IsDlgButtonChecked(hwnd, ID_O_ICONP) == BST_CHECKED); return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        g_options = NULL;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

void OpenOptions(void)
{
    if (g_options) { SetForegroundWindow(g_options); return; }
    HidePanel();
    g_options = MakeBox(CLASS_OPT, APP_NAME " Options", OPT_W, OPT_H);
    if (g_options && !g_testMode) SetForegroundWindow(g_options);
}

void OpenSettings(void)
{
    if (g_settings) { SetForegroundWindow(g_settings); return; }
    HidePanel();
    g_settings = MakeBox(CLASS_SET, APP_NAME " Hotkeys", 334, 267);
    if (g_settings) SetForegroundWindow(g_settings);
}
