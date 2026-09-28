/* main.c - Searchlight 98, a quick launcher for Windows 95/98.
 *
 * One small native program. It sits in the system tray, registers a hotkey
 * (default Ctrl+Space) and shows a floating panel over the desktop with a
 * search box, Favorites, Recent programs and every Start Menu program sorted
 * into groups. Tab shows the running programs instead: switch to one, close
 * it or end it. Tab again shows what starts together with Windows.
 *
 *  SLIGHT98.EXE                   start (or, if already running, open the panel)
 *  SLIGHT98.EXE /quit             stop the running copy
 *  SLIGHT98.EXE /uninstall        stop it and remove it from startup
 *  SLIGHT98.EXE /selftest         read TESTS.TXT, write SELFTEST.OUT, exit
 *  SLIGHT98.EXE /shot FILE [TEXT] draw the panel into a .BMP file and exit
 *                                 (TEXT: words to search, /running, /startup, /hover,
 *                                 /recent, /drag, /group, /hotkeys, /about, /tab)
 *
 * The last two never touch the tray, the hotkey or the screen. They exist so
 * the program can be tested on a development machine.
 *
 * This file starts the program and keeps the tray icon and the window that
 * receives the hotkeys. slight98.h names the other files and what is in them.
 *
 * Built with Tiny C Compiler. Uses only APIs present on Windows 95/98.
 */
#include "slight98.h"

static const MYGUID CLSID_ShellLink_ = { 0x00021401, 0, 0, { 0xC0, 0, 0, 0, 0, 0, 0, 0x46 } };
static const MYGUID IID_IShellLinkA_ = { 0x000214EE, 0, 0, { 0xC0, 0, 0, 0, 0, 0, 0, 0x46 } };
static const MYGUID IID_IPersistFile_ = { 0x0000010B, 0, 0, { 0xC0, 0, 0, 0, 0, 0, 0, 0x46 } };

static const char CLASS_MAIN[]  = "Searchlight98Daemon";
static const char RUN_KEY[]     = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static UINT   g_taskbarMsg = 0;

/* ------------------------------------------------------------------------
 * Tray icon and its menu
 * --------------------------------------------------------------------- */

void UpdateTray(int add)
{
    MY_NOTIFYICONDATAA nid;
    char text[64];
    if (!g_main || g_testMode) return;
    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_main;
    nid.uID = 1;
    nid.uFlags = MY_NIF_MESSAGE | MY_NIF_ICON | MY_NIF_TIP;
    nid.uCallbackMessage = WM_TRAY;
    nid.hIcon = g_appIcon;
    HotkeyText(g_mods, g_vk, text);
    if (g_offAt) wsprintf(nid.szTip, "Windows %s at %s", g_offAction == 1 ? "shuts down" : "restarts", g_offTime);
    else if (g_hotkeyOk) wsprintf(nid.szTip, APP_NAME " (%s)", text);
    else lstrcpy(nid.szTip, APP_NAME " (no hotkey)");
    Shell_NotifyIconA(add ? MY_NIM_ADD : MY_NIM_MODIFY, &nid);
}

static void RemoveTray(void)
{
    MY_NOTIFYICONDATAA nid;
    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_main;
    nid.uID = 1;
    Shell_NotifyIconA(MY_NIM_DELETE, &nid);
}

static void TrayMenu(void)
{
    HMENU m = CreatePopupMenu();
    POINT pt;
    char open[64], key[48];
    HotkeyText(g_mods, g_vk, key);
    wsprintf(open, "&Open Searchlight\t%s", key);
    AppendMenu(m, MF_STRING, ID_OPEN, open);
    if (g_hotkey2Ok) { HotkeyText(g_mods2, g_vk2, key); wsprintf(open, "Running &Programs\t%s", key); }
    else lstrcpy(open, "Running &Programs");
    AppendMenu(m, MF_STRING, ID_OPENRUN, open);
    AppendMenu(m, MF_STRING, ID_OPENSTART, "&Startup Programs");
    if (g_offAt) {
        wsprintf(open, "&Cancel the %s at %s", g_offAction == 1 ? "Shut Down" : "Restart", g_offTime);
        AppendMenu(m, MF_SEPARATOR, 0, NULL);
        AppendMenu(m, MF_STRING, ID_NOTIMER, open);
    }
    AppendMenu(m, MF_SEPARATOR, 0, NULL);
    AppendMenu(m, MF_STRING, ID_RESCAN, "&Rescan Programs and Documents");
    AppendMenu(m, MF_STRING, ID_SETTINGS, "Change &Hotkeys...");
    AppendMenu(m, MF_STRING, ID_OPTIONS, "O&ptions...");
    AppendMenu(m, MF_SEPARATOR, 0, NULL);
    AppendMenu(m, MF_STRING, ID_HELP, "Read &Me");
    AppendMenu(m, MF_STRING, ID_ABOUT, "&About " APP_NAME);
    AppendMenu(m, MF_STRING, ID_EXIT, "E&xit");
    SetMenuDefaultItem(m, ID_OPEN, FALSE);
    GetCursorPos(&pt);
    SetForegroundWindow(g_main);
    TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_main, NULL);
    PostMessage(g_main, WM_NULL, 0, 0);
    DestroyMenu(m);
}

/* ------------------------------------------------------------------------
 * Background window
 * --------------------------------------------------------------------- */

static void SetStartup(int on)
{
    HKEY k; DWORD disp;
    char exe[MAX_PATH];
    if (RegCreateKeyEx(HKEY_CURRENT_USER, RUN_KEY, 0, NULL, 0, KEY_SET_VALUE, NULL, &k, &disp) != ERROR_SUCCESS) return;
    if (on) { GetModuleFileName(NULL, exe, MAX_PATH); RegSetValueEx(k, APP_NAME, 0, REG_SZ, (BYTE *)exe, lstrlen(exe) + 1); }
    else RegDeleteValue(k, APP_NAME);
    RegCloseKey(k);
}

static LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == g_taskbarMsg && g_taskbarMsg) { UpdateTray(1); return 0; }     /* Explorer restarted */
    switch (msg) {
    case WM_HOTKEY:
        if (wp >= HOTKEY_FAV && wp < HOTKEY_FAV + MAX_PINS) FavoriteHotkey((int)wp - HOTKEY_FAV);
        else if (wp == 2) RunningHotkey();
        else TogglePanel();
        return 0;
    case WM_TIMER:
        if (wp == 5) CountdownTick();
        if (wp == 6) EagerTick();
        return 0;
    case WM_TRAY:
        if (lp == WM_RBUTTONUP) TrayMenu();
        else if (lp == WM_LBUTTONDBLCLK) ShowPanel(VIEW_PROGRAMS);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_OPEN:     ShowPanel(VIEW_PROGRAMS); return 0;
        case ID_OPENRUN:  ShowPanel(VIEW_RUNNING); return 0;
        case ID_OPENSTART: ShowPanel(VIEW_STARTUP); return 0;
        case ID_HELP:     Help(); return 0;
        case ID_NOTIMER:  StopCountdown(); return 0;
        case ID_OPTIONS:  OpenOptions(); return 0;
        case ID_RESCAN:   Rescan(1); return 0;
        case ID_SETTINGS: OpenSettings(); return 0;

        case ID_ABOUT:    About(); return 0;
        case ID_EXIT:     DestroyWindow(hwnd); return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        UnregisterHotKey(hwnd, 1);
        UnregisterHotKey(hwnd, 2);
        g_favKeys = 0;
        ApplyFavKeys();
        KillTimer(hwnd, 5);
        KillTimer(hwnd, 6);
        StopStats();
        RemoveTray();
        if (g_settings) DestroyWindow(g_settings);
        if (g_options) DestroyWindow(g_options);
        if (g_about) DestroyWindow(g_about);
        Log("Stopped");
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ------------------------------------------------------------------------
 * Start-up
 * --------------------------------------------------------------------- */

static void RegisterClasses(void)
{
    WNDCLASS wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = MainProc; wc.hInstance = g_inst; wc.lpszClassName = CLASS_MAIN;
    RegisterClass(&wc);
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = PanelProc; wc.hInstance = g_inst; wc.lpszClassName = CLASS_PANEL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = BackProc; wc.hInstance = g_inst; wc.lpszClassName = CLASS_BACK;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = SettingsProc; wc.hInstance = g_inst; wc.lpszClassName = CLASS_SET;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClass(&wc);
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = AboutProc; wc.hInstance = g_inst; wc.lpszClassName = CLASS_ABOUT;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClass(&wc);
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = OptionsProc; wc.hInstance = g_inst; wc.lpszClassName = CLASS_OPT;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClass(&wc);
}

static void StartCom(void)
{
    if (CoInitialize(NULL) < 0) return;
    if (CoCreateInstance(&CLSID_ShellLink_, NULL, 1, &IID_IShellLinkA_, (void **)&g_link) < 0) { g_link = NULL; return; }
    if (g_link->v->QueryInterface(g_link, &IID_IPersistFile_, (void **)&g_linkFile) < 0) g_linkFile = NULL;
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    HWND existing;
    MSG msg;
    char *slash, path[MAX_PATH], sw[32], arg1[MAX_PATH], arg2[120], *p;
    int n;
    (void)prev; (void)show;
    g_inst = inst;

    GetModuleFileName(NULL, g_dir, MAX_PATH);
    slash = strrchr(g_dir, '\\');
    if (slash) *slash = 0;

    /* Command line: /switch ["file"] [rest of line] */
    sw[0] = arg1[0] = arg2[0] = 0;
    p = cmd ? cmd : (char *)"";
    while (*p == ' ' || *p == '\t' || *p == '"') p++;
    for (n = 0; *p && *p != ' ' && *p != '\t' && *p != '"' && n < 31; ) sw[n++] = *p++;
    sw[n] = 0;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '"') { p++; for (n = 0; *p && *p != '"' && n < MAX_PATH - 1; ) arg1[n++] = *p++; if (*p == '"') p++; }
    else { for (n = 0; *p && *p != ' ' && *p != '\t' && n < MAX_PATH - 1; ) arg1[n++] = *p++; }
    arg1[n] = 0;
    while (*p == ' ' || *p == '\t') p++;
    CopyN(arg2, p, sizeof(arg2));

    g_defIcon = LoadIcon(NULL, IDI_APPLICATION);
    BuildKeyList();

    if (lstrcmpi(sw, "/selftest") == 0 || lstrcmpi(sw, "/shot") == 0) {
        int r;
        g_testMode = 1;
        RegisterClasses();
        StartCom();
        g_appIcon = EmbeddedIcon(16);
        r = (lstrcmpi(sw, "/selftest") == 0) ? SelfTest() : Shot(arg1, arg2);
        return r;
    }

    existing = FindWindow(CLASS_MAIN, NULL);
    if (lstrcmpi(sw, "/quit") == 0 || lstrcmpi(sw, "/uninstall") == 0) {
        if (existing) PostMessage(existing, WM_CLOSE, 0, 0);
        if (lstrcmpi(sw, "/uninstall") == 0) SetStartup(0);
        return 0;
    }
    if (existing) { PostMessage(existing, WM_COMMAND, ID_OPEN, 0); return 0; }

    PathIn(path, "SLIGHT98.LOG");
    g_log = fopen(path, "w");
    Log(APP_NAME " " APP_VERSION " starting");
    Log(g_dir);

    RegisterClasses();
    StartCom();
    Log(g_link && g_linkFile ? "Shortcut reader ready" : "Shortcut reader not available: icons come from the shortcut files");

    g_appIcon = EmbeddedIcon(16);
    g_appIconLarge = EmbeddedIcon(32);

    g_main = CreateWindow(CLASS_MAIN, APP_NAME " background", 0, 0, 0, 0, 0, NULL, NULL, inst, NULL);
    if (!g_main) { Log("Could not create the background window"); return 1; }
    g_taskbarMsg = RegisterWindowMessage("TaskbarCreated");

    CreatePanel();
    LoadState();
    LoadHotkey();
    Scan();
    FillLists();
    SaveState();
    SetHint();
    Layout();

    if (!ApplyHotkey(g_mods, g_vk)) {
        char text[64], m[240];
        HotkeyText(g_mods, g_vk, text);
        UpdateTray(1);
        wsprintf(m, "The hotkey %s is used by Windows or another program.\n\nChoose another one with Change Hotkeys in the Searchlight 98 tray menu.", text);
        MessageBox(NULL, m, APP_NAME, MB_OK | MB_ICONINFORMATION);
    } else UpdateTray(1);
    Log("Tray icon added");
    ApplyHotkey2(g_mods2, g_vk2);          /* if it is taken, the tray menu and Tab still open the list */
    ApplyFavKeys();
    StartStats();
    StartEager();

    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        if (g_settings && IsDialogMessage(g_settings, &msg)) continue;
        if (g_about && IsDialogMessage(g_about, &msg)) continue;
        if (g_options && IsDialogMessage(g_options, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    FreeIcons(g_items, g_count);
    if (g_linkFile) g_linkFile->v->Release(g_linkFile);
    if (g_link) g_link->v->Release(g_link);
    CoUninitialize();
    if (g_log) fclose(g_log);
    return 0;
}
