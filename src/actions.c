/* actions.c - Searchlight 98: what happens when something on the panel is chosen. */
#include "slight98.h"

static char   g_closingName[100];

/* ------------------------------------------------------------------------
 * Finding the program that removes a program
 *
 * Add/Remove Programs keeps its list in the registry. The entry belongs to
 * a program if its command mentions the folder the program is in, or
 * failing that, if its name has the program's name in it.
 * --------------------------------------------------------------------- */

/* Everything in lower case. 2 = the folder matches, 1 = the name, 0 = no. */
int UninstallMatches(const char *dir, const char *lname, const char *display, const char *command)
{
    char word[104];
    if (lstrlen(dir) > 3 && strstr(command, dir)) return 2;
    if (lstrlen(lname) < 4) return 0;
    wsprintf(word, "=%.99s", lname);
    return MatchWord(display, word, lstrlen(word)) ? 1 : 0;
}

/* The folder a program is in, lower case, or nothing if that folder says
 * nothing about it: the Windows folder, Program Files itself, a drive. */
static void ProgramFolder(const char *target, char *dir)
{
    char win[MAX_PATH], *slash;
    CopyN(dir, target, MAX_PATH);
    Lower(dir);
    slash = strrchr(dir, '\\');
    if (!slash) { dir[0] = 0; return; }
    *slash = 0;
    GetWindowsDirectory(win, MAX_PATH);
    Lower(win);
    if (lstrlen(dir) <= 3 || strncmp(dir, win, lstrlen(win)) == 0) { dir[0] = 0; return; }
    slash = strrchr(dir, '\\');
    if (slash && (strcmp(slash, "\\program files") == 0 || strcmp(slash, "\\archivos de programa") == 0)) dir[0] = 0;
}

static int FindUninstall(const ITEM *it, char *name, char *command)
{
    char target[MAX_PATH], dir[MAX_PATH], sub[200], display[200], cmd[MAX_PATH], ldisplay[200], lcmd[MAX_PATH];
    HKEY k, e;
    DWORD i, size, type;
    int best = 0, m;
    if (it->kind != 0 || g_testMode) return 0;
    LinkTarget(it->path, target);
    ProgramFolder(target, dir);
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall", 0, KEY_READ, &k) != ERROR_SUCCESS) return 0;
    for (i = 0; i < 500 && best < 2; i++) {
        if (RegEnumKey(k, i, sub, sizeof(sub)) != ERROR_SUCCESS) break;
        if (RegOpenKeyEx(k, sub, 0, KEY_READ, &e) != ERROR_SUCCESS) continue;
        display[0] = cmd[0] = 0;
        size = sizeof(display) - 1;
        if (RegQueryValueEx(e, "DisplayName", NULL, &type, (BYTE *)display, &size) != ERROR_SUCCESS || type != REG_SZ) display[0] = 0;
        size = sizeof(cmd) - 1;
        if (RegQueryValueEx(e, "UninstallString", NULL, &type, (BYTE *)cmd, &size) != ERROR_SUCCESS || type != REG_SZ) cmd[0] = 0;
        RegCloseKey(e);
        if (!display[0] || !cmd[0]) continue;
        lstrcpy(ldisplay, display); Lower(ldisplay);
        lstrcpy(lcmd, cmd); Lower(lcmd);
        m = UninstallMatches(dir, it->lname, ldisplay, lcmd);
        if (m > best) { best = m; CopyN(name, display, 100); CopyN(command, cmd, MAX_PATH); }
    }
    RegCloseKey(k);
    return best;
}

/* ------------------------------------------------------------------------
 * The panel: actions
 * --------------------------------------------------------------------- */

/* A question in a message box. The panel stays open behind it. */
int Ask(const char *text, UINT flags)
{
    int r;
    g_modal++;
    r = MessageBox(g_panel, text, APP_NAME, flags);
    g_modal--;
    if (IsWindowVisible(g_panel)) SetFocus(g_edit);
    return r;
}

static void FavChanged(void)
{
    int i;
    SaveState();
    InvalidateRect(g_panel, NULL, FALSE);
    for (i = 0; i < g_listCount; i++) InvalidateRect(g_list[i], NULL, FALSE);
    InvalidateRect(g_results, NULL, FALSE);
}

void PinAt(int index, int pos)
{
    char id[110], msg[200];
    int r;
    if (index < 0 || index >= g_count) return;
    MakeId(&g_items[index], id);
    r = PinMove(id, pos);
    if (r == 2) lstrcpy(msg, "Favorites is full (5). Remove one with its X button first.");
    else wsprintf(msg, r ? "Moved in Favorites: %s" : "Added to Favorites: %s", g_items[index].name);
    FavChanged();
    Flash(msg);
}

void Unpin(int index)
{
    char msg[200];
    int p;
    if (index < 0 || index >= g_count) return;
    p = IsPinned(g_favs, g_favCount, &g_items[index]);
    if (p < 0) return;
    wsprintf(msg, "Remove \"%s\" from Favorites?", g_items[index].name);
    if (Ask(msg, MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    PinRemove(p);
    FavChanged();
    wsprintf(msg, "Removed from Favorites: %s", g_items[index].name);
    Flash(msg);
}

/* F2, for the keyboard: pins the program, or offers to remove it. */
void ToggleFav(int index)
{
    if (index < 0 || index >= g_count) return;
    if (IsPinned(g_favs, g_favCount, &g_items[index]) >= 0) Unpin(index);
    else PinAt(index, g_favCount);
}

/* ------------------------------------------------------------------------
 * The panel: running programs
 * --------------------------------------------------------------------- */

void SwitchTo(int t)
{
    TASK *k;
    HWND target;
    if (t < 0 || t >= g_taskCount || g_testMode) return;
    k = &g_tasks[t];
    if (!k->hwnd) { Flash("That program runs in the background and has no window. Del ends it."); return; }
    if (!IsWindow(k->hwnd)) { RefreshTasks(); return; }
    target = GetLastActivePopup(k->hwnd);
    if (!target || !IsWindowVisible(target)) target = k->hwnd;
    if (IsIconic(k->hwnd)) PostMessage(k->hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
    HidePanel();
    SetForegroundWindow(target);
}

/* Ends a program at once, after asking. Unsaved work in it is lost. */
void KillTask(int t)
{
    TASK *k;
    char msg[400];
    HANDLE p;
    int ok;
    if (t < 0 || t >= g_taskCount || g_testMode) return;
    k = &g_tasks[t];
    if (k->locked) {
        wsprintf(msg, "%s is part of Windows and cannot be ended from here.", k->exe);
        Ask(msg, MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (k->hung) wsprintf(msg, "\"%.100s\" is not responding.\n\nEnd it now? Anything you have not saved in it will be lost.", k->title);
    else wsprintf(msg, "End \"%.100s\" now?\n\nAnything you have not saved in it will be lost.", k->title);
    if (Ask(msg, MB_YESNO | MB_ICONEXCLAMATION | MB_DEFBUTTON2) != IDYES) return;
    p = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, k->pid);
    ok = p && TerminateProcess(p, 1);
    if (ok) WaitForSingleObject(p, 2000);
    if (p) CloseHandle(p);
    if (ok) wsprintf(msg, "Ended: %.100s", k->title);
    else wsprintf(msg, "Windows did not allow Searchlight to end %.100s.", k->title);
    RefreshTasks();
    Flash(msg);
}

/* Asks a window to close, the same as clicking its X button. */
void CloseTask(int t)
{
    TASK *k;
    char msg[200];
    if (t < 0 || t >= g_taskCount || g_testMode) return;
    k = &g_tasks[t];
    if (!k->hwnd || k->hung) { KillTask(t); return; }
    if (!IsWindow(k->hwnd)) { RefreshTasks(); return; }
    PostMessage(k->hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
    g_closing = k->hwnd;
    g_closeTicks = 0;
    CopyN(g_closingName, k->title, sizeof(g_closingName));
    SetTimer(g_panel, 3, 250, NULL);
    wsprintf(msg, "Closing: %.100s", k->title);
    Flash(msg);
}

/* Every quarter of a second after a window was asked to close. If it is
 * still open after a while it is probably asking whether to save, so the
 * panel steps aside and shows it. */
void WatchClosing(void)
{
    char msg[200];
    HWND target;
    if (!g_closing) { KillTimer(g_panel, 3); return; }
    if (!IsWindow(g_closing) || !IsWindowVisible(g_closing)) {
        KillTimer(g_panel, 3);
        g_closing = NULL;
        RefreshTasks();
        wsprintf(msg, "Closed: %.100s", g_closingName);
        Flash(msg);
        return;
    }
    if (++g_closeTicks < 6) return;
    KillTimer(g_panel, 3);
    target = GetLastActivePopup(g_closing);
    if (!target || !IsWindowVisible(target)) target = g_closing;
    g_closing = NULL;
    HidePanel();
    SetForegroundWindow(target);
}

/* ------------------------------------------------------------------------
 * The panel: starting what was chosen
 * --------------------------------------------------------------------- */

static void WinKey(BYTE vk)
{
    keybd_event(VK_LWIN, 0, 0, 0);
    keybd_event(vk, 0, 0, 0);
    keybd_event(vk, 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_LWIN, 0, KEYEVENTF_KEYUP, 0);
}

void Launch(int index)
{
    ITEM *it;
    char id[110], msg[200];
    int i, n, ok = 1;
    if (index < 0 || index >= g_count) return;
    it = &g_items[index];
    if (it->kind == 1 && PowerAction(it->path)) { AskPower(PowerAction(it->path)); return; }
    MakeId(it, id);
    HidePanel();
    if (it->kind == 1 && it->path[0] == '@') {
        if (strcmp(it->path, "@run") == 0) WinKey('R');
        else if (strcmp(it->path, "@find") == 0) WinKey('F');
        else if (strcmp(it->path, "@minimize") == 0) WinKey('M');
        else if (strcmp(it->path, "@shutdown") == 0) { HWND d = FindWindow("Progman", NULL); if (d) PostMessage(d, WM_CLOSE, 0, 0); }
    } else if (it->kind == 1) {
        ok = WinExec(it->path, SW_SHOWNORMAL) > 31;
    } else {
        ok = (int)ShellExecuteA(NULL, NULL, it->path, NULL, NULL, SW_SHOWNORMAL) > 32;
    }
    if (!ok) {
        wsprintf(msg, "Searchlight 98 could not start \"%s\".", it->name);
        MessageBox(NULL, msg, APP_NAME, MB_OK | MB_ICONEXCLAMATION);
        return;
    }
    /* Move it to the front of Recent. */
    {
        char keep[MAX_PINS][110];
        n = 0;
        lstrcpy(keep[n++], id);
        for (i = 0; i < g_recentCount && n < MAX_PINS; i++) if (strcmp(g_recent[i], id) != 0) lstrcpy(keep[n++], g_recent[i]);
        for (i = 0; i < n; i++) lstrcpy(g_recent[i], keep[i]);
        g_recentCount = n;
    }
    it->uses = CountUse(id);
    SaveState();
}

/* Enter on the answer to a sum puts it on the Clipboard. */
static void CopyResult(void)
{
    char msg[120];
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, lstrlen(g_calcText) + 1);
    if (!mem) return;
    lstrcpy((char *)GlobalLock(mem), g_calcText);
    GlobalUnlock(mem);
    if (!OpenClipboard(g_panel)) { GlobalFree(mem); return; }
    EmptyClipboard();
    SetClipboardData(CF_TEXT, mem);
    CloseClipboard();
    wsprintf(msg, "%s is on the Clipboard. Paste it with Ctrl+V.", g_calcText);
    Flash(msg);
}

/* Opens or runs what was typed, the same as Start, Run would. */
static void RunTyped(void)
{
    char file[MAX_PATH], args[120], msg[400];
    if (!g_runOk || g_testMode) return;
    lstrcpy(file, g_runFile);
    lstrcpy(args, g_runArgs);
    HidePanel();
    if ((int)ShellExecuteA(NULL, NULL, file, args[0] ? args : NULL, NULL, SW_SHOWNORMAL) > 32) return;
    wsprintf(msg, "Searchlight 98 could not open \"%.250s\".", file);
    MessageBox(NULL, msg, APP_NAME, MB_OK | MB_ICONEXCLAMATION);
}

/* Enter or a click on a row of the results. */
void Activate(int row)
{
    if (row == ROW_CALC) CopyResult();
    else if (row == ROW_RUN) RunTyped();
    else if (row == ROW_MODE) ApplyMode();
    else if (row == ROW_TIMER) StartCountdown();
    else Launch(row);
}

void ForgetRecent(int tile)
{
    char msg[200];
    int i, index;
    if (tile < 0 || tile >= g_recentCount) return;
    index = FindById(g_recent[tile]);
    for (i = tile; i < g_recentCount - 1; i++) lstrcpy(g_recent[i], g_recent[i + 1]);
    g_recentCount--;
    SaveState();
    g_hoverBox = 0; g_hoverTile = -1;
    InvalidateRect(g_panel, NULL, FALSE);
    if (index >= 0) { wsprintf(msg, "Removed from Recent: %s", g_items[index].name); Flash(msg); }
}

/* F1 or a click on the gauges: the figures behind them, in words. */
void Details(void)
{
    char text[2000], part[500];
    if (g_testMode) return;
    ReadStats();
    lstrcpy(text, "PROCESSOR\n");
    if (g_cpu < 0) lstrcat(text, "This version of Windows does not report how busy the processor is.\n");
    else { wsprintf(part, "Windows reports it as %d%% busy.\n", g_cpu); lstrcat(text, part); }
    lstrcat(text, "Windows 95 and 98 do not keep this figure for each program.\n");

    wsprintf(part, "\nMEMORY\nInstalled:\t\t%lu MB\nIn use:\t\t\t%lu MB\nDisk cache:\t\t%lu MB%s\nNot in use:\t\t%lu MB\n",
             (g_memTotalK + 512) / 1024, (g_memUsedK + 512) / 1024, (g_memCacheK + 512) / 1024,
             g_cacheKnown ? "" : "  (Windows did not say)", (g_memFreeK + 512) / 1024);
    lstrcat(text, part);
    if (g_memSwapK != 0xFFFFFFFF) { wsprintf(part, "Swap file in use:\t\t%lu MB\n", (g_memSwapK + 512) / 1024); lstrcat(text, part); }
    lstrcat(text, "The disk cache is memory Windows borrows to speed up the disks.\n"
                  "It gives it back as soon as a program needs it.\n");

    lstrcat(text, "\nSYSTEM RESOURCES\n");
    if (g_resFree < 0) lstrcat(text, "Not available. On Windows 95 and 98 they are read through Resource Meter,\nan optional part of Windows that is not installed here.\n");
    else { wsprintf(part, "System %d%% free,  User %d%% free,  GDI %d%% free\n", g_resFree, g_resUser, g_resGdi); lstrcat(text, part); }

    {
        static int logged = 0;
        MEMUSE use;
        MeasureProcess(GetCurrentProcessId(), &use, !logged);
        logged = 1;
        wsprintf(part, "\nSEARCHLIGHT 98\nProgram and its own data:\t%lu K\nWindows libraries:\t\t%lu K in %d libraries\n"
                       "The libraries are shared with other programs and are in memory once.\n"
                       "%d programs, settings and documents are listed.\n",
                 use.ownK, use.libK, use.libs, g_count);
        lstrcat(text, part);
    }

    wsprintf(part, "\nFOR REPORTING A PROBLEM\nCounters as read, in %s: processor %lu, cache %lu, free %lu,\nallocated %lu, swap %lu. Measuring the programs took %lu ms.",
             g_statBytes ? "bytes" : "pages", g_statRaw[STAT_CPU], g_statRaw[STAT_CACHE], g_statRaw[STAT_FREE],
             g_statRaw[STAT_COMMIT], g_statRaw[STAT_SWAP], g_measureMs);
    lstrcat(text, part);
    g_modal++;
    MessageBox(g_panel, text, APP_NAME " - Details", MB_OK | MB_ICONINFORMATION);
    g_modal--;
    if (IsWindowVisible(g_panel)) SetFocus(g_edit);
}

/* ------------------------------------------------------------------------
 * The menu that a right-click opens
 * --------------------------------------------------------------------- */

static void CategoryFile(char *path) { PathIn(path, "CATEGORY.TXT"); }

static void OpenFolder(int index)
{
    char target[MAX_PATH], cmd[MAX_PATH + 40];
    LinkTarget(g_items[index].path, target);
    HidePanel();
    wsprintf(cmd, "explorer.exe /select,%s", target);
    if (WinExec(cmd, SW_SHOWNORMAL) <= 31) MessageBox(NULL, "The folder could not be opened.", APP_NAME, MB_OK | MB_ICONEXCLAMATION);
}

static void ShowProperties(int index)
{
    MY_SHELLEXECUTEINFOA e;
    char path[MAX_PATH];
    lstrcpy(path, g_items[index].path);
    HidePanel();
    ZeroMemory(&e, sizeof(e));
    e.cbSize = sizeof(e);
    e.fMask = MY_SEE_MASK_INVOKEIDLIST;
    e.lpVerb = "properties";
    e.lpFile = path;
    e.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExA(&e)) MessageBox(NULL, "Windows could not show the properties.", APP_NAME, MB_OK | MB_ICONEXCLAMATION);
}

static void HideItem(int index)
{
    char file[MAX_PATH], name[100], msg[400];
    lstrcpy(name, g_items[index].name);
    wsprintf(msg, "Hide \"%s\" from Searchlight?\n\nThe program itself is not touched. To bring it back, take its line out of\nCATEGORY.TXT in the Searchlight 98 folder.", name);
    if (Ask(msg, MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    CategoryFile(file);
    if (!SetOverride(file, name, "hide")) { Flash("CATEGORY.TXT could not be written."); return; }
    Rescan(1);
    UpdateQuery();
    wsprintf(msg, "Hidden: %s", name);
    Flash(msg);
}

/* A program dropped on another group moves there, and CATEGORY.TXT remembers. */
void MoveToGroup(int index, int cat)
{
    char file[MAX_PATH], name[100], msg[200];
    if (index < 0 || index >= g_count || g_items[index].kind != 0 || cat < 0 || cat >= g_catCount) return;
    lstrcpy(name, g_items[index].name);
    CategoryFile(file);
    if (!SetOverride(file, name, g_catName[cat])) { Flash("CATEGORY.TXT could not be written."); return; }
    wsprintf(msg, "Moved to %s: %s", g_catName[cat], name);
    Rescan(1);
    Flash(msg);
}

static void Uninstall(const char *name, const char *command)
{
    char msg[300], cmd[MAX_PATH];
    lstrcpy(cmd, command);
    wsprintf(msg, "Start the program that removes \"%.100s\"?", name);
    if (Ask(msg, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
    HidePanel();
    if (WinExec(cmd, SW_SHOWNORMAL) <= 31) MessageBox(NULL, "The program that removes it could not be started.\nPlease use Add/Remove Programs in Control Panel.", APP_NAME, MB_OK | MB_ICONEXCLAMATION);
}

/* Shows a menu at the mouse and returns what was chosen, 0 for nothing. */
static int PopMenu(HMENU m)
{
    POINT pt;
    int cmd;
    GetCursorPos(&pt);
    g_modal++;
    cmd = (int)TrackPopupMenu(m, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_panel, NULL);
    g_modal--;
    DestroyMenu(m);
    if (IsWindowVisible(g_panel)) SetFocus(g_edit);
    return cmd;
}

/* For a program, a setting or a document. "recent" is its place in Recent
 * if the click was there, otherwise -1. */
void ItemMenu(int index, int recent)
{
    HMENU m;
    ITEM *it;
    char label[160], unName[100], unCommand[MAX_PATH];
    int cmd, pinned, remove;
    if (index < 0 || index >= g_count || g_testMode) return;
    it = &g_items[index];
    pinned = IsPinned(g_favs, g_favCount, it) >= 0;
    remove = FindUninstall(it, unName, unCommand);
    m = CreatePopupMenu();
    AppendMenu(m, MF_STRING, CMD_OPEN, "&Open");
    SetMenuDefaultItem(m, CMD_OPEN, FALSE);
    if (it->kind != 1) {
        AppendMenu(m, MF_STRING, CMD_FOLDER, "Open Containing &Folder");
        AppendMenu(m, MF_STRING, CMD_PROPS, "P&roperties");
    }
    AppendMenu(m, MF_SEPARATOR, 0, NULL);
    AppendMenu(m, MF_STRING, CMD_PIN, pinned ? "Remove from Fa&vorites..." : "&Pin to Favorites");
    if (recent >= 0) AppendMenu(m, MF_STRING, CMD_FORGET, "Remove from R&ecent");
    if (it->kind == 0) AppendMenu(m, MF_STRING, CMD_HIDE, "&Hide from Searchlight...");
    if (remove) {
        wsprintf(label, "&Uninstall %.100s...", unName);
        AppendMenu(m, MF_SEPARATOR, 0, NULL);
        AppendMenu(m, MF_STRING, CMD_UNINSTALL, label);
    }
    cmd = PopMenu(m);
    switch (cmd) {
    case CMD_OPEN:      Launch(index); break;
    case CMD_FOLDER:    OpenFolder(index); break;
    case CMD_PROPS:     ShowProperties(index); break;
    case CMD_PIN:       if (pinned) Unpin(index); else PinAt(index, g_favCount); break;
    case CMD_FORGET:    ForgetRecent(recent); break;
    case CMD_HIDE:      HideItem(index); break;
    case CMD_UNINSTALL: Uninstall(unName, unCommand); break;
    }
}

void TaskMenu(int t)
{
    HMENU m;
    int cmd;
    if (t < 0 || t >= g_taskCount || g_testMode) return;
    m = CreatePopupMenu();
    if (g_tasks[t].hwnd) {
        AppendMenu(m, MF_STRING, CMD_SWITCH, "&Switch To");
        AppendMenu(m, MF_STRING, CMD_CLOSE, "&Close");
        SetMenuDefaultItem(m, CMD_SWITCH, FALSE);
        AppendMenu(m, MF_SEPARATOR, 0, NULL);
    }
    AppendMenu(m, MF_STRING | (g_tasks[t].locked ? MF_GRAYED : 0), CMD_END, "&End Now...");
    cmd = PopMenu(m);
    if (cmd == CMD_SWITCH) SwitchTo(t);
    else if (cmd == CMD_CLOSE) CloseTask(t);
    else if (cmd == CMD_END) KillTask(t);
}

/* Switches a row of the Startup list off or back on. */
void ToggleStartRow(int i)
{
    START *s;
    char msg[200];
    if (i < 0 || i >= g_startCount) return;
    s = &g_starts[i];
    if (s->source == SRC_WININI) { Flash("This one starts from WIN.INI. Searchlight leaves that file alone."); return; }
    if (!ToggleStart(s)) { Flash("Windows did not allow that change."); return; }
    wsprintf(msg, s->enabled ? "%.100s starts with Windows again." : "%.100s will not start the next time Windows starts.", s->name);
    Flash(msg);
    InvalidateRect(g_startList, NULL, FALSE);
    InvalidateRect(g_panel, &g_rcContent, FALSE);
}

void StartMenu(int i)
{
    HMENU m;
    if (i < 0 || i >= g_startCount || g_testMode) return;
    m = CreatePopupMenu();
    AppendMenu(m, MF_STRING | (g_starts[i].source == SRC_WININI ? MF_GRAYED : 0), CMD_TOGGLE,
               g_starts[i].enabled ? "Switch &Off" : "Switch &On");
    SetMenuDefaultItem(m, CMD_TOGGLE, FALSE);
    if (PopMenu(m) == CMD_TOGGLE) ToggleStartRow(i);
}

/* The Read Me opens in Notepad. */
void Help(void)
{
    char path[MAX_PATH];
    if (g_testMode) return;
    PathIn(path, "README.TXT");
    HidePanel();
    if ((int)ShellExecuteA(NULL, NULL, path, NULL, NULL, SW_SHOWNORMAL) <= 32)
        MessageBox(NULL, "README.TXT is missing from the Searchlight 98 folder.", APP_NAME, MB_OK | MB_ICONINFORMATION);
}

/* Starts a favorite by its hotkey, Ctrl+Alt+1 to 5. */
void FavoriteHotkey(int place)
{
    if (g_modal || place < 0 || place >= g_favCount) return;
    Launch(FindById(g_favs[place]));
}
