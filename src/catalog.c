/* catalog.c - Searchlight 98: the list of programs, settings and documents, and how it is filled. */
#include "slight98.h"

typedef struct { char lname[100]; char cat[40]; } OVERRIDE;
static OVERRIDE *g_over = NULL;              /* only while the Start Menu is scanned */
static int g_overCount = 0;

/* Group rules. The first rule that matches "folder names + program name"
 * decides the group. Words are separated by |. A leading = means the whole
 * word must match. */
typedef struct { int cat; const char *words; } RULE;
static const RULE g_rules[] = {
    { 0, "=game|=games|fifa|world cup|worldcup|commandos|=sims|simcity|age of empires|age of kings|midtown|"
         "duke nukem|dukenukem|duke3d|wolfenstein|wolf3d|spear of destiny|tomb raider|tombraider|black dahlia|"
         "futbol|f\xFAtbol|time commando|solitaire|minesweeper|freecell|hearts|pinball|maxis|ea sports|"
         "3d realms|id software|eidos|core design" },
    { 4, "firebird|interbase|mysql|msde|sql server" },
    { 7, "system tools|scandisk|defrag|disk cleanup|maintenance wizard|scheduled tasks|system information|"
         "backup|drivespace|compression agent|resource meter|windows explorer|ms-dos prompt|command prompt|"
         "windows update|control panel|welcome to windows|clipboard|net watcher|system monitor|"
         "drive converter|system file checker|registry checker|dr watson|dr. watson|update wizard" },
    { 6, "accessories|notepad|wordpad|calculator|hyperterminal|phone dialer|address book|character map|"
         "imaging|kodak" },
    { 4, "visual basic|visual c++|visual studio|visual foxpro|visual interdev|visual j++|visual sourcesafe|"
         "msdn|delphi|borland|c++builder|c++ builder|jbuilder|inprise|dreamweaver|homesite|allaire|flash5|"
         "flash 5|macromedia flash|fireworks|python|activeperl|=perl|=java|j2sdk|=jdk|freepascal|free pascal|"
         "=fpc|apache|=php|wincvs|=cvs|textpad|editplus|ultraedit|paradox|qbasic|quickbasic|developer|=ide|"
         "jrun|=jvm|extension manager|personal web|nt service wizard|connector wizard|activex" },
    { 1, "internet|explorer|netscape|navigator|communicator|opera|retrozilla|kmeleon|k-meleon|mozilla|"
         "outlook express|=icq|=aim|instant messenger|=msn|messenger|mirc|=irc|napster|winmx|kazaa|getright|"
         "gozilla|go!zilla|=ftp|netmeeting|dialup|dial-up|web publish|=publish|=chat" },
    { 2, "=word|excel|powerpoint|=access|outlook|publisher|frontpage|office|wordperfect|quattro|corel|"
         "presentations|=works|=money|quicken|printshop|print shop|encarta|atlas|acrobat|winfax|photodraw|"
         "binder" },
    { 3, "animation shop|jasc|winamp|sonique|realplayer|realjukebox|real jukebox|quicktime|media player|"
         "musicmatch|jukebox|divx|acdsee|paintshop|paint shop|photoshop|imageready|powerdvd|=dvd|nero|clonecd|"
         "daemon|cd player|sound recorder|volume control|=paint|playa|mp3|video|audio|music|graphic|photo" },
    { 5, "winrar|winzip|7-zip|=zip|commander|norton|symantec|partitionmagic|partition magic|powerquest|"
         "windowblinds|stardock|tweakui|tweak ui|3dmark|madonion|benchmark|=rain|sysinternals|virus|cleaner|"
         "utilit|searchlight" }
};

/* Name, command, search words, icon. Commands starting with @ are handled here. */
static const char *g_set[][4] = {
    { "Control Panel", "control.exe", "settings options configuration", "shell32.dll,21" },
    { "Display", "control.exe desk.cpl", "screen resolution wallpaper background colors color depth monitor active desktop appearance", "desk.cpl,0" },
    { "Add/Remove Programs", "control.exe appwiz.cpl", "uninstall install remove software windows setup components", "appwiz.cpl,0" },
    { "System Properties", "control.exe sysdm.cpl", "computer hardware performance virtual memory", "sysdm.cpl,0" },
    { "Device Manager", "control.exe sysdm.cpl,,1", "hardware devices drivers dma", "sysdm.cpl,0" },
    { "Multimedia and Sounds", "control.exe mmsys.cpl", "audio volume sound speakers midi", "mmsys.cpl,0" },
    { "Mouse", "control.exe main.cpl", "pointer cursor double click", "main.cpl,0" },
    { "Keyboard", "control.exe main.cpl,@1", "language layout repeat", "main.cpl,1" },
    { "Date/Time", "control.exe timedate.cpl", "clock time zone", "timedate.cpl,0" },
    { "Network", "control.exe netcpl.cpl", "tcp ip lan adapter protocol", "netcpl.cpl,0" },
    { "Internet Options", "control.exe inetcpl.cpl", "browser proxy cache security zones", "inetcpl.cpl,0" },
    { "Regional Settings", "control.exe intl.cpl", "locale currency number format language", "intl.cpl,0" },
    { "Accessibility Options", "control.exe access.cpl", "sticky keys contrast", "access.cpl,0" },
    { "Power Management", "control.exe powercfg.cpl", "standby sleep hibernate monitor off", "powercfg.cpl,0" },
    { "Game Controllers", "control.exe joy.cpl", "joystick gamepad", "joy.cpl,0" },
    { "Modems", "control.exe modem.cpl", "dial up", "modem.cpl,0" },
    { "Passwords", "control.exe password.cpl", "users profiles logon", "password.cpl,0" },
    { "Printers", "control.exe printers", "print", "shell32.dll,16" },
    { "Fonts", "control.exe fonts", "typeface", "shell32.dll,38" },
    { "System Configuration (msconfig)", "msconfig.exe", "startup boot services msconfig", "msconfig.exe,0" },
    { "Registry Editor", "regedit.exe", "regedit registry", "regedit.exe,0" },
    { "DirectX Diagnostics", "dxdiag.exe", "dxdiag directx direct3d video card", "dxdiag.exe,0" },
    { "ScanDisk", "scandskw.exe", "check disk errors", "scandskw.exe,0" },
    { "Disk Defragmenter", "defrag.exe", "defrag optimize", "defrag.exe,0" },
    { "MS-DOS Prompt", "command.com", "dos command line cmd", "command.com,-1" },
    { "Windows Explorer", "explorer.exe /e,C:\\", "files folders browse", "explorer.exe,0" },
    { "My Computer", "explorer.exe ,::{20D04FE0-3AEA-1069-A2D8-08002B30309D}", "drives disks", "shell32.dll,15" },
    { "Recycle Bin", "explorer.exe ,::{645FF040-5081-101B-9F08-00AA002F954E}", "trash deleted", "shell32.dll,31" },
    { "Run...", "@run", "run command", "shell32.dll,24" },
    { "Find Files", "@find", "search files folders", "shell32.dll,22" },
    { "Show Desktop", "@minimize", "minimize all windows", "shell32.dll,34" },
    { "Shut Down...", "@shutdown", "shutdown restart reboot turn off exit windows", "shell32.dll,27" },
    { "Restart Windows", "@restart", "reboot restart now", "shell32.dll,27" },
    { "Restart in MS-DOS Mode", "@dos", "dos mode exit restart", "command.com,-1" },
    { "Log Off", "@logoff", "logoff log out logout user", "shell32.dll,44" },
    { "Stand By", "@standby", "standby sleep suspend", "shell32.dll,25" }
};

/* Shortcuts with these words are left out. A trailing $ means "ends with". */
const char g_skip[] =
    "uninst|un-install|desinstal|deinstall|=remove|=doc|=docs|=online|readme|read me|release notes|=help|manual|license|"
    "licence|website|web site|on the web|registration|=register|=faq|whats new|what's new|documentation|"
    "tutorial|technical support|order form|online services|setup$|install$";
#define SET_COUNT ((int)(sizeof(g_set) / sizeof(g_set[0])))

/* ------------------------------------------------------------------------
 * Groups
 * --------------------------------------------------------------------- */

/* Tests one rule word against lower-case text. */
int MatchWord(const char *text, const char *word, int len)
{
    int whole = 0, tail = 0, tl = lstrlen(text);
    const char *p;
    if (len > 0 && word[0] == '=') { whole = 1; word++; len--; }
    if (len > 0 && word[len - 1] == '$') { tail = 1; len--; }
    if (len <= 0 || len > tl) return 0;
    if (tail) return strncmp(text + tl - len, word, len) == 0;
    for (p = text; *p; p++) {
        if (strncmp(p, word, len) != 0) continue;
        if (!whole) return 1;
        if ((p == text || !IsWordChar(p[-1])) && !IsWordChar(p[len])) return 1;
    }
    return 0;
}

int MatchAny(const char *text, const char *words)
{
    const char *p = words;
    while (*p) {
        const char *e = strchr(p, '|');
        int len = e ? (int)(e - p) : lstrlen(p);
        if (MatchWord(text, p, len)) return 1;
        if (!e) break;
        p = e + 1;
    }
    return 0;
}

static int FindCat(const char *name, int create)
{
    int i;
    for (i = 0; i < g_catCount; i++) if (lstrcmpi(g_catName[i], name) == 0) return i;
    if (!create || g_catCount >= MAX_CATS) return CAT_OTHER;
    CopyN(g_catName[g_catCount], name, sizeof(g_catName[0]));
    return g_catCount++;
}

/* Returns the group for a program, or -1 if it is hidden. */
int Categorize(const char *lname, const char *lfolder)
{
    char text[400];
    int i;
    for (i = 0; i < g_overCount; i++) {
        if (strcmp(g_over[i].lname, lname) == 0) {
            if (lstrcmpi(g_over[i].cat, "hide") == 0) return -1;
            return FindCat(g_over[i].cat, 1);
        }
    }
    wsprintf(text, "%.200s %.150s", lfolder, lname);
    for (i = 0; i < (int)(sizeof(g_rules) / sizeof(g_rules[0])); i++)
        if (MatchAny(text, g_rules[i].words)) return g_rules[i].cat;
    return CAT_OTHER;
}

/* ------------------------------------------------------------------------
 * The list
 * --------------------------------------------------------------------- */

void MakeId(const ITEM *it, char *out)
{
    wsprintf(out, "%s:%s", it->kind == 2 ? "doc" : (it->kind ? "set" : "app"), it->lname);
}

int FindById(const char *id)
{
    char tmp[110];
    int i;
    for (i = 0; i < g_count; i++) { MakeId(&g_items[i], tmp); if (strcmp(tmp, id) == 0) return i; }
    return -1;
}

int IsPinned(char list[][110], int count, const ITEM *it)
{
    char id[110];
    int i;
    MakeId(it, id);
    for (i = 0; i < count; i++) if (strcmp(list[i], id) == 0) return i;
    return -1;
}

static int HasName(const char *lname)
{
    int i;
    for (i = 0; i < g_count; i++) if (strcmp(g_items[i].lname, lname) == 0) return 1;
    return 0;
}

/* Keeps a copy of a text in the pool and returns where it is. */
static const char *Keep(const char *text)
{
    int len = lstrlen(text) + 1;
    char *at;
    if (len == 1) return "";
    if (!g_pool || g_pool->used + len > g_pool->size) {
        int size = len > POOL_BLOCK ? len : POOL_BLOCK;
        POOL *p = (POOL *)malloc(sizeof(POOL) + size);
        if (!p) return "";
        p->next = g_pool; p->used = 0; p->size = size;
        g_pool = p;
    }
    at = g_pool->text + g_pool->used;
    memcpy(at, text, len);
    g_pool->used += len;
    return at;
}

static void FreePool(POOL *p)
{
    while (p) { POOL *next = p->next; free(p); p = next; }
}

ITEM *AddItem(int kind, const char *name, const char *path, int cat, const char *keys, const char *icon)
{
    ITEM *it;
    char shown[100], lname[100], inits[24], lkeys[160], file[MAX_PATH], where[48];
    int i, n = 0, start = 1;
    CopyN(shown, name, sizeof(shown));
    lstrcpy(lname, shown);
    Lower(lname);
    if (HasName(lname)) return NULL;
    if (g_count >= g_cap) {
        ITEM *grown = (ITEM *)realloc(g_items, (g_cap + 64) * sizeof(ITEM));
        if (!grown) return NULL;
        g_items = grown; g_cap += 64;
    }
    CopyN(file, path, sizeof(file));
    CopyN(lkeys, keys ? keys : "", sizeof(lkeys));
    Lower(lkeys);
    CopyN(where, icon ? icon : "", sizeof(where));
    for (i = 0; lname[i]; i++) {
        char c = lname[i];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            if (start && n < (int)sizeof(inits) - 1) inits[n++] = c;
            start = 0;
        } else start = 1;
    }
    inits[n] = 0;
    it = &g_items[g_count++];
    ZeroMemory(it, sizeof(ITEM));
    it->kind = kind; it->cat = cat;
    it->name = Keep(shown);
    it->lname = Keep(lname);
    it->inits = Keep(inits);
    it->keys = Keep(lkeys);
    it->path = Keep(file);
    it->icon = Keep(where);
    return it;
}

/* ------------------------------------------------------------------------
 * Groups the user has chosen, kept in CATEGORY.TXT
 * --------------------------------------------------------------------- */

static void LoadOverrides(void)
{
    char path[MAX_PATH], line[300];
    FILE *f;
    g_overCount = 0;
    if (!g_over) g_over = (OVERRIDE *)calloc(MAX_OVER, sizeof(OVERRIDE));
    if (!g_over) return;
    PathIn(path, "CATEGORY.TXT");
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f) && g_overCount < MAX_OVER) {
        char *eq;
        Trim(line);
        if (!line[0] || line[0] == '#') continue;
        eq = strrchr(line, '=');
        if (!eq || eq == line) continue;
        *eq++ = 0;
        Trim(line); Trim(eq);
        if (!line[0] || !eq[0]) continue;
        CopyN(g_over[g_overCount].lname, line, sizeof(g_over[0].lname));
        Lower(g_over[g_overCount].lname);
        CopyN(g_over[g_overCount].cat, eq, sizeof(g_over[0].cat));
        g_overCount++;
    }
    fclose(f);
}

/* Writes "name=group" into a file like CATEGORY.TXT, in place of any line
 * that was there for the same name. Returns 1 if the file was written. */
int SetOverride(const char *file, const char *name, const char *group)
{
    char line[300], key[300], *text, *eq;
    FILE *f;
    long size = 0;
    int len = 0;
    f = fopen(file, "r");
    if (f) { fseek(f, 0, SEEK_END); size = ftell(f); fseek(f, 0, SEEK_SET); }
    text = (char *)malloc(size + 400);
    if (!text) { if (f) fclose(f); return 0; }
    text[0] = 0;
    while (f && fgets(line, sizeof(line), f)) {
        lstrcpy(key, line);
        Trim(key);
        eq = strrchr(key, '=');
        if (eq && key[0] != '#') {
            *eq = 0;
            Trim(key);
            if (lstrcmpi(key, name) == 0) continue;
        }
        if (len + lstrlen(line) + 2 > size + 100) break;
        lstrcpy(text + len, line);
        len += lstrlen(line);
        if (len && text[len - 1] != '\n') { text[len++] = '\n'; text[len] = 0; }
    }
    if (f) fclose(f);
    f = fopen(file, "w");
    if (!f) { free(text); return 0; }
    fputs(text, f);
    fprintf(f, "%s=%s\n", name, group);
    fclose(f);
    free(text);
    return 1;
}

/* ------------------------------------------------------------------------
 * Scanning the Start Menu
 * --------------------------------------------------------------------- */

static void Walk(const char *dir, const char *rel, int depth)
{
    char pattern[MAX_PATH], full[MAX_PATH], name[MAX_PATH], lname[MAX_PATH], sub[400];
    WIN32_FIND_DATA fd;
    HANDLE h;
    if (lstrlen(dir) > MAX_PATH - 20) return;
    wsprintf(pattern, "%s\\*.*", dir);
    h = FindFirstFile(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        char *ext;
        if (fd.cFileName[0] == '.') continue;
        if (lstrlen(dir) + lstrlen(fd.cFileName) > MAX_PATH - 4) continue;
        wsprintf(full, "%s\\%s", dir, fd.cFileName);
        lstrcpy(name, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (depth <= 0) continue;
            lstrcpy(lname, name); Lower(lname);
            if (MatchAny(lname, g_skip)) continue;
            wsprintf(sub, "%.200s %.150s", rel, lname);
            Walk(full, sub, depth - 1);
            continue;
        }
        ext = strrchr(name, '.');
        if (!ext || (lstrcmpi(ext, ".lnk") != 0 && lstrcmpi(ext, ".pif") != 0)) continue;
        *ext = 0;
        name[99] = 0;
        lstrcpy(lname, name); Lower(lname);
        if (MatchAny(lname, g_skip)) continue;
        {
            int cat = Categorize(lname, rel);
            if (cat < 0) continue;
            AddItem(0, name, full, cat, rel, "");
        }
    } while (FindNextFile(h, &fd));
    FindClose(h);
}

static void WalkSpecial(int csidl, int depth)
{
    void *pidl = NULL;
    char path[MAX_PATH];
    if (SHGetSpecialFolderLocation(NULL, csidl, &pidl) < 0 || !pidl) return;
    if (SHGetPathFromIDListA(pidl, path) && path[0]) Walk(path, "", depth);
    CoTaskMemFree(pidl);
}

/* Documents: everything in My Documents, and the documents opened lately.
 * "links" is set for the Recent folder, which holds shortcuts to documents. */
static void WalkDocs(const char *dir, const char *rel, int depth, int links)
{
    char pattern[MAX_PATH], full[MAX_PATH], name[MAX_PATH], sub[400];
    WIN32_FIND_DATA fd;
    HANDLE h;
    if (lstrlen(dir) > MAX_PATH - 20) return;
    wsprintf(pattern, "%s\\*.*", dir);
    h = FindFirstFile(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        char *ext;
        if (g_docCount >= MAX_DOCS) break;
        if (fd.cFileName[0] == '.') continue;
        if (fd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) continue;
        if (lstrlen(dir) + lstrlen(fd.cFileName) > MAX_PATH - 4) continue;
        wsprintf(full, "%s\\%s", dir, fd.cFileName);
        lstrcpy(name, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (depth <= 0 || links) continue;
            Lower(name);
            wsprintf(sub, "%.200s %.150s", rel, name);
            WalkDocs(full, sub, depth - 1, 0);
            continue;
        }
        ext = strrchr(name, '.');
        if (links) {
            if (!ext || lstrcmpi(ext, ".lnk") != 0) continue;
            *ext = 0;
        } else if (ext && (lstrcmpi(ext, ".lnk") == 0 || lstrcmpi(ext, ".pif") == 0 || lstrcmpi(ext, ".ini") == 0)) continue;
        name[99] = 0;
        if (AddItem(2, name, full, CAT_DOCS, rel, "")) g_docCount++;
    } while (FindNextFile(h, &fd));
    FindClose(h);
}

static void DocsIn(int csidl, int depth, int links)
{
    void *pidl = NULL;
    char path[MAX_PATH];
    if (SHGetSpecialFolderLocation(NULL, csidl, &pidl) < 0 || !pidl) return;
    if (SHGetPathFromIDListA(pidl, path) && path[0]) WalkDocs(path, "", depth, links);
    CoTaskMemFree(pidl);
}

/* Stand in for the real documents in test mode. */
static void FakeDocs(void)
{
    static const char *fake[][2] = {
        { "Letter to the bank.doc", "letters" }, { "Budget 1999.xls", "" }, { "Holiday in Bariloche.bmp", "photos" },
        { "Discography.txt", "music" }, { "School report.doc", "school" }
    };
    int i;
    for (i = 0; i < (int)(sizeof(fake) / sizeof(fake[0])); i++)
        if (AddItem(2, fake[i][0], "C:\\FAKE\\DOCUMENT", CAT_DOCS, fake[i][1], "")) g_docCount++;
}

/* The shortcut Windows uses for "Restart in MS-DOS mode", if it exists. */
int DosModeFile(char *out)
{
    char path[MAX_PATH];
    GetWindowsDirectory(path, MAX_PATH - 20);
    lstrcat(path, "\\Exit To Dos.pif");
    if (out) lstrcpy(out, path);
    return FileExists(path);
}

int CompareItems(const void *a, const void *b)
{
    return strcmp(((const ITEM *)a)->lname, ((const ITEM *)b)->lname);
}

void AddSettings(void)
{
    int i;
    for (i = 0; i < SET_COUNT; i++) {
        /* Windows makes "Exit To Dos" the first time MS-DOS mode is used. Without it there is no way in. */
        if (strcmp(g_set[i][1], "@dos") == 0 && !g_testMode && !DosModeFile(NULL)) continue;
        AddItem(1, g_set[i][0], g_set[i][1], CAT_SETTINGS, g_set[i][2], g_set[i][3]);
    }
}

void Scan(void)
{
    ITEM *old = g_items;
    POOL *oldPool = g_pool;
    int oldCount = g_count, i, j;
    char extra[MAX_PATH], msg[80];
    g_items = NULL; g_count = 0; g_cap = 0;
    g_pool = NULL;
    g_catCount = FIXED_CATS;
    LoadOverrides();
    WalkSpecial(2, 6);     /* Programs */
    WalkSpecial(23, 6);    /* Programs for all users (Windows NT) */
    WalkSpecial(11, 0);    /* top of the Start Menu */
    WalkSpecial(22, 0);
    WalkSpecial(16, 0);    /* desktop */
    WalkSpecial(25, 0);
    PathIn(extra, "EXTRA");
    Walk(extra, "", 2);
    AddSettings();
    g_docCount = 0;
    if (g_testMode) FakeDocs();
    else {
        DocsIn(5, DOC_DEPTH, 0);   /* My Documents */
        DocsIn(8, 0, 1);           /* documents opened lately */
    }
    if (g_over) free(g_over);                       /* the groups are decided: the table can go */
    g_over = NULL; g_overCount = 0;
    if (g_count > 1) qsort(g_items, g_count, sizeof(ITEM), CompareItems);
    /* Keep icons already loaded for programs that are still there. */
    for (i = 0; i < g_count && old; i++) {
        for (j = 0; j < oldCount; j++) {
            if (!old[j].iconState || lstrcmpi(old[j].path, g_items[i].path) != 0 || strcmp(old[j].icon, g_items[i].icon) != 0) continue;
            g_items[i].small = old[j].small; g_items[i].large = old[j].large;
            g_items[i].iconState = old[j].iconState;
            old[j].small = old[j].large = NULL; old[j].iconState = 0;
            break;
        }
    }
    if (old) { FreeIcons(old, oldCount); free(old); }
    FreePool(oldPool);
    for (i = 0; i < MAX_CATS; i++) g_catItems[i] = 0;
    for (i = 0; i < g_count; i++) g_catItems[g_items[i].cat]++;
    PrunePins(g_favs, &g_favCount);
    PrunePins(g_recent, &g_recentCount);
    ApplyUses();
    g_lastScan = GetTickCount();
    wsprintf(msg, "Scanned: %d programs and settings, %d documents", g_count - g_docCount, g_docCount);
    Log(msg);
}
