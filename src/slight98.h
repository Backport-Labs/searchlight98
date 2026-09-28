/* slight98.h - Searchlight 98: what the files of the program share.
 *
 * Constants, types, the shared state, and the functions each file offers to
 * the others. What a file keeps to itself is declared static in that file.
 */
#ifndef SLIGHT98_H
#define SLIGHT98_H

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ------------------------------------------------------------------------
 * Declarations Tiny C Compiler's headers do not have
 * --------------------------------------------------------------------- */
typedef struct { DWORD d1; WORD d2, d3; BYTE d4[8]; } MYGUID;

typedef struct ShellLink_ ShellLink;
typedef struct {
    HRESULT (WINAPI *QueryInterface)(ShellLink *, const MYGUID *, void **);
    ULONG   (WINAPI *AddRef)(ShellLink *);
    ULONG   (WINAPI *Release)(ShellLink *);
    HRESULT (WINAPI *GetPath)(ShellLink *, LPSTR, int, WIN32_FIND_DATAA *, DWORD);
    void *GetIDList, *SetIDList, *GetDescription, *SetDescription;
    void *GetWorkingDirectory, *SetWorkingDirectory, *GetArguments, *SetArguments;
    void *GetHotkey, *SetHotkey, *GetShowCmd, *SetShowCmd;
    HRESULT (WINAPI *GetIconLocation)(ShellLink *, LPSTR, int, int *);
    void *SetIconLocation, *SetRelativePath, *Resolve, *SetPath;
} ShellLinkVtbl;
struct ShellLink_ { ShellLinkVtbl *v; };

typedef struct PersistFile_ PersistFile;
typedef struct {
    HRESULT (WINAPI *QueryInterface)(PersistFile *, const MYGUID *, void **);
    ULONG   (WINAPI *AddRef)(PersistFile *);
    ULONG   (WINAPI *Release)(PersistFile *);
    void *GetClassID, *IsDirty;
    HRESULT (WINAPI *Load)(PersistFile *, const WCHAR *, DWORD);
    void *Save, *SaveCompleted, *GetCurFile;
} PersistFileVtbl;
struct PersistFile_ { PersistFileVtbl *v; };

#ifndef CP_ACP
#define CP_ACP 0
int WINAPI MultiByteToWideChar(UINT, DWORD, LPCSTR, int, LPWSTR, int);
#endif

HRESULT WINAPI CoInitialize(void *);
void    WINAPI CoUninitialize(void);
HRESULT WINAPI CoCreateInstance(const MYGUID *, void *, DWORD, const MYGUID *, void **);
void    WINAPI CoTaskMemFree(void *);

typedef struct {
    HICON hIcon;
    int iIcon;
    DWORD dwAttributes;
    CHAR szDisplayName[MAX_PATH];
    CHAR szTypeName[80];
} MY_SHFILEINFOA;
typedef struct {
    DWORD cbSize;
    HWND hWnd;
    UINT uID;
    UINT uFlags;
    UINT uCallbackMessage;
    HICON hIcon;
    CHAR szTip[64];
} MY_NOTIFYICONDATAA;
DWORD     WINAPI SHGetFileInfoA(LPCSTR, DWORD, MY_SHFILEINFOA *, UINT, UINT);
UINT      WINAPI ExtractIconExA(LPCSTR, int, HICON *, HICON *, UINT);
BOOL      WINAPI Shell_NotifyIconA(DWORD, MY_NOTIFYICONDATAA *);
HINSTANCE WINAPI ShellExecuteA(HWND, LPCSTR, LPCSTR, LPCSTR, LPCSTR, int);
typedef struct {
    DWORD cbSize;
    ULONG fMask;
    HWND hwnd;
    LPCSTR lpVerb, lpFile, lpParameters, lpDirectory;
    int nShow;
    HINSTANCE hInstApp;
    void *lpIDList;
    LPCSTR lpClass;
    HKEY hkeyClass;
    DWORD dwHotKey;
    HANDLE hIcon, hProcess;
} MY_SHELLEXECUTEINFOA;
BOOL      WINAPI ShellExecuteExA(MY_SHELLEXECUTEINFOA *);
#define MY_SEE_MASK_INVOKEIDLIST 0x0C
HRESULT   WINAPI SHGetSpecialFolderLocation(HWND, int, void **);
BOOL      WINAPI SHGetPathFromIDListA(void *, LPSTR);

#define MY_SHGFI_ICON      0x100
#define MY_SHGFI_SMALLICON 0x001
#define MY_NIM_ADD    0
#define MY_NIM_MODIFY 1
#define MY_NIM_DELETE 2
#define MY_NIF_MESSAGE 1
#define MY_NIF_ICON    2
#define MY_NIF_TIP     4

/* The list of running programs. Windows 95/98 have it in KERNEL32; Windows
 * NT 4.0 does not, so it is looked up when the program runs. */
typedef struct {
    DWORD dwSize, cntUsage, th32ProcessID, th32DefaultHeapID, th32ModuleID, cntThreads, th32ParentProcessID;
    LONG  pcPriClassBase;
    DWORD dwFlags;
    CHAR  szExeFile[MAX_PATH];
} MY_PROCESSENTRY32;
typedef HANDLE (WINAPI *SNAPSHOT_FN)(DWORD, DWORD);
typedef BOOL   (WINAPI *PROCESS_FN)(HANDLE, MY_PROCESSENTRY32 *);
#define MY_SNAPPROCESS 2
#define MY_SNAPMODULE  8
typedef struct {
    DWORD dwSize, th32ModuleID, th32ProcessID, GlblcntUsage, ProccntUsage;
    BYTE *modBaseAddr;
    DWORD modBaseSize;
    HMODULE hModule;
    CHAR  szModule[256];
    CHAR  szExePath[MAX_PATH];
} MY_MODULEENTRY32;
typedef BOOL   (WINAPI *MODULE_FN)(HANDLE, MY_MODULEENTRY32 *);
#define MY_ICON_SMALL  0
#define MY_ICON_BIG    1
#define MY_GCL_HICON   (-14)
#define MY_GCL_HICONSM (-34)

/* Windows 95/98 keep their performance counters in this part of the registry. */
#define MY_HKEY_DYN_DATA ((HKEY)0x80000006)
typedef LONG (WINAPI *RESOURCES_FN)(int);

/* ------------------------------------------------------------------------
 * Constants, types and the state the files share
 * --------------------------------------------------------------------- */
#define APP_NAME     "Searchlight 98"
#define APP_VERSION  "0.3.3"

#define WM_TRAY      (WM_APP + 1)
#define ID_OPEN      101
#define ID_SETTINGS  102
#define ID_ABOUT     103
#define ID_EXIT      104
#define ID_RESCAN    105
#define ID_OPENRUN   107
#define ID_HELP      109
#define ID_NOTIMER   110
#define ID_OPENSTART 111
#define ID_OPTIONS   112
#define ID_O_ICONP   301     /* the Options box */
#define ID_O_ICONR   302
#define ID_O_ICONS   303
#define ID_O_LAZY    304
#define ID_O_DIM     305
#define ID_O_OPACITY 306
#define ID_O_PERCENT 307
#define ID_O_FAVKEYS 308

/* the menu that a right-click opens */
#define CMD_OPEN     1
#define CMD_FOLDER   2
#define CMD_PROPS    3
#define CMD_PIN      4
#define CMD_FORGET   5
#define CMD_HIDE     6
#define CMD_UNINSTALL 7
#define CMD_SWITCH   8
#define CMD_CLOSE    9
#define CMD_END      10
#define CMD_TOGGLE   11

/* hotkeys: 1 opens the panel, 2 the running programs, 10 to 14 start the favorites */
#define HOTKEY_FAV   10
#define ID_CTRL      201
#define ID_ALT       202
#define ID_SHIFT     203
#define ID_KEY       204
#define ID_DEFAULT   205
#define ID_SECOND    10      /* added to the four above for the second hotkey */
#define ID_EDIT      800
#define ID_RESULTS   900
#define ID_TASKS     901
#define ID_STARTS    902
#define ID_LIST0     1000

#define MAX_CATS     24
#define FIXED_CATS   11
#define MAX_PINS     5
#define MAX_RESULTS  50
#define MAX_DOCS     1000
#define DOC_DEPTH    4
#define ROW_CALC     0x40000000      /* rows of the results list that are not programs */
#define ROW_RUN      0x40000001
#define ROW_MODE     0x40000002
#define ROW_TIMER    0x40000003
#define GAUGE_H      26
#define MAX_TASKS    96
#define MAX_PROCS    128
#define MAX_STARTS   64
#define MAX_USES     200
#define MAX_OVER     128
#define MAX_FILEICONS 64
#define POOL_BLOCK   8192    /* text of the program list is kept in blocks this large */
#define ICON_SMALL_TRIED 1
#define ICON_LARGE_TRIED 2
#define ICON_SMALL_OWN   4   /* the icon is ours to destroy */
#define ICON_LARGE_OWN   8
#define DIM_NONE     0       /* what the backdrop shows */
#define DIM_PICTURE  1
#define DIM_BLACK    2
#define DIM_BAND     32      /* rows enlarged at a time while painting */
#define DIM_DEFAULT  55      /* percent */
#define VIEW_PROGRAMS 0
#define VIEW_RUNNING  1
#define VIEW_STARTUP  2
#define VIEW_COUNT    3
#define DROP_NONE    (-1)
#define DROP_FULL    (-2)
#define CAT_SETTINGS 8
#define CAT_OTHER    9
#define CAT_DOCS     10      /* found by searching, never shown as a group */

#define ROW_H        18      /* height of a row in a group list */
#define RESULT_H     22      /* height of a row in the results list */
#define TITLE_H      18
#define MARGIN       8
extern const char CLASS_PANEL[];
extern const char CLASS_BACK[];
extern const char CLASS_SET[];
extern const char CLASS_ABOUT[];
extern const char CLASS_OPT[];

/* The text of an entry is not kept in the entry itself but in a pool, packed
 * end to end. An entry with fixed room for every text would be five times
 * the size, most of it empty. */
typedef struct {
    const char *name;      /* as shown */
    const char *lname;     /* lower case */
    const char *inits;     /* first letter of each word */
    const char *keys;      /* lower case folder names or search words */
    const char *path;      /* shortcut file, or command line for settings */
    const char *icon;      /* settings only: "file,index" */
    int kind;              /* 0 = program, 1 = setting, 2 = document */
    int cat;
    int uses;              /* how often it was started from Searchlight */
    HICON small, large;    /* loaded when first shown */
    int iconState;         /* ICON_ flags */
} ITEM;

typedef struct POOL_ { struct POOL_ *next; int used, size; char text[1]; } POOL;

extern ITEM *g_items;
extern int g_count, g_cap;
extern POOL *g_pool;
extern int g_iconsPrograms, g_iconsRunning, g_iconsStartup, g_iconsLazy;

extern char g_catName[MAX_CATS][40];
extern int g_catCount;
extern int g_catItems[MAX_CATS];

extern char g_favs[MAX_PINS][110], g_recent[MAX_PINS][110];
extern int g_favCount, g_recentCount;

extern char g_dir[MAX_PATH];
extern HINSTANCE g_inst;
extern HWND g_main, g_panel, g_settings, g_options;
extern HWND g_edit, g_results, g_list[MAX_CATS], g_hoverList;
extern int g_listCat[MAX_CATS], g_listCount;
extern RECT g_rcCaption, g_rcClose, g_rcFav, g_rcRec, g_rcStatus, g_rcContent, g_rcPanel[MAX_CATS];
extern int g_panelW, g_panelH, g_panelX, g_panelY;
extern HWND g_back;                     /* full-screen backdrop behind the panel */
extern int g_dimSW, g_dimSH, g_dimStride, g_dimMode;
extern int g_dimW, g_dimH, g_dimOn, g_dimSlow, g_dimOpacity;
extern int g_res[MAX_RESULTS], g_resCount, g_queryMode;
extern int g_hoverBox, g_hoverTile;
extern char g_status[200];
extern HFONT g_font, g_fontBold, g_fontEdit;
extern HICON g_appIcon, g_appIconLarge, g_defIcon;
extern UINT g_mods, g_vk;
extern int g_hotkeyOk, g_testMode;
extern DWORD g_lastScan;
extern FILE *g_log;
extern ShellLink *g_link;
extern PersistFile *g_linkFile;

/* One row of the Running list: a window, or a program without a window. */
typedef struct {
    HWND  hwnd;            /* NULL for a program that runs in the background */
    DWORD pid;
    char  title[100];      /* as shown */
    char  words[150];      /* lower case title and file name, for the filter */
    char  exe[40];         /* file name of the program */
    DWORD memK;            /* memory of the program and its data, without shared libraries, in KB; 0 = not known */
    HICON icon;
    int   hung;            /* the window does not answer */
    int   locked;          /* part of Windows: can be closed, never ended */
} TASK;
typedef struct { DWORD ownK, libK; int libs; } MEMUSE;       /* the memory of one program */
typedef struct { char path[MAX_PATH]; HICON icon; } FILEICON;

extern TASK *g_tasks;                   /* these tables exist while the panel is open */
extern int g_taskCount, g_taskWindows;
extern int g_view;
extern HWND g_taskList, g_startList, g_about;
extern RECT g_rcTab[VIEW_COUNT];

/* One row of the Startup list: something that starts together with Windows. */
#define SRC_FOLDER   3      /* 0 to 2 are the places in the registry */
#define SRC_FOLDERS  4      /* the Startup folder for all users */
#define SRC_WININI   5
typedef struct {
    char  name[100];
    char  words[300];      /* lower case name and command, for the filter */
    char  command[MAX_PATH];
    char  folder[MAX_PATH];/* SRC_FOLDER: the Startup folder it belongs in */
    int   source, enabled;
    HICON icon;
} START;
extern START *g_starts;
extern int g_startCount;

/* How often each program was started, so that habits count in the search. */
typedef struct { char id[110]; int n; } USE;
extern USE *g_uses;
extern int g_useCount, g_useCap;

/* Favorites on Ctrl+Alt+1 to 5, a screen mode or a countdown typed in the box. */
extern int g_favKeys, g_favKeysOn;
extern int g_modeOk, g_modeW, g_modeH, g_modeBpp;
extern int g_timerOk, g_timerAction, g_timerMinutes;
extern char g_modeLabel[120], g_timerLabel[120];
extern DWORD g_offAt;                   /* tick count at which Windows is shut down or restarted; 0 = no countdown */
extern int g_offAction;
extern char g_offTime[16];
extern int g_dropGroup;                 /* group a dragged program would move to */
extern int g_keyLogs;
extern HWND g_closing;                  /* window that was asked to close */
extern int g_closeTicks, g_modal;
extern FILEICON *g_fileIcons;
extern int g_fileIconCount;

/* The gauges above the Running list. -1 = this version of Windows does not say. */
extern RECT g_rcGauge;
extern int g_cpu, g_resFree, g_statTicks;
extern DWORD g_memTotalK, g_memUsedK, g_memFreeK, g_memCacheK, g_memSwapK, g_measureMs;
extern int g_resUser, g_resGdi, g_cacheKnown, g_statBytes;

/* The counters Searchlight reads, as Windows 95/98 name them. */
#define STAT_CPU     0
#define STAT_CACHE   1
#define STAT_FREE    2
#define STAT_COMMIT  3
#define STAT_SWAP    4
#define STAT_COUNT   5
extern DWORD g_statRaw[STAT_COUNT];

/* The extra rows of the results list: a sum that was typed, something to run. */
extern int g_calcOk, g_runOk, g_docCount;
extern char g_calcText[48], g_runFile[MAX_PATH], g_runArgs[120], g_runLabel[160];
extern UINT g_mods2, g_vk2;
extern int g_hotkey2Ok;

/* Dragging a program to Favorites. */
extern int g_dragItem, g_dragging, g_dropPos;

typedef struct { UINT vk; char name[12]; } KEYNAME;
extern KEYNAME g_keys[80];
extern int g_keyCount;

/* ------------------------------------------------------------------------
 * What each file offers to the others
 * --------------------------------------------------------------------- */

/* common.c: shared state and small helpers */
void PathIn(char *out, const char *name);
int FileExists(const char *p);
void CopyN(char *dst, const char *src, int size);
void Lower(char *s);
int IsWordChar(char c);
int ScreenW(void);
int ScreenH(void);
void Log(const char *text);
void Trim(char *s);

/* catalog.c: the list of programs, settings and documents, and how it is filled */
/* Shortcuts with these words are left out. A trailing $ means "ends with". */
extern const char g_skip[];
int MatchWord(const char *text, const char *word, int len);
int MatchAny(const char *text, const char *words);
int Categorize(const char *lname, const char *lfolder);
void MakeId(const ITEM *it, char *out);
int FindById(const char *id);
int IsPinned(char list[][110], int count, const ITEM *it);
ITEM *AddItem(int kind, const char *name, const char *path, int cat, const char *keys, const char *icon);
int SetOverride(const char *file, const char *name, const char *group);
int DosModeFile(char *out);
int CompareItems(const void *a, const void *b);
void AddSettings(void);
void Scan(void);

/* state.c: favorites, recent programs and usage counts, kept in STATE.TXT */
void LoadState(void);
int CountUse(const char *id);
void ApplyUses(void);
void SaveState(void);
void PrunePins(char list[][110], int *count);
int PinMove(const char *which, int pos);
void PinRemove(int p);

/* icons.c: icons of programs and files */
void FreeIcons(ITEM *items, int count);
void IndexedIcons(const char *file, int index, HICON *large, HICON *small);
void LinkTarget(const char *path, char *target);
HICON LargeIcon(ITEM *it);
HICON SmallIcon(ITEM *it);
void StartEager(void);
void EagerTick(void);
HICON EmbeddedIcon(int size);
HICON SmallFileIcon(const char *path);

/* search.c: searching the list */
void Search(const char *query);

/* commands.c: what can be typed besides a name: sums, things to run, screen modes, countdowns */
int Calculate(const char *text, char *out);
int RunTarget(const char *text);
int ParseMode(const char *text);
int ParseTimer(const char *text);
int PowerAction(const char *path);
void AskPower(int action);
void StopCountdown(void);
void StartCountdown(void);
void CountdownTick(void);
void ApplyMode(void);

/* running.c: running programs, their memory, and the gauges */
extern int g_procCount;
const char *BaseName(const char *path);
void BuildTasks(void);
int TaskMatches(const TASK *t, const char *query);
void StartStats(void);
void StopStats(void);
void ReadStats(void);
void MeasureProcess(DWORD pid, MEMUSE *use, int log);
void FreeTables(void);

/* startup.c: programs that start together with Windows */
const char *SourceLabel(int source);
void BuildStarts(void);
int StartMatches(const START *s, const char *query);
int ToggleStart(START *s);

/* hotkey.c: hotkeys and the settings kept in the registry */
void BuildKeyList(void);
void HotkeyText(UINT mods, UINT vk, char *out);
void LoadHotkey(void);
void SaveHotkey(void);
void SetHint(void);
int ApplyHotkey(UINT mods, UINT vk);
int ApplyHotkey2(UINT mods, UINT vk);
void ApplyFavKeys(void);

/* backdrop.c: the blurred, darkened picture behind the panel */
void FakeDesktop(HDC dc, int w, int h);
void FreeBackdrop(void);
void MakeBackdrop(void);
void PaintBackdrop(HDC dc, const RECT *area);
LRESULT CALLBACK BackProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

/* panel.c: the panel: layout, what it shows, opening and closing */
void FillLists(void);
void Layout(void);
int SelectedTask(void);
void RefreshTasks(void);
int SelectedStart(void);
void Flash(const char *text);
void Rescan(int quiet);
void UpdateQuery(void);
void SetView(int view);
void HidePanel(void);
void ShowPanel(int view);
void TogglePanel(void);
void RunningHotkey(void);

/* actions.c: what happens when something on the panel is chosen */
int UninstallMatches(const char *dir, const char *lname, const char *display, const char *command);
int Ask(const char *text, UINT flags);
void PinAt(int index, int pos);
void Unpin(int index);
void ToggleFav(int index);
void SwitchTo(int t);
void KillTask(int t);
void CloseTask(int t);
void WatchClosing(void);
void Launch(int index);
void Activate(int row);
void ForgetRecent(int tile);
void Details(void);
void MoveToGroup(int index, int cat);
void ItemMenu(int index, int recent);
void TaskMenu(int t);
void ToggleStartRow(int i);
void StartMenu(int i);
void Help(void);
void FavoriteHotkey(int place);

/* draw.c: drawing the panel */
void Gradient(HDC dc, const RECT *rc);
void TileRect(const RECT *box, int i, RECT *out);
void TileX(const RECT *tile, RECT *out);
void RedrawGauges(void);
void Paint(HDC dc);
void TaskX(const RECT *row, RECT *out);
void DrawRow(const DRAWITEMSTRUCT *d);

/* input.c: mouse and keyboard on the panel */
void DragReset(void);
LRESULT CALLBACK PanelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
void CreatePanel(void);

/* dialogs.c: the Hotkeys, Options and About boxes */
#define ABOUT_W 400
#define ABOUT_H 286
#define OPT_W 360
#define OPT_H 332
LRESULT CALLBACK SettingsProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
LRESULT CALLBACK AboutProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
HWND MakeBox(const char *cls, const char *title, int cw, int ch);
void About(void);
LRESULT CALLBACK OptionsProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
void OpenOptions(void);
void OpenSettings(void);

/* test.c: test modes: the self-test and rendering the panel to a bitmap */
int SelfTest(void);
int Shot(const char *file, const char *query);

/* main.c: start-up, the background window and the tray icon */
void UpdateTray(int add);

#endif
