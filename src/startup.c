/* startup.c - Searchlight 98: programs that start together with Windows.
 *
 * Programs start from three places in the registry, from the Startup folder
 * and from WIN.INI. Switching one off moves it aside, to the places the
 * System Configuration Utility of Windows 98 uses, so either tool can switch
 * it back on: registry entries go to a key with a - after its name,
 * shortcuts to the folder "Disabled Startup Items".
 */
#include "slight98.h"

static const struct { HKEY root; const char *key; const char *label; } g_startKey[3] = {
    { HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", "Registry" },
    { HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\RunServices", "Registry, as a service" },
    { HKEY_CURRENT_USER,  "Software\\Microsoft\\Windows\\CurrentVersion\\Run", "Registry, this user" }
};

const char *SourceLabel(int source)
{
    if (source <= 2) return g_startKey[source].label;
    if (source == SRC_WININI) return "WIN.INI";
    return "Startup folder";
}

/* The program file in a command such as "C:\Program Files\X\X.EXE" /tray */
static void CommandFile(const char *command, char *file)
{
    char c[MAX_PATH], *part, *sp, *quote;
    file[0] = 0;
    CopyN(c, command, MAX_PATH);
    Trim(c);
    if (c[0] == '"') {
        quote = strchr(c + 1, '"');
        if (quote) *quote = 0;
        if (!SearchPath(NULL, c + 1, ".exe", MAX_PATH, file, &part)) file[0] = 0;
        return;
    }
    /* Without quotes the name ends at a space, but folders have spaces too. */
    sp = c;
    for (;;) {
        sp = strchr(sp, ' ');
        if (sp) *sp = 0;
        if (SearchPath(NULL, c, ".exe", MAX_PATH, file, &part)) return;
        if (!sp) break;
        *sp++ = ' ';
    }
    file[0] = 0;
}

static START *AddStart(const char *name, const char *command, int source, int enabled, const char *folder)
{
    START *s;
    char file[MAX_PATH];
    if (!g_starts || g_startCount >= MAX_STARTS || !name[0]) return NULL;
    s = &g_starts[g_startCount++];
    ZeroMemory(s, sizeof(START));
    CopyN(s->name, name, sizeof(s->name));
    CopyN(s->command, command, sizeof(s->command));
    CopyN(s->folder, folder, sizeof(s->folder));
    s->source = source;
    s->enabled = enabled;
    wsprintf(s->words, "%.99s %.190s", s->name, s->command);
    Lower(s->words);
    if (g_testMode || !g_iconsStartup) return s;
    if (source == SRC_FOLDER || source == SRC_FOLDERS) s->icon = SmallFileIcon(s->command);
    else { CommandFile(s->command, file); s->icon = SmallFileIcon(file); }
    return s;
}

static void ReadStartKey(int source, int enabled)
{
    char key[200], name[200];
    BYTE data[MAX_PATH];
    DWORD i, len, size, type;
    HKEY k;
    LONG r;
    wsprintf(key, "%s%s", g_startKey[source].key, enabled ? "" : "-");
    if (RegOpenKeyEx(g_startKey[source].root, key, 0, KEY_READ, &k) != ERROR_SUCCESS) return;
    for (i = 0; i < 200; i++) {
        len = sizeof(name);
        size = sizeof(data) - 1;
        r = RegEnumValue(k, i, name, &len, NULL, &type, data, &size);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS || type != REG_SZ) continue;
        data[sizeof(data) - 1] = 0;
        AddStart(name, (char *)data, source, enabled, "");
    }
    RegCloseKey(k);
}

/* Where switched-off shortcuts are kept: next to the Startup folder. */
static void DisabledFolder(const char *folder, char *out)
{
    char *slash;
    lstrcpy(out, folder);
    slash = strrchr(out, '\\');
    if (slash) *slash = 0;
    lstrcat(out, "\\Disabled Startup Items");
}

static void ReadStartFolder(int source, const char *folder, int enabled)
{
    char dir[MAX_PATH], pattern[MAX_PATH], full[MAX_PATH], name[MAX_PATH], *ext;
    WIN32_FIND_DATA fd;
    HANDLE h;
    if (enabled) lstrcpy(dir, folder); else DisabledFolder(folder, dir);
    if (lstrlen(dir) > MAX_PATH - 20) return;
    wsprintf(pattern, "%s\\*.*", dir);
    h = FindFirstFile(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_HIDDEN)) continue;
        if (lstrcmpi(fd.cFileName, "desktop.ini") == 0) continue;
        if (lstrlen(dir) + lstrlen(fd.cFileName) > MAX_PATH - 4) continue;
        wsprintf(full, "%s\\%s", dir, fd.cFileName);
        lstrcpy(name, fd.cFileName);
        ext = strrchr(name, '.');
        if (ext) *ext = 0;
        AddStart(name, full, source, enabled, folder);
    } while (FindNextFile(h, &fd));
    FindClose(h);
}

static void ReadStartFolders(void)
{
    void *pidl;
    char mine[MAX_PATH] = "", all[MAX_PATH] = "";
    pidl = NULL;
    if (SHGetSpecialFolderLocation(NULL, 7, &pidl) >= 0 && pidl) { SHGetPathFromIDListA(pidl, mine); CoTaskMemFree(pidl); }
    pidl = NULL;
    if (SHGetSpecialFolderLocation(NULL, 24, &pidl) >= 0 && pidl) { SHGetPathFromIDListA(pidl, all); CoTaskMemFree(pidl); }
    if (mine[0]) { ReadStartFolder(SRC_FOLDER, mine, 1); ReadStartFolder(SRC_FOLDER, mine, 0); }
    if (all[0] && lstrcmpi(all, mine) != 0) { ReadStartFolder(SRC_FOLDERS, all, 1); ReadStartFolder(SRC_FOLDERS, all, 0); }
}

/* WIN.INI can start programs too, a leftover from Windows 3.1. These are
 * shown, but Searchlight leaves that file alone. */
static void ReadWinIni(void)
{
    static const char *keys[2] = { "load", "run" };
    char line[600], *p;
    int i;
    for (i = 0; i < 2; i++) {
        GetProfileString("windows", keys[i], "", line, sizeof(line));
        for (p = strtok(line, " ,\t"); p; p = strtok(NULL, " ,\t")) AddStart(BaseName(p), p, SRC_WININI, 1, "");
    }
}

static void FakeStarts(void)
{
    static const struct { const char *name, *command; int source, enabled; } fake[] = {
        { "ScanRegistry", "C:\\WINDOWS\\scanregw.exe /autorun", 0, 1 },
        { "SystemTray", "SysTray.Exe", 0, 1 },
        { "LoadPowerProfile", "Rundll32.exe powrprof.dll,LoadCurrentPwrScheme", 1, 1 },
        { "RealTray", "C:\\Program Files\\Real\\RealPlayer\\RealPlay.exe SYSTEMBOOTHIDEPLAYER", 0, 0 },
        { "ICQ NetDetect Agent", "C:\\WINDOWS\\Start Menu\\Programs\\StartUp\\ICQ NetDetect Agent.lnk", SRC_FOLDER, 0 },
        { "Microsoft Office", "C:\\WINDOWS\\Start Menu\\Programs\\StartUp\\Microsoft Office.lnk", SRC_FOLDER, 1 },
        { "Searchlight 98", "\"C:\\Program Files\\Searchlight 98\\SLIGHT98.EXE\"", 2, 1 },
        { "QuickTime Task", "\"C:\\WINDOWS\\SYSTEM\\QTTASK.EXE\" -atboottime", 0, 0 }
    };
    int i;
    for (i = 0; i < (int)(sizeof(fake) / sizeof(fake[0])); i++)
        AddStart(fake[i].name, fake[i].command, fake[i].source, fake[i].enabled, "C:\\WINDOWS\\Start Menu\\Programs\\StartUp");
}

static int CompareStarts(const void *a, const void *b)
{
    return lstrcmpi(((const START *)a)->name, ((const START *)b)->name);
}

void BuildStarts(void)
{
    int s;
    g_startCount = 0;
    if (!g_starts) g_starts = (START *)calloc(MAX_STARTS, sizeof(START));
    if (!g_starts) return;
    if (g_testMode) FakeStarts();
    else {
        for (s = 0; s < 3; s++) { ReadStartKey(s, 1); ReadStartKey(s, 0); }
        ReadStartFolders();
        ReadWinIni();
    }
    if (g_startCount > 1) qsort(g_starts, g_startCount, sizeof(START), CompareStarts);
}

int StartMatches(const START *s, const char *query)
{
    char q[120], *p;
    CopyN(q, query, sizeof(q));
    Lower(q);
    for (p = strtok(q, " \t"); p; p = strtok(NULL, " \t")) if (!strstr(s->words, p)) return 0;
    return 1;
}

/* Switches an entry off or back on. Returns 0 if Windows did not allow it.
 * In test mode nothing outside the program is touched. */
int ToggleStart(START *s)
{
    if (s->source == SRC_WININI) return 0;
    if (g_testMode) { s->enabled = !s->enabled; return 1; }
    if (s->source <= 2) {
        char from[200], to[200];
        HKEY a, b;
        DWORD made;
        int ok = 0;
        wsprintf(from, "%s%s", g_startKey[s->source].key, s->enabled ? "" : "-");
        wsprintf(to, "%s%s", g_startKey[s->source].key, s->enabled ? "-" : "");
        if (RegCreateKeyEx(g_startKey[s->source].root, to, 0, NULL, 0, KEY_SET_VALUE, NULL, &b, &made) != ERROR_SUCCESS) return 0;
        if (RegSetValueEx(b, s->name, 0, REG_SZ, (BYTE *)s->command, lstrlen(s->command) + 1) == ERROR_SUCCESS) {
            if (RegOpenKeyEx(g_startKey[s->source].root, from, 0, KEY_SET_VALUE, &a) == ERROR_SUCCESS) {
                ok = RegDeleteValue(a, s->name) == ERROR_SUCCESS;
                RegCloseKey(a);
            }
            if (!ok) RegDeleteValue(b, s->name);        /* it could not leave the old place: put things back */
        }
        RegCloseKey(b);
        if (!ok) return 0;
    } else {
        char off[MAX_PATH], to[MAX_PATH];
        DisabledFolder(s->folder, off);
        if (s->enabled) CreateDirectory(off, NULL);
        if (lstrlen(off) + lstrlen(BaseName(s->command)) > MAX_PATH - 4) return 0;
        wsprintf(to, "%s\\%s", s->enabled ? off : s->folder, BaseName(s->command));
        if (!MoveFile(s->command, to)) return 0;
        lstrcpy(s->command, to);
    }
    s->enabled = !s->enabled;
    return 1;
}
