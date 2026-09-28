/* running.c - Searchlight 98: running programs, their memory, and the gauges. */
#include "slight98.h"

typedef struct { DWORD pid; char path[MAX_PATH]; } PROC;
typedef struct { DWORD base, size; int program; } MODSPAN;    /* where a program file or library sits */
static const char *g_statWant[STAT_COUNT] = {
    "KERNEL\\CPUUsage", "VMM\\cpgDiskcache", "VMM\\cpgFree", "VMM\\cpgCommit", "VMM\\cpgSwapfileInUse"
};
static char   g_statName[STAT_COUNT][64];
static HKEY   g_perf = NULL;
static RESOURCES_FN g_resources = NULL;

#define MAX_MODULES 160

/* ------------------------------------------------------------------------
 * Running programs
 *
 * The Running list shows every open window, like Alt+Tab, followed by the
 * programs that run without a window. Parts of Windows itself are marked as
 * locked, so they cannot be ended by accident.
 * --------------------------------------------------------------------- */

static const char *g_locked[] = {
    "kernel32.dll", "msgsrv32.exe", "mprexe.exe", "mmtask.tsk", "explorer.exe",
    "system", "[system process]", "smss.exe", "csrss.exe", "winlogon.exe", "services.exe", "lsass.exe"
};
static PROC *g_procs = NULL;
int g_procCount = 0;

const char *BaseName(const char *path)
{
    const char *s = strrchr(path, '\\');
    return s ? s + 1 : path;
}

static int IsLocked(const char *exe)
{
    int i;
    for (i = 0; i < (int)(sizeof(g_locked) / sizeof(g_locked[0])); i++) if (lstrcmpi(exe, g_locked[i]) == 0) return 1;
    return 0;
}

static int ListProcesses(PROC *out, int max)
{
    HMODULE k = GetModuleHandle("KERNEL32.DLL");
    SNAPSHOT_FN snap = (SNAPSHOT_FN)GetProcAddress(k, "CreateToolhelp32Snapshot");
    PROCESS_FN first = (PROCESS_FN)GetProcAddress(k, "Process32First");
    PROCESS_FN next = (PROCESS_FN)GetProcAddress(k, "Process32Next");
    MY_PROCESSENTRY32 pe;
    HANDLE h;
    int n = 0;
    if (!snap || !first || !next) return 0;
    h = snap(MY_SNAPPROCESS, 0);
    if (!h || h == INVALID_HANDLE_VALUE) return 0;
    ZeroMemory(&pe, sizeof(pe));
    pe.dwSize = sizeof(pe);
    if (first(h, &pe)) {
        do {
            if (n < max) { out[n].pid = pe.th32ProcessID; CopyN(out[n].path, pe.szExeFile, MAX_PATH); n++; }
            pe.dwSize = sizeof(pe);
        } while (next(h, &pe));
    }
    CloseHandle(h);
    return n;
}

static const char *ProcPath(DWORD pid)
{
    int i;
    for (i = 0; i < g_procCount; i++) if (g_procs[i].pid == pid) return g_procs[i].path;
    return "";
}

static TASK *AddTask(HWND hwnd, DWORD pid, const char *title, const char *path)
{
    TASK *t;
    if (!g_tasks || g_taskCount >= MAX_TASKS) return NULL;
    t = &g_tasks[g_taskCount++];
    ZeroMemory(t, sizeof(TASK));
    t->hwnd = hwnd; t->pid = pid;
    CopyN(t->exe, BaseName(path), sizeof(t->exe));
    CopyN(t->title, title[0] ? title : t->exe, sizeof(t->title));
    wsprintf(t->words, "%.99s %.39s", t->title, t->exe);
    Lower(t->words);
    t->locked = IsLocked(t->exe);
    return t;
}

/* The same windows Alt+Tab shows: visible, with a title, not owned by another. */
static BOOL CALLBACK TaskWindow(HWND h, LPARAM lp)
{
    char title[100], cls[40];
    DWORD pid = 0, answer = 0;
    const char *path;
    TASK *t;
    (void)lp;
    if (!IsWindowVisible(h) || GetWindow(h, GW_OWNER)) return TRUE;
    if (GetWindowLong(h, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return TRUE;
    GetWindowThreadProcessId(h, &pid);
    if (pid == GetCurrentProcessId()) return TRUE;
    cls[0] = 0;
    GetClassName(h, cls, sizeof(cls));
    if (lstrcmpi(cls, "Progman") == 0) return TRUE;                 /* the desktop */
    title[0] = 0;
    GetWindowText(h, title, sizeof(title));
    if (!title[0]) return TRUE;
    path = ProcPath(pid);
    t = AddTask(h, pid, title, path);
    if (!t) return FALSE;
    /* Asking for the icon also tells whether the program still answers. */
    if (SendMessageTimeout(h, WM_GETICON, MY_ICON_SMALL, 0, SMTO_ABORTIFHUNG, 100, &answer)) t->icon = (HICON)answer;
    else t->hung = 1;
    if (!g_iconsRunning) { t->icon = NULL; return TRUE; }
    if (!t->icon && !t->hung && SendMessageTimeout(h, WM_GETICON, MY_ICON_BIG, 0, SMTO_ABORTIFHUNG, 100, &answer)) t->icon = (HICON)answer;
    if (!t->icon) t->icon = (HICON)GetClassLong(h, MY_GCL_HICONSM);
    if (!t->icon) t->icon = (HICON)GetClassLong(h, MY_GCL_HICON);
    if (!t->icon) t->icon = SmallFileIcon(path);
    return TRUE;
}

/* Stands in for the real list in test mode. */
static void FakeTasks(void)
{
    static const char *fake[][2] = {
        { "Document1 - Microsoft Word", "C:\\FAKE\\WINWORD.EXE" },
        { "Winamp 2.95", "C:\\FAKE\\WINAMP.EXE" },
        { "Exploring - C:\\My Documents", "C:\\FAKE\\EXPLORER.EXE" },
        { "Inbox - Outlook Express", "C:\\FAKE\\MSIMN.EXE" },
        { "Age of Empires II", "C:\\FAKE\\EMPIRES2.EXE" },
        { "", "C:\\FAKE\\SYSTRAY.EXE" },
        { "", "C:\\FAKE\\REALPLAY.EXE" },
        { "", "C:\\FAKE\\ICQNET.EXE" }
    };
    static const DWORD memory[] = { 14820, 6240, 3904, 9112, 61440, 612, 4380, 2216 };
    int i;
    g_taskCount = g_taskWindows = 0;
    for (i = 0; i < (int)(sizeof(fake) / sizeof(fake[0])); i++) {
        TASK *t = AddTask(fake[i][0][0] ? (HWND)(i + 1) : NULL, 1000 + i, fake[i][0], fake[i][1]);
        if (t && fake[i][0][0]) g_taskWindows++;
        if (t && i == 3) t->hung = 1;
        if (t) t->memK = memory[i];
    }
}

static void MeasureTasks(void);

static void ScanTasks(void)
{
    int i, j;
    g_taskCount = 0;
    g_procCount = ListProcesses(g_procs, MAX_PROCS);
    EnumWindows(TaskWindow, 0);
    g_taskWindows = g_taskCount;
    for (i = 0; i < g_procCount; i++) {
        TASK *t;
        if (!g_procs[i].pid || g_procs[i].pid == GetCurrentProcessId() || IsLocked(BaseName(g_procs[i].path))) continue;
        for (j = 0; j < g_taskWindows; j++) if (g_tasks[j].pid == g_procs[i].pid) break;
        if (j < g_taskWindows) continue;
        t = AddTask(NULL, g_procs[i].pid, "", g_procs[i].path);
        if (!t) break;
        if (g_iconsRunning) t->icon = SmallFileIcon(g_procs[i].path);
    }
    MeasureTasks();
}

void BuildTasks(void)
{
    g_taskCount = g_taskWindows = 0;
    if (!g_tasks) g_tasks = (TASK *)calloc(MAX_TASKS, sizeof(TASK));
    if (!g_procs) g_procs = (PROC *)calloc(MAX_PROCS, sizeof(PROC));
    if (!g_tasks || !g_procs) return;
    if (g_testMode) FakeTasks(); else ScanTasks();
}

/* Every word typed must be part of the window title or the file name. */
int TaskMatches(const TASK *t, const char *query)
{
    char q[120], *p;
    CopyN(q, query, sizeof(q));
    Lower(q);
    for (p = strtok(q, " \t"); p; p = strtok(NULL, " \t")) if (!strstr(t->words, p)) return 0;
    return 1;
}

/* ------------------------------------------------------------------------
 * The gauges: processor, memory and system resources
 *
 * Windows 95/98 publish their counters in the registry under HKEY_DYN_DATA,
 * the same place System Monitor reads. Reading a name under StartStat
 * switches that counter on. The free system resources come from RSRC32.DLL,
 * the same as Resource Meter. Windows NT has neither, so there the gauges
 * that cannot be filled say so.
 *
 * Windows 95/98 do not keep processor time for each program, so there is no
 * such column. Memory for each program is the program file and its own
 * data. The Windows libraries it has loaded are left out.
 * --------------------------------------------------------------------- */

static DWORD PerfValue(const char *name)
{
    DWORD v = 0, type = 0, size = sizeof(v);
    if (!g_perf || !name[0] || RegQueryValueEx(g_perf, name, NULL, &type, (BYTE *)&v, &size) != ERROR_SUCCESS) return 0xFFFFFFFF;
    return v;
}

/* Reading a counter's name under StartStat switches it on, under StopStat off. */
static void SwitchStats(const char *key)
{
    HKEY k;
    int s;
    if (RegOpenKeyEx(MY_HKEY_DYN_DATA, key, 0, KEY_READ, &k) != ERROR_SUCCESS) return;
    for (s = 0; s < STAT_COUNT; s++) {
        DWORD v, type, size = sizeof(v);
        RegQueryValueEx(k, g_statName[s], NULL, &type, (BYTE *)&v, &size);
    }
    RegCloseKey(k);
}

void StartStats(void)
{
    HKEY k;
    HMODULE rs;
    char name[128], msg[300];
    DWORD i, len;
    int s;
    for (s = 0; s < STAT_COUNT; s++) lstrcpy(g_statName[s], g_statWant[s]);
    if (RegOpenKeyEx(MY_HKEY_DYN_DATA, "PerfStats\\StartStat", 0, KEY_READ, &k) == ERROR_SUCCESS) {
        /* Take the spelling of each name from Windows itself. */
        for (i = 0; i < 300; i++) {
            len = sizeof(name);
            if (RegEnumValue(k, i, name, &len, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) break;
            for (s = 0; s < STAT_COUNT; s++) if (lstrcmpi(name, g_statWant[s]) == 0) CopyN(g_statName[s], name, sizeof(g_statName[0]));
            if (strncmp(name, "VMM\\", 4) == 0 || strncmp(name, "KERNEL\\", 7) == 0) { wsprintf(msg, "Counter: %.100s", name); Log(msg); }
        }
        RegCloseKey(k);
        SwitchStats("PerfStats\\StartStat");
        if (RegOpenKeyEx(MY_HKEY_DYN_DATA, "PerfStats\\StatData", 0, KEY_READ, &g_perf) != ERROR_SUCCESS) g_perf = NULL;
    }
    rs = LoadLibrary("RSRC32.DLL");
    if (rs) g_resources = (RESOURCES_FN)GetProcAddress(rs, "_MyGetFreeSystemResources32@4");
    ReadStats();
    wsprintf(msg, "Gauges: counters %s, system resources %s. Raw: processor %lu, cache %lu, free %lu, allocated %lu, swap %lu (%s)",
             g_perf ? "yes" : "no", g_resources ? "yes" : "no", g_statRaw[STAT_CPU], g_statRaw[STAT_CACHE], g_statRaw[STAT_FREE],
             g_statRaw[STAT_COMMIT], g_statRaw[STAT_SWAP], g_statBytes ? "bytes" : "pages");
    Log(msg);
}

void StopStats(void)
{
    if (!g_perf) return;
    RegCloseKey(g_perf);
    g_perf = NULL;
    SwitchStats("PerfStats\\StopStat");
}

/* A memory counter in KB, or 0xFFFFFFFF if Windows did not give it. */
static DWORD StatK(int s)
{
    DWORD v = g_statRaw[s];
    if (v == 0xFFFFFFFF) return v;
    if (g_statBytes) return v / 1024;
    if (v > 0xFFFFF) return 0xFFFFFFFF;
    return v * 4;
}

void ReadStats(void)
{
    MEMORYSTATUS ms;
    DWORD total, freeK, cacheK, usedK;
    int s;
    if (g_testMode) {
        g_cpu = 12; g_resFree = 78; g_memTotalK = 1024 * 1024; g_memUsedK = 212 * 1024;
        g_memCacheK = 300 * 1024; g_memFreeK = 512 * 1024; g_memSwapK = 0;
        return;
    }
    for (s = 0; s < STAT_COUNT; s++) g_statRaw[s] = PerfValue(g_statName[s]);
    g_cpu = g_statRaw[STAT_CPU] <= 100 ? (int)g_statRaw[STAT_CPU] : -1;
    ZeroMemory(&ms, sizeof(ms));
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatus(&ms);
    total = ms.dwTotalPhys / 1024;
    /* The memory counters are in bytes or in pages of 4 KB. More than a million
     * pages of allocated memory cannot be, so a number that large means bytes. */
    g_statBytes = g_statRaw[STAT_COMMIT] != 0xFFFFFFFF && g_statRaw[STAT_COMMIT] > 0x100000;
    freeK = StatK(STAT_FREE);
    if (freeK == 0xFFFFFFFF || freeK > total) freeK = ms.dwAvailPhys / 1024;
    cacheK = StatK(STAT_CACHE);
    g_cacheKnown = cacheK != 0xFFFFFFFF && cacheK <= total;
    if (!g_cacheKnown) cacheK = 0;
    /* Windows 98 fills spare memory with its disk cache and hands it back when
     * a program needs it, so the cache does not count as used. */
    usedK = total > freeK ? total - freeK : 0;
    usedK = usedK > cacheK ? usedK - cacheK : 0;
    g_memTotalK = total;
    g_memUsedK = usedK;
    g_memFreeK = freeK;
    g_memCacheK = cacheK;
    g_memSwapK = StatK(STAT_SWAP);
    g_resFree = g_resUser = g_resGdi = -1;
    if (g_resources) {
        g_resFree = (int)g_resources(0);
        g_resGdi = (int)g_resources(1);
        g_resUser = (int)g_resources(2);
        if (g_resFree < 0) g_resFree = 0;
        if (g_resFree > 100) g_resFree = 100;
    }
}

/* The program file and the libraries a program has loaded, with where each
 * sits in its memory. With "log" set they are written to the log as well. */
static int ListModules(DWORD pid, MODSPAN *out, int max, int log)
{
    HMODULE k = GetModuleHandle("KERNEL32.DLL");
    SNAPSHOT_FN snap = (SNAPSHOT_FN)GetProcAddress(k, "CreateToolhelp32Snapshot");
    MODULE_FN first = (MODULE_FN)GetProcAddress(k, "Module32First");
    MODULE_FN next = (MODULE_FN)GetProcAddress(k, "Module32Next");
    MY_MODULEENTRY32 me;
    HANDLE h;
    const char *ext;
    char msg[200];
    int n = 0;
    if (!snap || !first || !next) return 0;
    h = snap(MY_SNAPMODULE, pid);
    if (!h || h == INVALID_HANDLE_VALUE) return 0;
    ZeroMemory(&me, sizeof(me));
    me.dwSize = sizeof(me);
    if (first(h, &me)) {
        do {
            if (n < max) {
                ext = strrchr(me.szExePath, '.');
                out[n].base = (DWORD)me.modBaseAddr;
                out[n].size = me.modBaseSize;
                out[n].program = ext && lstrcmpi(ext, ".exe") == 0;
                n++;
            }
            if (log) {
                wsprintf(msg, "Module: %.60s at %08lX, %lu K", me.szModule, (DWORD)me.modBaseAddr, me.modBaseSize / 1024);
                Log(msg);
            }
            me.dwSize = sizeof(me);
        } while (next(h, &me));
    }
    CloseHandle(h);
    return n;
}

/* Adds up the memory of a program, in KB, in two parts: the program with its
 * own data, and the Windows libraries it has loaded. The libraries are the
 * same for every program that uses them and are in memory once, so they say
 * little about the program itself. */
void MeasureProcess(DWORD pid, MEMUSE *use, int log)
{
    static MODSPAN mods[MAX_MODULES];
    HANDLE p = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    MEMORY_BASIC_INFORMATION mi;
    SYSTEM_INFO si;
    DWORD a, end = 0x7FFF0000, at, kb;
    int guard = 0, count, i, library;
    ZeroMemory(use, sizeof(MEMUSE));
    if (!p) return;
    count = ListModules(pid, mods, MAX_MODULES, log);
    for (i = 0; i < count; i++) if (!mods[i].program && mods[i].base < end) use->libs++;
    GetSystemInfo(&si);
    a = (DWORD)si.lpMinimumApplicationAddress;
    while (a < end && guard++ < 4000) {
        if (VirtualQueryEx(p, (void *)a, &mi, sizeof(mi)) != sizeof(mi)) break;
        if (!mi.RegionSize) break;
        at = (DWORD)mi.BaseAddress;
        if (mi.State == MEM_COMMIT) {
            kb = (DWORD)(mi.RegionSize / 1024);
            library = 0;
            for (i = 0; i < count; i++) {
                if (at < mods[i].base || at >= mods[i].base + mods[i].size) continue;
                library = !mods[i].program;
                break;
            }
            if (library) use->libK += kb; else use->ownK += kb;
        }
        if (at + (DWORD)mi.RegionSize <= a) break;
        a = at + (DWORD)mi.RegionSize;
    }
    CloseHandle(p);
}

static DWORD ProcessMemoryK(DWORD pid)
{
    MEMUSE use;
    MeasureProcess(pid, &use, 0);
    return use.ownK;
}

/* Measures every program once, when the list is read. It is not repeated
 * while the list is open, so that looking at the processor gauge does not
 * itself keep the processor busy. It gives up after a third of a second. */
static void MeasureTasks(void)
{
    static int logged = 0;
    DWORD started = GetTickCount();
    char msg[120];
    int i, j, done = 0;
    if (g_testMode) return;
    for (i = 0; i < g_taskCount; i++) {
        g_tasks[i].memK = 0;
        for (j = 0; j < i; j++) if (g_tasks[j].pid == g_tasks[i].pid) break;
        if (j < i) { g_tasks[i].memK = g_tasks[j].memK; continue; }
        if (GetTickCount() - started > 330) continue;
        g_tasks[i].memK = ProcessMemoryK(g_tasks[i].pid);
        done++;
    }
    g_measureMs = GetTickCount() - started;
    if (logged++ < 5) {
        wsprintf(msg, "Measured the memory of %d programs in %lu ms", done, g_measureMs);
        Log(msg);
    }
}

/* The tables of running and startup programs, and the icons fetched for
 * them, are only needed while the panel is open. */
void FreeTables(void)
{
    int i;
    for (i = 0; i < g_fileIconCount && g_fileIcons; i++) if (g_fileIcons[i].icon) DestroyIcon(g_fileIcons[i].icon);
    if (g_fileIcons) free(g_fileIcons);
    if (g_tasks) free(g_tasks);
    if (g_procs) free(g_procs);
    if (g_starts) free(g_starts);
    g_fileIcons = NULL; g_tasks = NULL; g_procs = NULL; g_starts = NULL;
    g_fileIconCount = g_taskCount = g_taskWindows = g_procCount = g_startCount = 0;
}
