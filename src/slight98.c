/* SLIGHT98.EXE - Searchlight 98, a quick launcher for Windows 95/98.
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
 * Built with Tiny C Compiler. Uses only APIs present on Windows 95/98.
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "icon.h"

/* ------------------------------------------------------------------------
 * Declarations Tiny C Compiler's headers do not have
 * --------------------------------------------------------------------- */
typedef struct { DWORD d1; WORD d2, d3; BYTE d4[8]; } MYGUID;
static const MYGUID CLSID_ShellLink_ = { 0x00021401, 0, 0, { 0xC0, 0, 0, 0, 0, 0, 0, 0x46 } };
static const MYGUID IID_IShellLinkA_ = { 0x000214EE, 0, 0, { 0xC0, 0, 0, 0, 0, 0, 0, 0x46 } };
static const MYGUID IID_IPersistFile_ = { 0x0000010B, 0, 0, { 0xC0, 0, 0, 0, 0, 0, 0, 0x46 } };

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
 * Constants and data
 * --------------------------------------------------------------------- */
#define APP_NAME     "Searchlight 98"
#define APP_VERSION  "0.3.2"

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

static const char CLASS_MAIN[]  = "Searchlight98Daemon";
static const char CLASS_PANEL[] = "Searchlight98Panel";
static const char CLASS_BACK[]  = "Searchlight98Backdrop";
static const char CLASS_SET[]   = "Searchlight98Settings";
static const char CLASS_ABOUT[] = "Searchlight98About";
static const char CLASS_OPT[]   = "Searchlight98Options";
static const char APP_KEY[]     = "Software\\Searchlight 98";
static const char RUN_KEY[]     = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";

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

static ITEM *g_items = NULL;
static int g_count = 0, g_cap = 0;
static POOL *g_pool = NULL;
static int g_iconsPrograms = 1, g_iconsRunning = 1, g_iconsStartup = 1, g_iconsLazy = 1;

static char g_catName[MAX_CATS][40] = {
    "Games", "Internet", "Office", "Media & Graphics", "Development",
    "Utilities", "Accessories", "System", "Settings", "Other", "Documents"
};
static int g_catCount = FIXED_CATS;
static int g_catItems[MAX_CATS];

static char g_favs[MAX_PINS][110], g_recent[MAX_PINS][110];
static int g_favCount = 0, g_recentCount = 0;

typedef struct { char lname[100]; char cat[40]; } OVERRIDE;
static OVERRIDE *g_over = NULL;              /* only while the Start Menu is scanned */
static int g_overCount = 0;

static char   g_dir[MAX_PATH];
static HINSTANCE g_inst;
static HWND   g_main = NULL, g_panel = NULL, g_settings = NULL, g_options = NULL;
static HWND   g_edit = NULL, g_results = NULL, g_list[MAX_CATS], g_hoverList = NULL;
static WNDPROC g_oldEdit = NULL, g_oldList = NULL;
static int    g_listCat[MAX_CATS], g_listCount = 0;
static RECT   g_rcCaption, g_rcClose, g_rcFav, g_rcRec, g_rcStatus, g_rcContent, g_rcPanel[MAX_CATS];
static int    g_panelW = 0, g_panelH = 0, g_panelX = 0, g_panelY = 0;
static HWND   g_back = NULL;            /* full-screen backdrop behind the panel */
static BYTE  *g_dimBits = NULL;         /* the screen at quarter size, blurred and darkened */
static int    g_dimSW = 0, g_dimSH = 0, g_dimStride = 0, g_dimMode = DIM_NONE;
static int    g_dimW = 0, g_dimH = 0, g_dimOn = 1, g_dimSlow = 0, g_dimOpacity = DIM_DEFAULT;
static int    g_res[MAX_RESULTS], g_resCount = 0, g_queryMode = 0;
static int    g_hoverBox = 0, g_hoverTile = -1;
static char   g_status[200] = "";
static HFONT  g_font, g_fontBold, g_fontEdit;
static HICON  g_appIcon = NULL, g_appIconLarge = NULL, g_defIcon = NULL;
static UINT   g_taskbarMsg = 0;
static UINT   g_mods = MOD_CONTROL, g_vk = VK_SPACE;
static int    g_hotkeyOk = 0, g_testMode = 0;
static DWORD  g_lastScan = 0;
static FILE  *g_log = NULL;
static ShellLink *g_link = NULL;
static PersistFile *g_linkFile = NULL;

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
typedef struct { DWORD pid; char path[MAX_PATH]; } PROC;
typedef struct { DWORD ownK, libK; int libs; } MEMUSE;       /* the memory of one program */
typedef struct { DWORD base, size; int program; } MODSPAN;    /* where a program file or library sits */
#define MAX_MODULES 160
typedef struct { char path[MAX_PATH]; HICON icon; } FILEICON;

static TASK  *g_tasks = NULL;            /* these tables exist while the panel is open */
static int    g_taskCount = 0, g_taskWindows = 0;
static int    g_view = VIEW_PROGRAMS;
static HWND   g_taskList = NULL, g_startList = NULL, g_about = NULL;
static RECT   g_rcTab[VIEW_COUNT];

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
static START *g_starts = NULL;
static int    g_startCount = 0;

/* How often each program was started, so that habits count in the search. */
typedef struct { char id[110]; int n; } USE;
static USE   *g_uses = NULL;
static int    g_useCount = 0, g_useCap = 0;

/* Favorites on Ctrl+Alt+1 to 5, a screen mode or a countdown typed in the box. */
static int    g_favKeys = 1, g_favKeysOn = 0;
static int    g_modeOk = 0, g_modeW = 0, g_modeH = 0, g_modeBpp = 0;
static int    g_timerOk = 0, g_timerAction = 0, g_timerMinutes = 0;
static char   g_modeLabel[120], g_timerLabel[120];
static DWORD  g_offAt = 0;              /* tick count at which Windows is shut down or restarted; 0 = no countdown */
static int    g_offAction = 0;
static char   g_offTime[16];
static int    g_dropGroup = -1;         /* group a dragged program would move to */
static int    g_keyLogs = 0;
static HWND   g_closing = NULL;         /* window that was asked to close */
static int    g_closeTicks = 0, g_modal = 0;
static char   g_closingName[100];
static FILEICON *g_fileIcons = NULL;
static int    g_fileIconCount = 0;

/* The gauges above the Running list. -1 = this version of Windows does not say. */
static RECT   g_rcGauge;
static int    g_cpu = -1, g_resFree = -1, g_statTicks = 0;
static DWORD  g_memTotalK = 0, g_memUsedK = 0, g_memFreeK = 0, g_memCacheK = 0, g_memSwapK = 0, g_measureMs = 0;
static int    g_resUser = -1, g_resGdi = -1, g_cacheKnown = 0, g_statBytes = 0;

/* The counters Searchlight reads, as Windows 95/98 name them. */
#define STAT_CPU     0
#define STAT_CACHE   1
#define STAT_FREE    2
#define STAT_COMMIT  3
#define STAT_SWAP    4
#define STAT_COUNT   5
static const char *g_statWant[STAT_COUNT] = {
    "KERNEL\\CPUUsage", "VMM\\cpgDiskcache", "VMM\\cpgFree", "VMM\\cpgCommit", "VMM\\cpgSwapfileInUse"
};
static char   g_statName[STAT_COUNT][64];
static DWORD  g_statRaw[STAT_COUNT];
static HKEY   g_perf = NULL;
static RESOURCES_FN g_resources = NULL;

/* The extra rows of the results list: a sum that was typed, something to run. */
static int    g_calcOk = 0, g_runOk = 0, g_docCount = 0;
static char   g_calcText[48], g_runFile[MAX_PATH], g_runArgs[120], g_runLabel[160];
static HICON  g_calcIcon = NULL, g_runIcon = NULL;
static UINT   g_mods2 = MOD_CONTROL | MOD_SHIFT, g_vk2 = VK_SPACE;
static int    g_hotkey2Ok = 0;

/* Dragging a program to Favorites. */
static int    g_dragItem = -1, g_dragging = 0, g_dropPos = DROP_NONE;
static POINT  g_dragStart;
static HCURSOR g_dragCursor = NULL;

typedef struct { UINT vk; char name[12]; } KEYNAME;
static KEYNAME g_keys[80];
static int g_keyCount = 0;

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

/* Shortcuts with these words are left out. A trailing $ means "ends with". */
static const char g_skip[] =
    "uninst|un-install|desinstal|deinstall|=remove|=doc|=docs|=online|readme|read me|release notes|=help|manual|license|"
    "licence|website|web site|on the web|registration|=register|=faq|whats new|what's new|documentation|"
    "tutorial|technical support|order form|online services|setup$|install$";

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
#define SET_COUNT ((int)(sizeof(g_set) / sizeof(g_set[0])))

/* ------------------------------------------------------------------------
 * Small helpers
 * --------------------------------------------------------------------- */
static void PathIn(char *out, const char *name) { lstrcpy(out, g_dir); lstrcat(out, "\\"); lstrcat(out, name); }
static int FileExists(const char *p) { return GetFileAttributes(p) != 0xFFFFFFFF; }
static void CopyN(char *dst, const char *src, int size) { lstrcpyn(dst, src, size); }
static void Lower(char *s) { CharLowerBuff(s, lstrlen(s)); }
static int IsWordChar(char c) { return IsCharAlphaNumeric(c) || c == '_'; }
/* Test modes use a fixed screen size of 1024 x 768, so their output is the same everywhere. */
static int ScreenW(void) { return g_testMode ? 1024 : GetSystemMetrics(SM_CXSCREEN); }
static int ScreenH(void) { return g_testMode ? 768 : GetSystemMetrics(SM_CYSCREEN); }

static void Log(const char *text)
{
    SYSTEMTIME t; char stamp[32];
    if (!g_log) return;
    GetLocalTime(&t);
    wsprintf(stamp, "%02d:%02d:%02d  ", t.wHour, t.wMinute, t.wSecond);
    fputs(stamp, g_log); fputs(text, g_log); fputs("\n", g_log); fflush(g_log);
}

/* Tests one rule word against lower-case text. */
static int MatchWord(const char *text, const char *word, int len)
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

static int MatchAny(const char *text, const char *words)
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
static int Categorize(const char *lname, const char *lfolder)
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

static void MakeId(const ITEM *it, char *out)
{
    wsprintf(out, "%s:%s", it->kind == 2 ? "doc" : (it->kind ? "set" : "app"), it->lname);
}

static int FindById(const char *id)
{
    char tmp[110];
    int i;
    for (i = 0; i < g_count; i++) { MakeId(&g_items[i], tmp); if (strcmp(tmp, id) == 0) return i; }
    return -1;
}

static int IsPinned(char list[][110], int count, const ITEM *it)
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

static ITEM *AddItem(int kind, const char *name, const char *path, int cat, const char *keys, const char *icon)
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
 * Settings files
 * --------------------------------------------------------------------- */
static void Trim(char *s)
{
    char *p = s;
    int n;
    while (*p == ' ' || *p == '\t') p++;
    if (p != s) memmove(s, p, lstrlen(p) + 1);
    n = lstrlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) s[--n] = 0;
}

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

/* Makes room for one more usage count. The table grows as it is needed. */
static int RoomForUse(void)
{
    USE *grown;
    if (g_useCount < g_useCap) return 1;
    if (g_useCap >= MAX_USES) return 0;
    grown = (USE *)realloc(g_uses, (g_useCap + 16) * sizeof(USE));
    if (!grown) return 0;
    g_uses = grown; g_useCap += 16;
    return 1;
}

static void LoadState(void)
{
    char path[MAX_PATH], line[200];
    FILE *f;
    g_favCount = g_recentCount = g_useCount = 0;
    PathIn(path, "STATE.TXT");
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        Trim(line);
        if (line[0] == 'F' && line[1] == ':' && g_favCount < MAX_PINS) CopyN(g_favs[g_favCount++], line + 2, 110);
        if (line[0] == 'R' && line[1] == ':' && g_recentCount < MAX_PINS) CopyN(g_recent[g_recentCount++], line + 2, 110);
        if (line[0] == 'N' && line[1] == ':') {                                    /* N:times:program */
            char *colon = strchr(line + 2, ':');
            if (!colon || atoi(line + 2) <= 0 || !RoomForUse()) continue;
            g_uses[g_useCount].n = atoi(line + 2);
            CopyN(g_uses[g_useCount].id, colon + 1, 110);
            g_useCount++;
        }
    }
    fclose(f);
}

/* Counts one more start of a program. Old habits fade: when one count gets
 * large, all of them are halved. Returns the new count. */
static int CountUse(const char *id)
{
    int i, least = 0;
    for (i = 0; i < g_useCount; i++) if (strcmp(g_uses[i].id, id) == 0) break;
    if (i == g_useCount) {
        if (RoomForUse()) g_useCount++;
        else if (!g_useCount) return 0;
        else {                                          /* full: the least used one makes room */
            for (i = 1; i < g_useCount; i++) if (g_uses[i].n < g_uses[least].n) least = i;
            i = least;
        }
        CopyN(g_uses[i].id, id, 110);
        g_uses[i].n = 0;
    }
    if (++g_uses[i].n > 60) {
        int j, n = 0, keep = g_uses[i].n / 2;
        for (j = 0; j < g_useCount; j++) {
            g_uses[j].n /= 2;
            if (g_uses[j].n > 0) { if (n != j) g_uses[n] = g_uses[j]; n++; }
        }
        g_useCount = n;
        return keep;
    }
    return g_uses[i].n;
}

/* Tells every program how often it was started. */
static void ApplyUses(void)
{
    char id[110];
    int i, j;
    for (i = 0; i < g_count; i++) {
        g_items[i].uses = 0;
        if (!g_useCount) continue;
        MakeId(&g_items[i], id);
        for (j = 0; j < g_useCount; j++) if (strcmp(g_uses[j].id, id) == 0) { g_items[i].uses = g_uses[j].n; break; }
    }
}

/* Writes "name=group" into a file like CATEGORY.TXT, in place of any line
 * that was there for the same name. Returns 1 if the file was written. */
static int SetOverride(const char *file, const char *name, const char *group)
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

static void SaveState(void)
{
    char path[MAX_PATH];
    FILE *f;
    int i;
    if (g_testMode) return;
    PathIn(path, "STATE.TXT");
    f = fopen(path, "w");
    if (!f) return;
    for (i = 0; i < g_favCount; i++) fprintf(f, "F:%s\n", g_favs[i]);
    for (i = 0; i < g_recentCount; i++) fprintf(f, "R:%s\n", g_recent[i]);
    for (i = 0; i < g_useCount; i++) fprintf(f, "N:%d:%s\n", g_uses[i].n, g_uses[i].id);
    fclose(f);
}

static void PrunePins(char list[][110], int *count)
{
    int i, n = 0;
    for (i = 0; i < *count; i++) if (FindById(list[i]) >= 0) { if (n != i) lstrcpy(list[n], list[i]); n++; }
    *count = n;
}

/* Puts a program in Favorites in front of place "pos", or moves it there if
 * it is already pinned. Returns 0 = added, 1 = moved, 2 = Favorites is full. */
static int PinMove(const char *which, int pos)
{
    char id[110];
    int i, p = -1;
    CopyN(id, which, sizeof(id));
    for (i = 0; i < g_favCount; i++) if (strcmp(g_favs[i], id) == 0) p = i;
    if (p < 0 && g_favCount >= MAX_PINS) return 2;
    if (p >= 0) {
        if (pos > p) pos--;
        for (i = p; i < g_favCount - 1; i++) lstrcpy(g_favs[i], g_favs[i + 1]);
        g_favCount--;
    }
    if (pos < 0) pos = 0;
    if (pos > g_favCount) pos = g_favCount;
    for (i = g_favCount; i > pos; i--) lstrcpy(g_favs[i], g_favs[i - 1]);
    lstrcpy(g_favs[pos], id);
    g_favCount++;
    return p >= 0 ? 1 : 0;
}

static void PinRemove(int p)
{
    int i;
    if (p < 0 || p >= g_favCount) return;
    for (i = p; i < g_favCount - 1; i++) lstrcpy(g_favs[i], g_favs[i + 1]);
    g_favCount--;
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
static int DosModeFile(char *out)
{
    char path[MAX_PATH];
    GetWindowsDirectory(path, MAX_PATH - 20);
    lstrcat(path, "\\Exit To Dos.pif");
    if (out) lstrcpy(out, path);
    return FileExists(path);
}

static int CompareItems(const void *a, const void *b)
{
    return strcmp(((const ITEM *)a)->lname, ((const ITEM *)b)->lname);
}

static void AddSettings(void)
{
    int i;
    for (i = 0; i < SET_COUNT; i++) {
        /* Windows makes "Exit To Dos" the first time MS-DOS mode is used. Without it there is no way in. */
        if (strcmp(g_set[i][1], "@dos") == 0 && !g_testMode && !DosModeFile(NULL)) continue;
        AddItem(1, g_set[i][0], g_set[i][1], CAT_SETTINGS, g_set[i][2], g_set[i][3]);
    }
}

/* Gives the memory of the icons back. They are loaded again when next shown. */
static void FreeIcons(ITEM *items, int count)
{
    int i;
    for (i = 0; i < count; i++) {
        if (items[i].iconState & ICON_SMALL_OWN) DestroyIcon(items[i].small);
        if (items[i].iconState & ICON_LARGE_OWN) DestroyIcon(items[i].large);
        items[i].small = items[i].large = NULL;
        items[i].iconState = 0;
    }
}

static void Scan(void)
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

/* ------------------------------------------------------------------------
 * Icons
 * --------------------------------------------------------------------- */
/* Either pointer may be NULL when that size is not wanted. */
static void FileIcons(const char *path, HICON *large, HICON *small)
{
    MY_SHFILEINFOA fi;
    if (large) {
        ZeroMemory(&fi, sizeof(fi));
        if (SHGetFileInfoA(path, 0, &fi, sizeof(fi), MY_SHGFI_ICON)) *large = fi.hIcon;
    }
    if (small) {
        ZeroMemory(&fi, sizeof(fi));
        if (SHGetFileInfoA(path, 0, &fi, sizeof(fi), MY_SHGFI_ICON | MY_SHGFI_SMALLICON)) *small = fi.hIcon;
    }
}

static void IndexedIcons(const char *file, int index, HICON *large, HICON *small)
{
    char full[MAX_PATH], *part;
    lstrcpy(full, file);
    if (!FileExists(full) && SearchPath(NULL, file, NULL, MAX_PATH, full, &part) == 0) return;
    ExtractIconExA(full, index, large, small, 1);
}

/* Where a shortcut leads. Gives the file itself if it is not a shortcut. */
static void LinkTarget(const char *path, char *target)
{
    const char *ext = strrchr(path, '.');
    WCHAR wide[MAX_PATH];
    WIN32_FIND_DATAA fd;
    target[0] = 0;
    if (ext && lstrcmpi(ext, ".lnk") == 0 && g_link && g_linkFile) {
        MultiByteToWideChar(CP_ACP, 0, path, -1, wide, MAX_PATH);
        if (g_linkFile->v->Load(g_linkFile, wide, 0) >= 0) g_link->v->GetPath(g_link, target, MAX_PATH, &fd, 0);
    }
    if (!target[0]) CopyN(target, path, MAX_PATH);
}

/* Fetches the icon of a program in one size or both. A size that is not
 * asked for is not loaded, which is what keeps the memory use down: only
 * Favorites and Recent ever show the large one. */
static void FetchIcons(const ITEM *it, HICON *wantLarge, HICON *wantSmall)
{
    HICON l = NULL, s = NULL;
    HICON *large = wantLarge ? &l : NULL, *small = wantSmall ? &s : NULL;
    if (it->kind == 1) {
        char file[48], *comma;
        int index = 0;
        CopyN(file, it->icon, sizeof(file));
        comma = strrchr(file, ',');
        if (comma) { *comma = 0; index = atoi(comma + 1); }
        if (index < 0) {
            char full[MAX_PATH], *part;
            if (SearchPath(NULL, file, NULL, MAX_PATH, full, &part)) FileIcons(full, large, small);
        } else IndexedIcons(file, index, large, small);
    } else {
        const char *ext = strrchr(it->path, '.');
        if (ext && lstrcmpi(ext, ".lnk") == 0 && g_link && g_linkFile) {
            WCHAR wide[MAX_PATH];
            char iconPath[MAX_PATH] = "", expanded[MAX_PATH], target[MAX_PATH] = "";
            int index = 0;
            WIN32_FIND_DATAA fd;
            MultiByteToWideChar(CP_ACP, 0, it->path, -1, wide, MAX_PATH);
            if (g_linkFile->v->Load(g_linkFile, wide, 0) >= 0) {
                g_link->v->GetIconLocation(g_link, iconPath, MAX_PATH, &index);
                g_link->v->GetPath(g_link, target, MAX_PATH, &fd, 0);
                if (iconPath[0]) {
                    ExpandEnvironmentStrings(iconPath, expanded, MAX_PATH);
                    IndexedIcons(expanded, index, large, small);
                }
                if (!s && !l && target[0] && FileExists(target)) FileIcons(target, large, small);
            }
        }
        if (!s && !l) FileIcons(it->path, large, small);
    }
    if (wantLarge) *wantLarge = l;
    if (wantSmall) *wantSmall = s;
}

/* The 32 dot icon, loaded the first time it is asked for. */
static HICON LargeIcon(ITEM *it)
{
    HICON l = NULL;
    if (!(it->iconState & ICON_LARGE_TRIED)) {
        it->iconState |= ICON_LARGE_TRIED;
        FetchIcons(it, &l, NULL);
        if (l) { it->large = l; it->iconState |= ICON_LARGE_OWN; }
    }
    if (it->large) return it->large;
    return it->small ? it->small : g_defIcon;
}

/* The 16 dot icon, loaded the first time it is asked for. */
static HICON SmallIcon(ITEM *it)
{
    HICON s = NULL;
    if (!(it->iconState & ICON_SMALL_TRIED)) {
        it->iconState |= ICON_SMALL_TRIED;
        FetchIcons(it, NULL, &s);
        if (s) { it->small = s; it->iconState |= ICON_SMALL_OWN; }
        else if (LargeIcon(it) != g_defIcon) it->small = it->large;       /* some files only have the large one */
    }
    return it->small ? it->small : g_defIcon;
}

/* With "load icons only when they come into view" switched off, the icons
 * are loaded a few at a time after scanning, so that nothing waits for them. */
static int g_eagerAt = 0;

static void StartEager(void)
{
    if (g_testMode || !g_main) return;
    KillTimer(g_main, 6);
    if (!g_iconsPrograms || g_iconsLazy) return;
    g_eagerAt = 0;
    SetTimer(g_main, 6, 40, NULL);
}

static void EagerTick(void)
{
    int done = 0;
    while (g_eagerAt < g_count && done < 6) {
        ITEM *it = &g_items[g_eagerAt++];
        if (it->kind == 2 || (it->iconState & ICON_SMALL_TRIED)) continue;       /* documents only show in results */
        SmallIcon(it);
        done++;
    }
    if (g_eagerAt >= g_count) { KillTimer(g_main, 6); Log("All program icons are loaded"); }
}

static HICON EmbeddedIcon(int size)
{
    if (size <= 16) return CreateIconFromResourceEx((PBYTE)ICON16, sizeof(ICON16), TRUE, 0x00030000, 16, 16, 0);
    return CreateIconFromResourceEx((PBYTE)ICON32, sizeof(ICON32), TRUE, 0x00030000, 32, 32, 0);
}

/* ------------------------------------------------------------------------
 * Search
 * --------------------------------------------------------------------- */
static int TokenScore(const ITEM *it, const char *words, const char *t)
{
    int n = lstrlen(it->lname), tl = lstrlen(t);
    char cat[40], spaced[104];
    const char *p;
    if (strcmp(it->lname, t) == 0) return 1000;
    if (strncmp(it->lname, t, tl) == 0) return 900 - n;
    wsprintf(spaced, " %s", words);
    for (p = spaced; (p = strstr(p, t)) != NULL; p++) if (p > spaced && p[-1] == ' ') return 700 - n;
    if (tl >= 2 && strncmp(it->inits, t, tl) == 0) return 650;
    if (strstr(it->lname, t)) return 500 - n;
    if (strstr(it->keys, t)) return 300;
    lstrcpy(cat, g_catName[it->cat]); Lower(cat);
    if (strncmp(cat, t, tl) == 0) return 150;
    return 0;
}

typedef struct { int index, score; } HIT;
static int CompareHits(const void *a, const void *b)
{
    const HIT *x = (const HIT *)a, *y = (const HIT *)b;
    if (x->score != y->score) return y->score - x->score;
    return strcmp(g_items[x->index].lname, g_items[y->index].lname);
}

static void Search(const char *query)
{
    char q[120], words[100], *toks[12];
    int tokCount = 0, i, j, hitCount = 0;
    HIT *hits;
    char *p;
    g_resCount = 0;
    CopyN(q, query, sizeof(q));
    Lower(q);
    for (p = strtok(q, " \t"); p && tokCount < 12; p = strtok(NULL, " \t")) toks[tokCount++] = p;
    if (!tokCount || !g_count) return;
    hits = (HIT *)malloc(g_count * sizeof(HIT));
    if (!hits) return;
    for (i = 0; i < g_count; i++) {
        ITEM *it = &g_items[i];
        int total = 0, ok = 1;
        lstrcpy(words, it->lname);
        for (j = 0; words[j]; j++) {
            char c = words[j];
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) words[j] = ' ';
        }
        for (j = 0; j < tokCount; j++) {
            int s = TokenScore(it, words, toks[j]);
            if (!s) { ok = 0; break; }
            total += s;
        }
        if (!ok) continue;
        if (it->kind == 2) total = total / 2 + 1;          /* programs come before documents */
        total += (it->uses > 20 ? 20 : it->uses) * 12;     /* what is started often comes first */
        if (IsPinned(g_favs, g_favCount, it) >= 0) total += 80;
        if (IsPinned(g_recent, g_recentCount, it) >= 0) total += 50;
        hits[hitCount].index = i; hits[hitCount].score = total; hitCount++;
    }
    qsort(hits, hitCount, sizeof(HIT), CompareHits);
    for (i = 0; i < hitCount && i < MAX_RESULTS; i++) g_res[g_resCount++] = hits[i].index;
    free(hits);
}

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
static int g_procCount = 0;

static const char *BaseName(const char *path)
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

/* The small icon of a program file. Looked up once, then remembered. */
static HICON SmallFileIcon(const char *path)
{
    MY_SHFILEINFOA fi;
    int i;
    if (!strchr(path, '\\') || !FileExists(path)) return NULL;
    if (!g_fileIcons) g_fileIcons = (FILEICON *)calloc(MAX_FILEICONS, sizeof(FILEICON));
    if (!g_fileIcons) return NULL;
    for (i = 0; i < g_fileIconCount; i++) if (lstrcmpi(g_fileIcons[i].path, path) == 0) return g_fileIcons[i].icon;
    if (g_fileIconCount >= MAX_FILEICONS) return NULL;
    ZeroMemory(&fi, sizeof(fi));
    if (!SHGetFileInfoA(path, 0, &fi, sizeof(fi), MY_SHGFI_ICON | MY_SHGFI_SMALLICON)) fi.hIcon = NULL;
    lstrcpy(g_fileIcons[g_fileIconCount].path, path);
    g_fileIcons[g_fileIconCount].icon = fi.hIcon;
    g_fileIconCount++;
    return fi.hIcon;
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

static void BuildTasks(void)
{
    g_taskCount = g_taskWindows = 0;
    if (!g_tasks) g_tasks = (TASK *)calloc(MAX_TASKS, sizeof(TASK));
    if (!g_procs) g_procs = (PROC *)calloc(MAX_PROCS, sizeof(PROC));
    if (!g_tasks || !g_procs) return;
    if (g_testMode) FakeTasks(); else ScanTasks();
}

/* Every word typed must be part of the window title or the file name. */
static int TaskMatches(const TASK *t, const char *query)
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

static void ReadStats(void);

static void StartStats(void)
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

static void StopStats(void)
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

static void ReadStats(void)
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
static void MeasureProcess(DWORD pid, MEMUSE *use, int log)
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

/* ------------------------------------------------------------------------
 * Sums and commands typed in the search box
 *
 * "12*4+2" shows the answer as the first result. A web address, the full
 * name of a file or folder, or the name of a program Windows can find, the
 * same as in Start, Run, adds a row that opens it.
 * --------------------------------------------------------------------- */
static const char *g_cp;
static int g_cerr, g_cops;

static double CalcSum(void);
static double CalcSigned(void);

static double CalcAtom(void)
{
    double v = 0, scale = 0.1;
    int digits = 0;
    while (*g_cp == ' ') g_cp++;
    if (*g_cp == '(') {
        g_cp++;
        v = CalcSum();
        while (*g_cp == ' ') g_cp++;
        if (*g_cp == ')') g_cp++; else g_cerr = 1;
        return v;
    }
    if (strncmp(g_cp, "sqrt", 4) == 0) {
        g_cp += 4;
        g_cops++;
        v = CalcAtom();
        if (v < 0) { g_cerr = 1; return 0; }
        return sqrt(v);
    }
    while (*g_cp >= '0' && *g_cp <= '9') { v = v * 10 + (*g_cp++ - '0'); digits++; }
    if ((*g_cp == '.' || *g_cp == ',') && g_cp[1] >= '0' && g_cp[1] <= '9') {       /* 1.5 or 1,5 */
        g_cp++;
        while (*g_cp >= '0' && *g_cp <= '9') { v += (*g_cp++ - '0') * scale; scale /= 10; digits++; }
    }
    if (!digits) g_cerr = 1;
    return v;
}

static double CalcPower(void)
{
    double v = CalcAtom(), e;
    while (*g_cp == ' ') g_cp++;
    if (*g_cp != '^') return v;
    g_cp++;
    g_cops++;
    e = CalcSigned();
    return pow(v, e);
}

static double CalcSigned(void)
{
    while (*g_cp == ' ') g_cp++;
    if (*g_cp == '-') { g_cp++; return -CalcSigned(); }
    if (*g_cp == '+') { g_cp++; return CalcSigned(); }
    return CalcPower();
}

static double CalcTerm(void)
{
    double v = CalcSigned(), d;
    for (;;) {
        while (*g_cp == ' ') g_cp++;
        if (*g_cp == '*') { g_cp++; g_cops++; v *= CalcSigned(); }
        else if (*g_cp == '/') {
            g_cp++; g_cops++;
            d = CalcSigned();
            if (d == 0) { g_cerr = 1; return 0; }
            v /= d;
        } else return v;
    }
}

static double CalcSum(void)
{
    double v = CalcTerm();
    for (;;) {
        while (*g_cp == ' ') g_cp++;
        if (*g_cp == '+') { g_cp++; g_cops++; v += CalcTerm(); }
        else if (*g_cp == '-') { g_cp++; g_cops++; v -= CalcTerm(); }
        else return v;
        if (g_cerr) return 0;
    }
}

/* Works out what was typed, if it is a sum. A number on its own is not one. */
static int Calculate(const char *text, char *out)
{
    char q[120];
    double v;
    CopyN(q, text, sizeof(q));
    Lower(q);
    g_cp = q; g_cerr = 0; g_cops = 0;
    v = CalcSum();
    while (*g_cp == ' ') g_cp++;
    if (g_cerr || *g_cp || !g_cops) return 0;
    if (v != v || v > 1e100 || v < -1e100) return 0;
    if (v == 0) v = 0;
    if (v == floor(v) && fabs(v) < 1e15) sprintf(out, "%.0f", v);
    else sprintf(out, "%.10g", v);
    return 1;
}

static int StartsWith(const char *text, const char *start)
{
    return strncmp(text, start, lstrlen(start)) == 0;
}

/* Decides whether what was typed can be opened or run as it stands. */
static int RunTarget(const char *text)
{
    static const char *ext[] = { ".exe", ".com", ".bat" };
    char t[120], low[120], *sp, *part;
    int i;
    g_runFile[0] = g_runArgs[0] = g_runLabel[0] = 0;
    CopyN(t, text, sizeof(t));
    Trim(t);
    if (lstrlen(t) < 3) return 0;
    lstrcpy(low, t);
    Lower(low);
    if (StartsWith(low, "http://") || StartsWith(low, "https://") || StartsWith(low, "ftp://") || StartsWith(low, "mailto:")) {
        if (strchr(t, ' ')) return 0;
        lstrcpy(g_runFile, t);
        wsprintf(g_runLabel, "Open %s", t);
        return 1;
    }
    if (StartsWith(low, "www.") && !strchr(t, ' ') && lstrlen(t) > 6) {
        wsprintf(g_runFile, "http://%s", t);
        wsprintf(g_runLabel, "Open %s", t);
        return 1;
    }
    if ((t[1] == ':' && t[2] == '\\') || (t[0] == '\\' && t[1] == '\\')) {
        DWORD attr;
        if (FileExists(t)) {
            lstrcpy(g_runFile, t);
            wsprintf(g_runLabel, "Open %s", t);
            return 1;
        }
        sp = strchr(t, ' ');                                   /* a program, then what to give it */
        if (!sp) return 0;
        *sp++ = 0;
        attr = GetFileAttributes(t);
        if (attr == 0xFFFFFFFF || (attr & FILE_ATTRIBUTE_DIRECTORY)) return 0;
        lstrcpy(g_runFile, t);
        CopyN(g_runArgs, sp, sizeof(g_runArgs));
        wsprintf(g_runLabel, "Run %s %s", t, g_runArgs);
        return 1;
    }
    if (strchr(t, '\\') || strchr(t, '/') || strchr(t, ':')) return 0;
    sp = strchr(t, ' ');
    if (sp) *sp++ = 0;
    if (lstrlen(t) < 3) return 0;
    for (i = 0; i < 3; i++) {
        if (!SearchPath(NULL, t, ext[i], MAX_PATH, g_runFile, &part)) continue;
        if (sp) CopyN(g_runArgs, sp, sizeof(g_runArgs));
        if (sp) wsprintf(g_runLabel, "Run %s %s", t, g_runArgs); else wsprintf(g_runLabel, "Run %s", t);
        return 1;
    }
    g_runFile[0] = 0;
    return 0;
}

/* ------------------------------------------------------------------------
 * Screen modes and countdowns typed in the search box
 *
 * "800x600" or "1024x768 16" offers to change the screen. "shutdown in 30"
 * or "restart in 10" offers to do that after so many minutes.
 * --------------------------------------------------------------------- */
static const char *DepthName(int bpp)
{
    if (bpp <= 4) return "16 colors";
    if (bpp <= 8) return "256 colors";
    if (bpp <= 16) return "High Color (16 bit)";
    if (bpp <= 24) return "True Color (24 bit)";
    return "True Color (32 bit)";
}

/* Reads a number and steps past it. -1 if there is none. */
static int ReadNumber(const char **p)
{
    int v = 0, digits = 0;
    while (**p == ' ') (*p)++;
    while (**p >= '0' && **p <= '9' && digits < 6) { v = v * 10 + (**p - '0'); (*p)++; digits++; }
    return digits ? v : -1;
}

/* Can the screen do this? With *bpp 0 it picks the colors in use now, or
 * failing that the most this size can show. */
static int HasMode(int w, int h, int *bpp)
{
    DEVMODE dm;
    HDC dc;
    int i, best = 0, now;
    if (g_testMode) {
        if (!((w == 640 && h == 480) || (w == 800 && h == 600) || (w == 1024 && h == 768))) return 0;
        if (*bpp == 0) *bpp = 16;
        return *bpp == 8 || *bpp == 16 || *bpp == 32;
    }
    dc = GetDC(NULL);
    now = GetDeviceCaps(dc, BITSPIXEL) * GetDeviceCaps(dc, PLANES);
    ReleaseDC(NULL, dc);
    for (i = 0; i < 2000; i++) {
        ZeroMemory(&dm, sizeof(dm));
        dm.dmSize = sizeof(dm);
        if (!EnumDisplaySettings(NULL, i, &dm)) break;
        if ((int)dm.dmPelsWidth != w || (int)dm.dmPelsHeight != h) continue;
        if (*bpp) { if ((int)dm.dmBitsPerPel == *bpp) return 1; continue; }
        if ((int)dm.dmBitsPerPel == now) best = now;
        else if (best != now && (int)dm.dmBitsPerPel > best) best = (int)dm.dmBitsPerPel;
    }
    if (*bpp || !best) return 0;
    *bpp = best;
    return 1;
}

static int ParseMode(const char *text)
{
    const char *p = text;
    int w, h, bpp;
    g_modeLabel[0] = 0;
    w = ReadNumber(&p);
    if (w < 0) return 0;
    while (*p == ' ') p++;
    if (*p != 'x' && *p != 'X') return 0;
    p++;
    h = ReadNumber(&p);
    if (h < 0) return 0;
    bpp = ReadNumber(&p);
    if (bpp < 0) bpp = 0;
    while (*p == ' ') p++;
    if (bpp && (lstrcmpi(p, "bit") == 0 || lstrcmpi(p, "bits") == 0)) p += lstrlen(p);
    if (*p) return 0;
    if (w < 320 || h < 200 || w > 4096 || h > 4096) return 0;
    if (bpp && bpp != 8 && bpp != 16 && bpp != 24 && bpp != 32) return 0;
    if (!HasMode(w, h, &bpp)) return 0;
    g_modeW = w; g_modeH = h; g_modeBpp = bpp;
    wsprintf(g_modeLabel, "Change the screen to %d x %d, %s", w, h, DepthName(bpp));
    return 1;
}

static int ParseTimer(const char *text)
{
    static const struct { const char *words; int action; } starts[] = {
        { "shutdown in ", 1 }, { "shut down in ", 1 }, { "restart in ", 2 }, { "reboot in ", 2 }
    };
    char low[120];
    const char *p = NULL;
    int i, action = 0, minutes;
    g_timerLabel[0] = 0;
    CopyN(low, text, sizeof(low));
    Trim(low);
    Lower(low);
    for (i = 0; i < 4 && !p; i++) if (StartsWith(low, starts[i].words)) { p = low + lstrlen(starts[i].words); action = starts[i].action; }
    if (!p) return 0;
    minutes = ReadNumber(&p);
    if (minutes < 1 || minutes > 1440) return 0;
    while (*p == ' ') p++;
    if (*p && strcmp(p, "m") != 0 && strcmp(p, "min") != 0 && strcmp(p, "mins") != 0 && strcmp(p, "minute") != 0 && strcmp(p, "minutes") != 0) return 0;
    g_timerAction = action; g_timerMinutes = minutes;
    wsprintf(g_timerLabel, "%s Windows in %d minute%s", action == 1 ? "Shut down" : "Restart", minutes, minutes == 1 ? "" : "s");
    return 1;
}

/* ------------------------------------------------------------------------
 * Startup: what starts together with Windows
 *
 * Programs start from three places in the registry, from the Startup folder
 * and from WIN.INI. Switching one off moves it aside, to the places the
 * System Configuration Utility of Windows 98 uses, so either tool can switch
 * it back on: registry entries go to a key with a - after its name,
 * shortcuts to the folder "Disabled Startup Items".
 * --------------------------------------------------------------------- */
static const struct { HKEY root; const char *key; const char *label; } g_startKey[3] = {
    { HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", "Registry" },
    { HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\RunServices", "Registry, as a service" },
    { HKEY_CURRENT_USER,  "Software\\Microsoft\\Windows\\CurrentVersion\\Run", "Registry, this user" }
};

static const char *SourceLabel(int source)
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

static void BuildStarts(void)
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

static int StartMatches(const START *s, const char *query)
{
    char q[120], *p;
    CopyN(q, query, sizeof(q));
    Lower(q);
    for (p = strtok(q, " \t"); p; p = strtok(NULL, " \t")) if (!strstr(s->words, p)) return 0;
    return 1;
}

/* Switches an entry off or back on. Returns 0 if Windows did not allow it.
 * In test mode nothing outside the program is touched. */
static int ToggleStart(START *s)
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

/* The tables of running and startup programs, and the icons fetched for
 * them, are only needed while the panel is open. */
static void FreeTables(void)
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
/* ------------------------------------------------------------------------
 * Finding the program that removes a program
 *
 * Add/Remove Programs keeps its list in the registry. The entry belongs to
 * a program if its command mentions the folder the program is in, or
 * failing that, if its name has the program's name in it.
 * --------------------------------------------------------------------- */
/* Everything in lower case. 2 = the folder matches, 1 = the name, 0 = no. */
static int UninstallMatches(const char *dir, const char *lname, const char *display, const char *command)
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
 * Hotkey
 * --------------------------------------------------------------------- */
static void BuildKeyList(void)
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

static void HotkeyText(UINT mods, UINT vk, char *out)
{
    int i;
    out[0] = 0;
    if (mods & MOD_CONTROL) lstrcat(out, "Ctrl+");
    if (mods & MOD_ALT) lstrcat(out, "Alt+");
    if (mods & MOD_SHIFT) lstrcat(out, "Shift+");
    for (i = 0; i < g_keyCount; i++) if (g_keys[i].vk == vk) { lstrcat(out, g_keys[i].name); return; }
    lstrcat(out, "?");
}

static void LoadHotkey(void)
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

static void SaveHotkey(void)
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

static void SetHint(void)
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

static void UpdateTray(int add);

static int ApplyHotkey(UINT mods, UINT vk)
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
static int ApplyHotkey2(UINT mods, UINT vk)
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
static void ApplyFavKeys(void)
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
/* ------------------------------------------------------------------------
 * The panel: layout
 * --------------------------------------------------------------------- */
static void FillLists(void)
{
    int i, c;
    g_listCount = 0;
    for (c = 0; c < g_catCount; c++) if (g_catItems[c] > 0 && c != CAT_DOCS) g_listCat[g_listCount++] = c;
    for (i = 0; i < MAX_CATS; i++) {
        SendMessage(g_list[i], WM_SETREDRAW, FALSE, 0);
        SendMessage(g_list[i], LB_RESETCONTENT, 0, 0);
    }
    for (i = 0; i < g_count; i++) {
        for (c = 0; c < g_listCount; c++) {
            if (g_listCat[c] == g_items[i].cat) { SendMessage(g_list[c], LB_ADDSTRING, 0, (LPARAM)i); break; }
        }
    }
    for (i = 0; i < MAX_CATS; i++) SendMessage(g_list[i], WM_SETREDRAW, TRUE, 0);
    g_hoverList = NULL;
}

/* Below Favorites and Recent there is one of three things: the groups, the
 * search results, or the running programs. */
static void ShowContent(void)
{
    int i;
    for (i = 0; i < MAX_CATS; i++)
        ShowWindow(g_list[i], (i < g_listCount && g_view == VIEW_PROGRAMS && !g_queryMode) ? SW_SHOWNA : SW_HIDE);
    ShowWindow(g_results, (g_view == VIEW_PROGRAMS && g_queryMode) ? SW_SHOWNA : SW_HIDE);
    ShowWindow(g_taskList, g_view == VIEW_RUNNING ? SW_SHOWNA : SW_HIDE);
    ShowWindow(g_startList, g_view == VIEW_STARTUP ? SW_SHOWNA : SW_HIDE);
}

static void Layout(void)
{
    int sw = ScreenW(), sh = ScreenH();
    int w = sw - 40, cols, rows, colW, listH, y, i, top, fixed, avail, boxW, tabW = 76;
    if (w > 1100) w = 1100;
    if (w < 600) w = sw < 600 ? sw : 600;
    cols = w >= 1060 ? 5 : (w >= 860 ? 4 : 3);
    if (g_listCount > 0 && g_listCount < cols) cols = g_listCount;
    rows = g_listCount ? (g_listCount + cols - 1) / cols : 1;
    colW = (w - MARGIN * (cols + 1)) / cols;

    SetRect(&g_rcCaption, 2, 2, w - 2, 22);
    SetRect(&g_rcClose, w - 20, 4, w - 4, 18);
    y = 22 + MARGIN;
    MoveWindow(g_edit, MARGIN, y, w - 2 * MARGIN - VIEW_COUNT * (tabW + 4), 28, FALSE);
    for (i = 0; i < VIEW_COUNT; i++) {
        int right = w - MARGIN - (VIEW_COUNT - 1 - i) * (tabW + 4);
        SetRect(&g_rcTab[i], right - tabW, y, right, y + 28);
    }
    y += 28 + MARGIN;
    boxW = (w - 3 * MARGIN) / 2;
    SetRect(&g_rcFav, MARGIN, y, MARGIN + boxW, y + 86);
    SetRect(&g_rcRec, w - MARGIN - boxW, y, w - MARGIN, y + 86);
    y += 86 + MARGIN;
    top = y;

    fixed = y + 24 + MARGIN;                     /* everything except the group rows */
    avail = (sh - 60) - fixed;
    listH = avail / rows - TITLE_H - 6 - MARGIN;
    if (listH > 12 * ROW_H + 4) listH = 12 * ROW_H + 4;
    if (listH < 4 * ROW_H + 4) listH = 4 * ROW_H + 4;

    for (i = 0; i < MAX_CATS; i++) {
        if (i < g_listCount) {
            int col = i % cols, row = i / cols;
            int x = MARGIN + col * (colW + MARGIN), py = y + row * (TITLE_H + 6 + listH + MARGIN);
            SetRect(&g_rcPanel[i], x, py, x + colW, py + TITLE_H + 6 + listH);
            MoveWindow(g_list[i], x + 3, py + TITLE_H + 3, colW - 6, listH, FALSE);
        }
    }
    y += rows * (TITLE_H + 6 + listH + MARGIN);
    SetRect(&g_rcContent, MARGIN, top, w - MARGIN, y - MARGIN);
    MoveWindow(g_results, MARGIN + 3, top + TITLE_H + 3, w - 2 * MARGIN - 6, (y - MARGIN) - top - TITLE_H - 6, FALSE);
    MoveWindow(g_startList, MARGIN + 3, top + TITLE_H + 3, w - 2 * MARGIN - 6, (y - MARGIN) - top - TITLE_H - 6, FALSE);
    SetRect(&g_rcGauge, MARGIN + 3, top + TITLE_H + 3, w - MARGIN - 3, top + TITLE_H + 3 + GAUGE_H);
    MoveWindow(g_taskList, MARGIN + 3, g_rcGauge.bottom, w - 2 * MARGIN - 6, (y - MARGIN) - g_rcGauge.bottom - 3, FALSE);
    ShowContent();
    SetRect(&g_rcStatus, MARGIN, y, w - MARGIN, y + 22);
    g_panelW = w;
    g_panelH = y + 22 + MARGIN;
    g_panelX = (sw - g_panelW) / 2;
    g_panelY = (sh - g_panelH) / 3;
    if (g_panelY < 0) g_panelY = 0;
    if (!g_testMode)
        SetWindowPos(g_panel, HWND_TOPMOST, g_panelX, g_panelY, g_panelW, g_panelH, SWP_NOACTIVATE);
    else
        SetWindowPos(g_panel, NULL, 0, 0, g_panelW, g_panelH, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
    InvalidateRect(g_panel, NULL, TRUE);
}

/* ------------------------------------------------------------------------
 * Backdrop
 *
 * While the panel is open, the rest of the screen is covered by a blurred,
 * darkened picture of itself, so the panel stands out. Windows 95 and 98
 * cannot make a window see-through. Instead, just before the panel opens,
 * the screen is copied at quarter size, blurred, darkened and stretched back
 * into a full-screen window that sits behind the panel.
 * --------------------------------------------------------------------- */
#define DIM_SCALE  4       /* the blur works on a copy this many times smaller */

#define DIM_LIMIT  400     /* milliseconds; slower than this and the backdrop turns itself off */

static void HidePanel(void);
static void About(void);
static void Details(void);
static void MoveToGroup(int index, int cat);
static int  PowerAction(const char *path);
static void AskPower(int action);
static void ApplyMode(void);
static void StartCountdown(void);

static void BlurPass(const BYTE *src, BYTE *dst, int count, int lines, int step, int lineStep, int r)
{
    int j, c, i, n = 2 * r + 1;
    for (j = 0; j < lines; j++) {
        for (c = 0; c < 3; c++) {
            const BYTE *s = src + j * lineStep + c;
            BYTE *d = dst + j * lineStep + c;
            int sum = 0;
            for (i = -r; i <= r; i++) { int k = i < 0 ? 0 : (i >= count ? count - 1 : i); sum += s[k * step]; }
            for (i = 0; i < count; i++) {
                int a = i - r, b = i + r + 1;
                d[i * step] = (BYTE)(sum / n);
                if (a < 0) a = 0;
                if (b >= count) b = count - 1;
                sum += s[b * step] - s[a * step];
            }
        }
    }
}

/* Stands in for the screen in test mode, so tests never copy the real one. */
static void FakeDesktop(HDC dc, int w, int h)
{
    RECT r;
    HBRUSH b;
    int i;
    static const COLORREF icons[] = { RGB(255, 255, 0), RGB(255, 0, 0), RGB(0, 0, 255), RGB(255, 255, 255), RGB(0, 200, 0) };
    SetRect(&r, 0, 0, w, h);
    b = CreateSolidBrush(RGB(0, 128, 128)); FillRect(dc, &r, b); DeleteObject(b);
    for (i = 0; i < 5; i++) {
        SetRect(&r, 20, 20 + i * 75, 52, 52 + i * 75);
        b = CreateSolidBrush(icons[i]); FillRect(dc, &r, b); DeleteObject(b);
    }
    SetRect(&r, w / 2, h / 3, w - 20, h - 60);                 /* a window */
    FillRect(dc, &r, (HBRUSH)GetStockObject(WHITE_BRUSH));
    r.bottom = r.top + 20;
    b = CreateSolidBrush(RGB(0, 0, 128)); FillRect(dc, &r, b); DeleteObject(b);
    SetRect(&r, 0, h - 28, w, h);                              /* the taskbar */
    FillRect(dc, &r, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
}

static void FreeBackdrop(void)
{
    if (g_dimBits) free(g_dimBits);
    g_dimBits = NULL;
    g_dimMode = DIM_NONE;
}

/* Stretches one row of the small picture to full width, blending neighbours. */
static void ExpandRow(const BYTE *src, BYTE *dst, int w)
{
    int x;
    for (x = 0; x < w; x++) {
        const BYTE *s = src + (x / DIM_SCALE) * 3;
        int k = x % DIM_SCALE, j = DIM_SCALE - k;
        dst[0] = (BYTE)((s[0] * j + s[3] * k) / DIM_SCALE);
        dst[1] = (BYTE)((s[1] * j + s[4] * k) / DIM_SCALE);
        dst[2] = (BYTE)((s[2] * j + s[5] * k) / DIM_SCALE);
        dst += 3;
    }
}

/* Prepares the backdrop. Only the quarter-size picture is kept; it is
 * enlarged while it is painted. At full opacity the backdrop is black and
 * the screen is not copied at all. */
static void MakeBackdrop(void)
{
    HDC screen, mem;
    HBITMAP old, small;
    BITMAPINFO bi;
    BYTE *sbits = NULL, *tmp;
    int w = ScreenW(), h = ScreenH(), light = 100 - g_dimOpacity;
    int sw = w / DIM_SCALE + 2, sh = h / DIM_SCALE + 2, sstride = (sw * 3 + 3) & ~3;
    int x, y;
    DWORD started = GetTickCount();
    char msg[120];

    FreeBackdrop();
    if (!g_dimOn || g_dimOpacity <= 0 || w <= 0 || h <= 0) return;
    g_dimW = w; g_dimH = h;
    if (g_dimOpacity >= 100) { g_dimMode = DIM_BLACK; return; }
    if (g_dimSlow) return;
    screen = GetDC(NULL);
    if (GetDeviceCaps(screen, BITSPIXEL) * GetDeviceCaps(screen, PLANES) < 15) {     /* needs thousands of colours */
        ReleaseDC(NULL, screen);
        return;
    }
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 24; bi.bmiHeader.biCompression = BI_RGB;
    bi.bmiHeader.biWidth = sw; bi.bmiHeader.biHeight = -sh;                          /* negative: top row first */
    small = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, (void **)&sbits, NULL, 0);
    tmp = (BYTE *)malloc(sstride * sh);
    g_dimBits = (BYTE *)malloc(sstride * sh);
    if (!small || !sbits || !tmp || !g_dimBits) {
        if (small) DeleteObject(small);
        if (tmp) free(tmp);
        FreeBackdrop();
        ReleaseDC(NULL, screen);
        return;
    }

    /* 1. A small copy of the screen. */
    mem = CreateCompatibleDC(screen);
    old = (HBITMAP)SelectObject(mem, small);
    SetStretchBltMode(mem, COLORONCOLOR);
    if (g_testMode) {
        HDC fake = CreateCompatibleDC(screen);
        HBITMAP fb = CreateCompatibleBitmap(screen, sw * DIM_SCALE, sh * DIM_SCALE), fo = (HBITMAP)SelectObject(fake, fb);
        FakeDesktop(fake, w, h);
        StretchBlt(mem, 0, 0, sw, sh, fake, 0, 0, sw * DIM_SCALE, sh * DIM_SCALE, SRCCOPY);
        SelectObject(fake, fo); DeleteObject(fb); DeleteDC(fake);
    } else {
        StretchBlt(mem, 0, 0, sw, sh, screen, 0, 0, sw * DIM_SCALE, sh * DIM_SCALE, SRCCOPY);
    }
    GdiFlush();
    SelectObject(mem, old);
    DeleteDC(mem);
    ReleaseDC(NULL, screen);

    /* 2. Blur it. Two rounds of a box blur look close to a smooth lens blur. */
    BlurPass(sbits, tmp, sw, sh, 3, sstride, 2);
    BlurPass(tmp, sbits, sh, sw, sstride, 3, 2);
    BlurPass(sbits, tmp, sw, sh, 3, sstride, 2);
    BlurPass(tmp, g_dimBits, sh, sw, sstride, 3, 2);

    /* 3. Darken it. The more opaque the backdrop, the less light is kept. */
    for (y = 0; y < sh; y++) {
        BYTE *p = g_dimBits + y * sstride;
        for (x = 0; x < sw * 3; x++) p[x] = (BYTE)(p[x] * light / 100);
    }
    free(tmp);
    DeleteObject(small);
    g_dimSW = sw; g_dimSH = sh; g_dimStride = sstride;
    g_dimMode = DIM_PICTURE;

    if (GetTickCount() - started > DIM_LIMIT) {
        wsprintf(msg, "The dimmed background took %lu ms, which is too slow. It is off until Searchlight restarts.", GetTickCount() - started);
        Log(msg);
        g_dimSlow = 1;
        FreeBackdrop();
    }
}

/* Paints part of the backdrop, enlarging the small picture a band at a time
 * and blending between its rows and columns. */
static void PaintBackdrop(HDC dc, const RECT *area)
{
    BITMAPINFO bi;
    BYTE *band, *rowA, *rowB, *swap;
    RECT r = *area;
    int w = g_dimW, n = w * 3, stride = (n + 3) & ~3, top, i, x, have = -1;
    if (r.left < 0) r.left = 0;
    if (r.top < 0) r.top = 0;
    if (r.right > g_dimW) r.right = g_dimW;
    if (r.bottom > g_dimH) r.bottom = g_dimH;
    if (r.right <= r.left || r.bottom <= r.top) return;
    if (g_dimMode == DIM_BLACK) { FillRect(dc, &r, (HBRUSH)GetStockObject(BLACK_BRUSH)); return; }
    if (g_dimMode != DIM_PICTURE || !g_dimBits) return;
    band = (BYTE *)malloc(stride * DIM_BAND);
    rowA = (BYTE *)malloc(n);
    rowB = (BYTE *)malloc(n);
    if (!band || !rowA || !rowB) {
        if (band) free(band);
        if (rowA) free(rowA);
        if (rowB) free(rowB);
        FillRect(dc, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));
        return;
    }
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 24; bi.bmiHeader.biCompression = BI_RGB;
    bi.bmiHeader.biWidth = w;
    for (top = r.top; top < r.bottom; top += DIM_BAND) {
        int rows = r.bottom - top > DIM_BAND ? DIM_BAND : r.bottom - top;
        for (i = 0; i < rows; i++) {
            int y = top + i, sy = y / DIM_SCALE, k = y % DIM_SCALE, j = DIM_SCALE - k;
            BYTE *d = band + (rows - 1 - i) * stride;                   /* the band is stored bottom row first */
            if (have != sy) {
                if (have == sy - 1) { swap = rowA; rowA = rowB; rowB = swap; }
                else ExpandRow(g_dimBits + sy * g_dimStride, rowA, w);
                ExpandRow(g_dimBits + (sy + 1) * g_dimStride, rowB, w);
                have = sy;
            }
            if (k == 0) memcpy(d, rowA, n);
            else for (x = 0; x < n; x++) d[x] = (BYTE)((rowA[x] * j + rowB[x] * k) / DIM_SCALE);
        }
        bi.bmiHeader.biHeight = rows;
        SetDIBitsToDevice(dc, r.left, top, r.right - r.left, rows, r.left, 0, 0, rows, band, &bi, DIB_RGB_COLORS);
    }
    free(band); free(rowA); free(rowB);
}

static LRESULT CALLBACK BackProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        PaintBackdrop(dc, &ps.rcPaint);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        HidePanel();                       /* a click outside the panel closes it */
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wp) != WA_INACTIVE && IsWindowVisible(g_panel)) SetForegroundWindow(g_panel);
        return 0;
    case WM_CLOSE:
        HidePanel();
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* Fills the Running list with the rows that match what was typed. */
static void FillTasks(const char *query, int select)
{
    int i, n = 0;
    SendMessage(g_taskList, WM_SETREDRAW, FALSE, 0);
    SendMessage(g_taskList, LB_RESETCONTENT, 0, 0);
    for (i = 0; i < g_taskCount; i++) {
        if (!TaskMatches(&g_tasks[i], query)) continue;
        SendMessage(g_taskList, LB_ADDSTRING, 0, (LPARAM)i);
        n++;
    }
    if (select >= n) select = n - 1;
    if (select >= 0) SendMessage(g_taskList, LB_SETCURSEL, select, 0);
    SendMessage(g_taskList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_taskList, NULL, TRUE);
}

static int SelectedTask(void)
{
    int row = (int)SendMessage(g_taskList, LB_GETCURSEL, 0, 0), t;
    if (row < 0) return -1;
    t = (int)SendMessage(g_taskList, LB_GETITEMDATA, row, 0);
    return (t >= 0 && t < g_taskCount) ? t : -1;
}

/* Reads the running programs again and keeps the same row selected. */
static void RefreshTasks(void)
{
    char text[120];
    int t = SelectedTask(), row = (int)SendMessage(g_taskList, LB_GETCURSEL, 0, 0), i, n;
    HWND wnd = NULL;
    DWORD pid = 0;
    if (t >= 0) { wnd = g_tasks[t].hwnd; pid = g_tasks[t].pid; }
    BuildTasks();
    GetWindowText(g_edit, text, sizeof(text));
    Trim(text);
    FillTasks(text, row < 0 ? 0 : row);
    n = (int)SendMessage(g_taskList, LB_GETCOUNT, 0, 0);
    for (i = 0; i < n && t >= 0; i++) {
        TASK *k = &g_tasks[SendMessage(g_taskList, LB_GETITEMDATA, i, 0)];
        if (k->hwnd == wnd && k->pid == pid) { SendMessage(g_taskList, LB_SETCURSEL, i, 0); break; }
    }
    InvalidateRect(g_panel, &g_rcContent, FALSE);
}

/* Fills the Startup list with the rows that match what was typed. */
static void FillStarts(const char *query)
{
    int i, n = 0, row = (int)SendMessage(g_startList, LB_GETCURSEL, 0, 0);
    SendMessage(g_startList, WM_SETREDRAW, FALSE, 0);
    SendMessage(g_startList, LB_RESETCONTENT, 0, 0);
    for (i = 0; i < g_startCount; i++) {
        if (!StartMatches(&g_starts[i], query)) continue;
        SendMessage(g_startList, LB_ADDSTRING, 0, (LPARAM)i);
        n++;
    }
    if (row < 0) row = 0;
    if (row >= n) row = n - 1;
    if (row >= 0) SendMessage(g_startList, LB_SETCURSEL, row, 0);
    SendMessage(g_startList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_startList, NULL, TRUE);
}

static int SelectedStart(void)
{
    int row = (int)SendMessage(g_startList, LB_GETCURSEL, 0, 0), s;
    if (row < 0) return -1;
    s = (int)SendMessage(g_startList, LB_GETITEMDATA, row, 0);
    return (s >= 0 && s < g_startCount) ? s : -1;
}

static void Flash(const char *text)
{
    CopyN(g_status, text, sizeof(g_status));
    if (!g_panel) return;
    InvalidateRect(g_panel, &g_rcStatus, FALSE);
    SetTimer(g_panel, 2, 4000, NULL);
}

static void Rescan(int quiet)
{
    char msg[80];
    HCURSOR old = SetCursor(LoadCursor(NULL, IDC_WAIT));
    Scan();
    FillLists();
    SaveState();
    Layout();
    StartEager();
    SetCursor(old);
    if (!quiet) { wsprintf(msg, "Rescanned: %d programs and settings, %d documents.", g_count - g_docCount, g_docCount); Flash(msg); }
}

static void UpdateQuery(void)
{
    char text[120];
    int i, was = g_queryMode;
    GetWindowText(g_edit, text, sizeof(text));
    Trim(text);
    if (g_view == VIEW_RUNNING) {
        g_queryMode = 0;
        FillTasks(text, (!text[0] && g_taskWindows > 1) ? 1 : 0);
        ShowContent();
        InvalidateRect(g_panel, NULL, FALSE);
        return;
    }
    if (g_view == VIEW_STARTUP) {
        g_queryMode = 0;
        FillStarts(text);
        ShowContent();
        InvalidateRect(g_panel, NULL, FALSE);
        return;
    }
    g_queryMode = text[0] != 0;
    Search(text);
    SendMessage(g_results, WM_SETREDRAW, FALSE, 0);
    SendMessage(g_results, LB_RESETCONTENT, 0, 0);
    g_calcOk = Calculate(text, g_calcText);
    g_modeOk = ParseMode(text);
    g_timerOk = ParseTimer(text);
    g_runOk = (g_modeOk || g_timerOk) ? 0 : RunTarget(text);
    if (g_calcOk) SendMessage(g_results, LB_ADDSTRING, 0, (LPARAM)ROW_CALC);
    if (g_modeOk) SendMessage(g_results, LB_ADDSTRING, 0, (LPARAM)ROW_MODE);
    if (g_timerOk) SendMessage(g_results, LB_ADDSTRING, 0, (LPARAM)ROW_TIMER);
    for (i = 0; i < g_resCount; i++) SendMessage(g_results, LB_ADDSTRING, 0, (LPARAM)g_res[i]);
    if (g_runOk) SendMessage(g_results, LB_ADDSTRING, 0, (LPARAM)ROW_RUN);
    if (SendMessage(g_results, LB_GETCOUNT, 0, 0) > 0) SendMessage(g_results, LB_SETCURSEL, 0, 0);
    SendMessage(g_results, WM_SETREDRAW, TRUE, 0);
    if (was != g_queryMode) ShowContent();
    InvalidateRect(g_results, NULL, TRUE);
    InvalidateRect(g_panel, NULL, FALSE);
}

/* Shows the programs, the running programs or the startup programs. */
static void SetView(int view)
{
    g_view = view;
    if (view == VIEW_STARTUP) BuildStarts();
    g_closing = NULL;
    KillTimer(g_panel, 3);
    KillTimer(g_panel, 4);
    if (view == VIEW_RUNNING) {
        BuildTasks();
        ReadStats();
        g_statTicks = 0;
        if (!g_testMode) SetTimer(g_panel, 4, 1000, NULL);     /* the gauges move once a second */
    }
    SetWindowText(g_edit, "");
    UpdateQuery();
    ShowContent();
    KillTimer(g_panel, 2);
    SetHint();
    InvalidateRect(g_panel, NULL, FALSE);
    InvalidateRect(g_edit, NULL, TRUE);
}

/* ------------------------------------------------------------------------
 * The panel: actions
 * --------------------------------------------------------------------- */
/* A question in a message box. The panel stays open behind it. */
static int Ask(const char *text, UINT flags)
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

static void PinAt(int index, int pos)
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

static void Unpin(int index)
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
static void ToggleFav(int index)
{
    if (index < 0 || index >= g_count) return;
    if (IsPinned(g_favs, g_favCount, &g_items[index]) >= 0) Unpin(index);
    else PinAt(index, g_favCount);
}

/* ------------------------------------------------------------------------
 * Dragging a program to Favorites
 *
 * Pressing the mouse button on a program arms the drag. It becomes a drag
 * once the mouse has moved a few dots; until then it is still a click.
 * --------------------------------------------------------------------- */
static void DragReset(void)
{
    int was = g_dragging;
    g_dragItem = -1; g_dragging = 0; g_dropPos = DROP_NONE; g_dropGroup = -1;
    if (GetCapture() && (GetCapture() == g_panel || GetParent(GetCapture()) == g_panel)) ReleaseCapture();
    if (!was) return;
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    if (g_dragCursor) DestroyIcon((HICON)g_dragCursor);
    g_dragCursor = NULL;
    InvalidateRect(g_panel, NULL, FALSE);
}

/* The group a program would move to if it were dropped here, or -1. Only
 * programs move, and only to a group they are not in already. */
static int DropGroup(POINT p)
{
    int i;
    if (g_view != VIEW_PROGRAMS || g_queryMode || g_items[g_dragItem].kind != 0) return -1;
    ScreenToClient(g_panel, &p);
    for (i = 0; i < g_listCount; i++) {
        if (!PtInRect(&g_rcPanel[i], p)) continue;
        if (g_listCat[i] == g_items[g_dragItem].cat || g_listCat[i] == CAT_SETTINGS) return -1;
        return i;
    }
    return -1;
}

static void DragArm(HWND from, int item)
{
    if (item < 0 || item >= g_count) return;
    g_dragItem = item; g_dragging = 0; g_dropPos = DROP_NONE;
    GetCursorPos(&g_dragStart);
    SetCapture(from);
}

/* Where in Favorites a drop at this place of the screen would land. */
static int DropPos(POINT p)
{
    int w = (g_rcFav.right - g_rcFav.left - 8) / MAX_PINS, pos;
    ScreenToClient(g_panel, &p);
    if (!PtInRect(&g_rcFav, p)) return DROP_NONE;
    if (g_favCount >= MAX_PINS && IsPinned(g_favs, g_favCount, &g_items[g_dragItem]) < 0) return DROP_FULL;
    pos = (p.x - g_rcFav.left - 4 + w / 2) / w;
    if (pos < 0) pos = 0;
    if (pos > g_favCount) pos = g_favCount;
    return pos;
}

static void DragMove(void)
{
    POINT p;
    int pos, group;
    GetCursorPos(&p);
    if (!g_dragging) {
        ICONINFO ii;
        ITEM *it = &g_items[g_dragItem];
        if (abs(p.x - g_dragStart.x) < 5 && abs(p.y - g_dragStart.y) < 5) return;
        g_dragging = 1;
        /* The program's own icon becomes the mouse pointer. */
        if (g_iconsPrograms && GetIconInfo(LargeIcon(it), &ii)) {
            ii.fIcon = FALSE; ii.xHotspot = 16; ii.yHotspot = 16;
            g_dragCursor = (HCURSOR)CreateIconIndirect(&ii);
            if (ii.hbmMask) DeleteObject(ii.hbmMask);
            if (ii.hbmColor) DeleteObject(ii.hbmColor);
        }
        KillTimer(g_panel, 2);
        if (it->kind == 0) wsprintf(g_status, "Drop \"%.80s\" on Favorites to pin it, or on another group to move it there. Esc: cancel", it->name);
        else wsprintf(g_status, "Drop \"%.100s\" on Favorites to pin it. Esc: cancel", it->name);
        InvalidateRect(g_panel, &g_rcStatus, FALSE);
        InvalidateRect(g_panel, &g_rcFav, FALSE);
    }
    pos = DropPos(p);
    if (pos != g_dropPos) { g_dropPos = pos; InvalidateRect(g_panel, &g_rcFav, FALSE); }
    group = pos == DROP_NONE ? DropGroup(p) : -1;
    if (group != g_dropGroup) { g_dropGroup = group; InvalidateRect(g_panel, NULL, FALSE); }
    if (pos == DROP_FULL) SetCursor(LoadCursor(NULL, IDC_NO));
    else SetCursor(g_dragCursor ? g_dragCursor : LoadCursor(NULL, IDC_ARROW));
}

/* The mouse button came up. Returns 1 if that ended a drag, 0 if it was a click. */
static int DragDrop(void)
{
    int item = g_dragItem, was = g_dragging, pos = g_dropPos, group = g_dropGroup;
    DragReset();
    if (!was) return 0;
    if (pos >= 0 || pos == DROP_FULL) PinAt(item, pos);
    else if (group >= 0 && group < g_listCount) MoveToGroup(item, g_listCat[group]);
    else { SetHint(); InvalidateRect(g_panel, &g_rcStatus, FALSE); }
    return 1;
}

/* ------------------------------------------------------------------------
 * The panel: running programs
 * --------------------------------------------------------------------- */
static void SwitchTo(int t)
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
static void KillTask(int t)
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
static void CloseTask(int t)
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
static void WatchClosing(void)
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

static void HidePanel(void)
{
    if (!IsWindowVisible(g_panel)) return;
    DragReset();
    ShowWindow(g_back, SW_HIDE);
    ShowWindow(g_panel, SW_HIDE);
    FreeBackdrop();
    KillTimer(g_panel, 1);
    KillTimer(g_panel, 3);
    KillTimer(g_panel, 4);
    g_closing = NULL;
    g_view = VIEW_PROGRAMS;
    SetWindowText(g_edit, "");
    ShowContent();
    SendMessage(g_taskList, LB_RESETCONTENT, 0, 0);
    SendMessage(g_startList, LB_RESETCONTENT, 0, 0);
    FreeTables();
}

static void ShowPanel(int view)
{
    int wasVisible = IsWindowVisible(g_panel);
    if (g_modal) return;
    if (GetTickCount() - g_lastScan > 30000) Rescan(1);
    g_view = VIEW_PROGRAMS;
    SetWindowText(g_edit, "");
    SetHint();
    Layout();
    if (view != VIEW_PROGRAMS) SetView(view);
    if (!wasVisible) {
        MakeBackdrop();                    /* copies the screen while the panel is still hidden */
        if (g_dimMode != DIM_NONE) {
            SetWindowPos(g_back, HWND_TOPMOST, 0, 0, g_dimW, g_dimH, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            UpdateWindow(g_back);
        }
    }
    ShowWindow(g_panel, SW_SHOW);
    SetForegroundWindow(g_panel);
    SetFocus(g_edit);
    if (GetFocus() != g_edit && g_keyLogs < 60) {
        g_keyLogs++;
        Log(GetForegroundWindow() == g_panel ? "The search box did not get the keyboard when the panel opened."
                                             : "Windows did not bring the panel to the front when it opened.");
    }
    SetTimer(g_panel, 1, 10000, NULL);
}

static void TogglePanel(void)
{
    if (IsWindowVisible(g_panel) && GetForegroundWindow() == g_panel) HidePanel(); else ShowPanel(VIEW_PROGRAMS);
}

/* The second hotkey opens the running programs. Pressed again while they are
 * showing it steps to the next one, so it can be used like Alt+Tab. */
static void RunningHotkey(void)
{
    int n, row;
    if (g_modal) return;
    if (!IsWindowVisible(g_panel) || GetForegroundWindow() != g_panel) { ShowPanel(VIEW_RUNNING); return; }
    if (g_view != VIEW_RUNNING) { SetView(VIEW_RUNNING); return; }
    n = (int)SendMessage(g_taskList, LB_GETCOUNT, 0, 0);
    row = (int)SendMessage(g_taskList, LB_GETCURSEL, 0, 0);
    if (n > 0) SendMessage(g_taskList, LB_SETCURSEL, (row + 1) % n, 0);
}

static void WinKey(BYTE vk)
{
    keybd_event(VK_LWIN, 0, 0, 0);
    keybd_event(vk, 0, 0, 0);
    keybd_event(vk, 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_LWIN, 0, KEYEVENTF_KEYUP, 0);
}

static void Launch(int index)
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
static void Activate(int row)
{
    if (row == ROW_CALC) CopyResult();
    else if (row == ROW_RUN) RunTyped();
    else if (row == ROW_MODE) ApplyMode();
    else if (row == ROW_TIMER) StartCountdown();
    else Launch(row);
}

static void ForgetRecent(int tile)
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
static void Details(void)
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
 * Shutting down, restarting, changing the screen
 * --------------------------------------------------------------------- */
#ifndef MB_TOPMOST
#define MB_TOPMOST 0x00040000
#endif
#ifndef EWX_POWEROFF
#define EWX_POWEROFF 8
#endif
#define POWER_OFF     1
#define POWER_RESTART 2
#define POWER_LOGOFF  3
#define POWER_STANDBY 4
#define POWER_DOS     5

static char g_boxTitle[64];
static UINT g_boxTimer = 0;
static int  g_boxTimedOut = 0;

static VOID CALLBACK BoxTimeout(HWND h, UINT m, UINT_PTR id, DWORD t)
{
    HWND box = FindWindow("#32770", g_boxTitle);
    (void)h; (void)m; (void)id; (void)t;
    KillTimer(NULL, g_boxTimer);
    g_boxTimer = 0;
    g_boxTimedOut = 1;
    if (box) PostMessage(box, WM_COMMAND, IDCANCEL, 0);
}

/* A message box with OK and Cancel that closes by itself after some seconds.
 * Returns IDOK, IDCANCEL, or 0 if nobody answered. */
static int TimedBox(const char *text, const char *title, int seconds)
{
    int r;
    CopyN(g_boxTitle, title, sizeof(g_boxTitle));
    g_boxTimedOut = 0;
    g_boxTimer = SetTimer(NULL, 0, seconds * 1000, BoxTimeout);
    g_modal++;
    r = MessageBox(NULL, text, title, MB_OKCANCEL | MB_ICONQUESTION | MB_SETFOREGROUND | MB_TOPMOST);
    g_modal--;
    if (g_boxTimer) KillTimer(NULL, g_boxTimer);
    g_boxTimer = 0;
    return g_boxTimedOut ? 0 : r;
}

static int PowerAction(const char *path)
{
    if (strcmp(path, "@restart") == 0) return POWER_RESTART;
    if (strcmp(path, "@logoff") == 0) return POWER_LOGOFF;
    if (strcmp(path, "@standby") == 0) return POWER_STANDBY;
    if (strcmp(path, "@dos") == 0) return POWER_DOS;
    return 0;
}

static void PowerNow(int action)
{
    char pif[MAX_PATH];
    int ok = 0;
    if (g_testMode) return;
    if (action == POWER_OFF) ok = ExitWindowsEx(EWX_SHUTDOWN | EWX_POWEROFF, 0) || ExitWindowsEx(EWX_SHUTDOWN, 0);
    else if (action == POWER_RESTART) ok = ExitWindowsEx(EWX_REBOOT, 0);
    else if (action == POWER_LOGOFF) ok = ExitWindowsEx(EWX_LOGOFF, 0);
    else if (action == POWER_STANDBY) ok = SetSystemPowerState(TRUE, FALSE);
    else if (action == POWER_DOS) ok = DosModeFile(pif) && (int)ShellExecuteA(NULL, NULL, pif, NULL, NULL, SW_SHOWNORMAL) > 32;
    if (!ok) MessageBox(NULL, "Windows did not allow that.", APP_NAME, MB_OK | MB_ICONEXCLAMATION);
}

/* Restart, Log Off and the like happen at once, so they ask first. */
static void AskPower(int action)
{
    static const char *what[] = { "",
        "Shut down Windows now?", "Restart Windows now?", "Log off now?",
        "Put the computer on stand by now?", "Restart the computer in MS-DOS mode now?" };
    char text[200];
    if (g_testMode || action < 1 || action > 5) return;
    wsprintf(text, "%s\n\nPrograms with work you have not saved will ask about it first.", what[action]);
    if (Ask(text, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
    HidePanel();
    PowerNow(action);
}

static void StopCountdown(void)
{
    g_offAt = 0;
    KillTimer(g_main, 5);
    UpdateTray(0);
}

/* "shutdown in 30": Windows is shut down that many minutes from now. */
static void StartCountdown(void)
{
    SYSTEMTIME t;
    char msg[200];
    int minutes = g_timerMinutes, total;
    if (!g_timerOk || g_testMode) return;
    g_offAction = g_timerAction == 1 ? POWER_OFF : POWER_RESTART;
    g_offAt = GetTickCount() + (DWORD)minutes * 60000;
    if (!g_offAt) g_offAt = 1;
    GetLocalTime(&t);
    total = t.wHour * 60 + t.wMinute + minutes;
    wsprintf(g_offTime, "%02d:%02d", (total / 60) % 24, total % 60);
    SetTimer(g_main, 5, 1000, NULL);
    UpdateTray(0);
    SetWindowText(g_edit, "");
    wsprintf(msg, "Windows will %s at %s. To stop that, right-click the magnifier next to the clock.",
             g_offAction == POWER_OFF ? "shut down" : "restart", g_offTime);
    Flash(msg);
}

/* Once a second while a countdown runs. */
static void CountdownTick(void)
{
    char text[300];
    int action = g_offAction;
    if (!g_offAt || (LONG)(GetTickCount() - g_offAt) < 0) return;
    StopCountdown();
    HidePanel();
    wsprintf(text, "It is %s. Windows will %s in 30 seconds.\n\nOK does it now. Cancel stops it.",
             g_offTime, action == POWER_OFF ? "shut down" : "restart");
    if (TimedBox(text, APP_NAME " - Countdown", 30) == IDCANCEL) return;
    PowerNow(action);
}

/* "800x600": changes the screen, and changes it back unless somebody says
 * within 15 seconds that the new picture can be read. */
static void ApplyMode(void)
{
    DEVMODE dm, old;
    HDC dc;
    LONG r;
    int w = g_modeW, h = g_modeH, bpp = g_modeBpp;
    if (!g_modeOk || g_testMode) return;
    ZeroMemory(&old, sizeof(old));
    old.dmSize = sizeof(old);
    old.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
    old.dmPelsWidth = GetSystemMetrics(SM_CXSCREEN);
    old.dmPelsHeight = GetSystemMetrics(SM_CYSCREEN);
    dc = GetDC(NULL);
    old.dmBitsPerPel = GetDeviceCaps(dc, BITSPIXEL) * GetDeviceCaps(dc, PLANES);
    ReleaseDC(NULL, dc);
    dm = old;
    dm.dmPelsWidth = w; dm.dmPelsHeight = h; dm.dmBitsPerPel = bpp;
    HidePanel();
    if ((int)old.dmPelsWidth == w && (int)old.dmPelsHeight == h && (int)old.dmBitsPerPel == bpp) {
        MessageBox(NULL, "The screen is already set to that.", APP_NAME, MB_OK | MB_ICONINFORMATION);
        return;
    }
    r = ChangeDisplaySettings(&dm, 0);
    if (r == DISP_CHANGE_RESTART) {
        MessageBox(NULL, "Windows has to restart before it can show that many colors.\nPlease use Display in Control Panel for this one.", APP_NAME, MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (r != DISP_CHANGE_SUCCESSFUL) {
        MessageBox(NULL, "The screen could not be changed to that.", APP_NAME, MB_OK | MB_ICONEXCLAMATION);
        return;
    }
    if (TimedBox("Can you read this?\n\nOK keeps the new screen. Cancel goes back to the old one.\nWithout an answer the old one comes back in 15 seconds.",
                 APP_NAME " - Screen", 15) == IDOK)
        ChangeDisplaySettings(&dm, CDS_UPDATEREGISTRY);
    else
        ChangeDisplaySettings(&old, 0);
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
static void MoveToGroup(int index, int cat)
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
static void ItemMenu(int index, int recent)
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

static void TaskMenu(int t)
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
static void ToggleStartRow(int i)
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

static void StartMenu(int i)
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
static void Help(void)
{
    char path[MAX_PATH];
    if (g_testMode) return;
    PathIn(path, "README.TXT");
    HidePanel();
    if ((int)ShellExecuteA(NULL, NULL, path, NULL, NULL, SW_SHOWNORMAL) <= 32)
        MessageBox(NULL, "README.TXT is missing from the Searchlight 98 folder.", APP_NAME, MB_OK | MB_ICONINFORMATION);
}

/* Starts a favorite by its hotkey, Ctrl+Alt+1 to 5. */
static void FavoriteHotkey(int place)
{
    if (g_modal || place < 0 || place >= g_favCount) return;
    Launch(FindById(g_favs[place]));
}

/* ------------------------------------------------------------------------
 * The panel: drawing
 * --------------------------------------------------------------------- */
static void Gradient(HDC dc, const RECT *rc)
{
    int w = rc->right - rc->left, i, steps = 48;
    for (i = 0; i < steps; i++) {
        RECT r;
        HBRUSH b = CreateSolidBrush(RGB(16 * i / (steps - 1), 132 * i / (steps - 1), 128 + 80 * i / (steps - 1)));
        r.left = rc->left + w * i / steps; r.right = rc->left + w * (i + 1) / steps;
        r.top = rc->top; r.bottom = rc->bottom;
        FillRect(dc, &r, b);
        DeleteObject(b);
    }
}

static void Title(HDC dc, const RECT *box, const char *text)
{
    RECT r = *box, t;
    DrawEdge(dc, &r, EDGE_RAISED, BF_RECT);
    SetRect(&t, box->left + 2, box->top + 2, box->right - 2, box->top + 2 + TITLE_H);
    Gradient(dc, &t);
    t.left += 4;
    SelectObject(dc, g_fontBold);
    SetTextColor(dc, RGB(255, 255, 255));
    SetBkMode(dc, TRANSPARENT);
    DrawText(dc, text, -1, &t, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
}

/* Stands in for an icon where icons are switched off: a coloured square
 * with the first letter of the name. */
static void Letter(HDC dc, int x, int y, int size, const char *name)
{
    static const COLORREF tones[8] = {
        RGB(0, 0, 128), RGB(0, 128, 128), RGB(128, 0, 0), RGB(0, 96, 0),
        RGB(128, 0, 128), RGB(128, 96, 0), RGB(64, 64, 64), RGB(0, 80, 160)
    };
    RECT r;
    char c[2] = "?";
    const char *p;
    HBRUSH b;
    HFONT old;
    COLORREF ink;
    int mode;
    for (p = name; *p; p++) if (IsCharAlphaNumeric(*p)) { c[0] = *p; break; }
    CharUpperBuff(c, 1);
    SetRect(&r, x, y, x + size, y + size);
    b = CreateSolidBrush(tones[(BYTE)c[0] % 8]);
    FillRect(dc, &r, b);
    DeleteObject(b);
    mode = SetBkMode(dc, TRANSPARENT);
    ink = SetTextColor(dc, RGB(255, 255, 255));
    old = (HFONT)SelectObject(dc, size > 16 ? g_fontEdit : g_fontBold);
    DrawText(dc, c, 1, &r, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
    SelectObject(dc, old);
    SetTextColor(dc, ink);
    SetBkMode(dc, mode);
}
static void TileRect(const RECT *box, int i, RECT *out)
{
    int w = (box->right - box->left - 8) / MAX_PINS;
    SetRect(out, box->left + 4 + i * w, box->top + TITLE_H + 5, box->left + 4 + (i + 1) * w - 2, box->bottom - 4);
}

/* The small X button in the corner of a Favorites or Recent tile. */
static void TileX(const RECT *tile, RECT *out)
{
    SetRect(out, tile->right - 17, tile->top, tile->right - 1, tile->top + 14);
}

/* While a program is dragged over Favorites, a bar shows where it would land. */
static void DropMark(HDC dc, const RECT *box)
{
    RECT r;
    HBRUSH b;
    int w = (box->right - box->left - 8) / MAX_PINS, x;
    if (!g_dragging || g_dropPos < 0) return;
    x = box->left + 4 + g_dropPos * w - 2;
    if (x < box->left + 3) x = box->left + 3;
    SetRect(&r, x, box->top + TITLE_H + 5, x + 3, box->bottom - 4);
    b = CreateSolidBrush(GetSysColor(COLOR_HIGHLIGHT));
    FillRect(dc, &r, b);
    DeleteObject(b);
}

static void Tiles(HDC dc, const RECT *box, const char *title, char list[][110], int count, int boxId, const char *empty)
{
    int i;
    Title(dc, box, title);
    SelectObject(dc, g_font);
    if (boxId == 1 && g_dragging) {
        RECT r;
        HBRUSH b = CreateSolidBrush(GetSysColor(g_dropPos == DROP_FULL ? COLOR_BTNSHADOW : COLOR_HIGHLIGHT));
        SetRect(&r, box->left + 2, box->top + TITLE_H + 2, box->right - 2, box->bottom - 2);
        FrameRect(dc, &r, b);
        DeleteObject(b);
    }
    if (!count) {
        RECT r;
        SetRect(&r, box->left + 10, box->top + TITLE_H + 10, box->right - 8, box->bottom - 6);
        SetTextColor(dc, RGB(128, 128, 128));
        DrawText(dc, (boxId == 1 && g_dragging) ? "Drop it here." : empty, -1, &r, DT_NOPREFIX | DT_WORDBREAK);
        if (boxId == 1) DropMark(dc, box);
        return;
    }
    for (i = 0; i < count; i++) {
        RECT r, t;
        int index = FindById(list[i]), hot = (g_hoverBox == boxId && g_hoverTile == i);
        ITEM *it;
        if (index < 0) continue;
        it = &g_items[index];
        TileRect(box, i, &r);
        if (g_iconsPrograms) DrawIconEx(dc, (r.left + r.right) / 2 - 16, r.top + 1, LargeIcon(it), 32, 32, 0, NULL, DI_NORMAL);
        else Letter(dc, (r.left + r.right) / 2 - 16, r.top + 1, 32, it->name);
        if (boxId == 1 && (g_testMode || (g_favKeysOn & (1 << i)))) {        /* its hotkey is Ctrl+Alt+this */
            char digit[2];
            digit[0] = (char)('1' + i); digit[1] = 0;
            SetRect(&t, r.left + 3, r.top, r.left + 14, r.top + 13);
            SetTextColor(dc, RGB(128, 128, 128));
            DrawText(dc, digit, 1, &t, DT_SINGLELINE | DT_NOPREFIX);
        }
        SetRect(&t, r.left, r.top + 35, r.right, r.bottom);
        if (hot) {
            HBRUSH b = CreateSolidBrush(GetSysColor(COLOR_HIGHLIGHT));
            FillRect(dc, &t, b);
            DeleteObject(b);
        }
        SetTextColor(dc, hot ? GetSysColor(COLOR_HIGHLIGHTTEXT) : RGB(0, 0, 0));
        DrawText(dc, it->name, -1, &t, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX | DT_END_ELLIPSIS | DT_EDITCONTROL);
        if (hot && !g_dragging) {
            TileX(&r, &t);
            DrawFrameControl(dc, &t, DFC_CAPTION, DFCS_CAPTIONCLOSE);
        }
    }
    if (boxId == 1) DropMark(dc, box);
}

/* Programs, Running and Startup, next to the search box. The pressed one is showing. */
static void Tabs(HDC dc)
{
    static const char *names[VIEW_COUNT] = { "Programs", "Running", "Startup" };
    int i;
    for (i = 0; i < VIEW_COUNT; i++) {
        RECT r = g_rcTab[i];
        int on = (g_view == i);
        DrawFrameControl(dc, &r, DFC_BUTTON, DFCS_BUTTONPUSH | (on ? DFCS_PUSHED : 0));
        if (on) OffsetRect(&r, 1, 1);
        SelectObject(dc, on ? g_fontBold : g_font);
        SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
        DrawText(dc, names[i], -1, &r, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
    }
}

/* One gauge: its name, a bar of blocks like a Windows 98 progress bar, its value. */
static void Gauge(HDC dc, const RECT *cell, const char *name, int percent, const char *value)
{
    RECT r, bar;
    HBRUSH b;
    int x, nameW = 62, valueW = (cell->right - cell->left) > 260 ? 104 : 74;
    SelectObject(dc, g_font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    SetRect(&r, cell->left + 4, cell->top, cell->left + 4 + nameW, cell->bottom);
    DrawText(dc, name, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    SetRect(&r, cell->right - valueW, cell->top, cell->right - 4, cell->bottom);
    if (percent < 0) SetTextColor(dc, RGB(128, 128, 128));
    DrawText(dc, value, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
    SetRect(&bar, cell->left + 4 + nameW, cell->top + 6, cell->right - valueW - 6, cell->bottom - 6);
    if (bar.right - bar.left < 20) return;
    DrawEdge(dc, &bar, BDR_SUNKENOUTER, BF_RECT);
    if (percent <= 0) return;
    if (percent > 100) percent = 100;
    InflateRect(&bar, -2, -2);
    b = CreateSolidBrush(percent >= 90 ? RGB(192, 0, 0) : GetSysColor(COLOR_HIGHLIGHT));
    for (x = bar.left; x < bar.left + (bar.right - bar.left) * percent / 100; x += 8) {
        SetRect(&r, x, bar.top, x + 6 > bar.right ? bar.right : x + 6, bar.bottom);
        FillRect(dc, &r, b);
    }
    DeleteObject(b);
}

static void Gauges(HDC dc)
{
    RECT r = g_rcGauge, cell;
    char value[64];
    int w = (r.right - r.left) / 3;
    FillRect(dc, &r, (HBRUSH)(COLOR_BTNFACE + 1));
    if (g_cpu < 0) lstrcpy(value, "not available");
    else wsprintf(value, "%d%%", g_cpu);
    SetRect(&cell, r.left, r.top, r.left + w, r.bottom);
    Gauge(dc, &cell, "Processor", g_cpu, value);
    wsprintf(value, "%lu of %lu MB", (g_memUsedK + 512) / 1024, (g_memTotalK + 512) / 1024);
    SetRect(&cell, r.left + w, r.top, r.left + 2 * w, r.bottom);
    Gauge(dc, &cell, "Memory", g_memTotalK ? (int)(g_memUsedK / (g_memTotalK / 100 + 1)) : -1, value);
    if (g_resFree < 0) lstrcpy(value, "not available"); else wsprintf(value, "%d%% free", g_resFree);
    SetRect(&cell, r.left + 2 * w, r.top, r.right, r.bottom);
    Gauge(dc, &cell, "Resources", g_resFree < 0 ? -1 : 100 - g_resFree, value);
}

/* Once a second only the gauges are drawn again, not the whole panel. */
static void RedrawGauges(void)
{
    HDC dc = GetDC(g_panel), mem = CreateCompatibleDC(dc);
    int w = g_rcGauge.right - g_rcGauge.left, h = g_rcGauge.bottom - g_rcGauge.top;
    HBITMAP bm = CreateCompatibleBitmap(dc, w, h), old = (HBITMAP)SelectObject(mem, bm);
    SetViewportOrgEx(mem, -g_rcGauge.left, -g_rcGauge.top, NULL);
    Gauges(mem);
    SetViewportOrgEx(mem, 0, 0, NULL);
    BitBlt(dc, g_rcGauge.left, g_rcGauge.top, w, h, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bm);
    DeleteDC(mem);
    ReleaseDC(g_panel, dc);
}

static void Paint(HDC dc)
{
    RECT rc, r;
    char text[120];
    SYSTEMTIME t;
    int i;
    static const char *days[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
    static const char *months[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };

    SetRect(&rc, 0, 0, g_panelW, g_panelH);
    FillRect(dc, &rc, (HBRUSH)(COLOR_BTNFACE + 1));
    DrawEdge(dc, &rc, EDGE_RAISED, BF_RECT);

    /* caption */
    Gradient(dc, &g_rcCaption);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    SelectObject(dc, g_fontBold);
    r = g_rcCaption; r.left += 6;
    DrawText(dc, APP_NAME, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    GetLocalTime(&t);
    if (g_offAt) wsprintf(text, "Windows %s at %s      %02d:%02d", g_offAction == 1 ? "shuts down" : "restarts", g_offTime, t.wHour, t.wMinute);
    else wsprintf(text, "%02d:%02d   %s, %d %s %d", t.wHour, t.wMinute, days[t.wDayOfWeek], t.wDay, months[t.wMonth - 1], t.wYear);
    r = g_rcCaption; r.right = g_rcClose.left - 8;
    SelectObject(dc, g_font);
    DrawText(dc, text, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX);
    DrawFrameControl(dc, &g_rcClose, DFC_CAPTION, DFCS_CAPTIONCLOSE);

    Tabs(dc);
    Tiles(dc, &g_rcFav, "Favorites", g_favs, g_favCount, 1, "Drag any program here to pin it.");
    Tiles(dc, &g_rcRec, "Recent", g_recent, g_recentCount, 2, "Programs you start from Searchlight appear here.");
    if (g_view == VIEW_RUNNING) {
        int shown = (int)SendMessage(g_taskList, LB_GETCOUNT, 0, 0);
        if (shown < g_taskCount) wsprintf(text, "Running (%d of %d)", shown, g_taskCount);
        else wsprintf(text, "Running (%d open windows, %d in the background)", g_taskWindows, g_taskCount - g_taskWindows);
        Title(dc, &g_rcContent, text);
        SetRect(&r, g_rcContent.left, g_rcContent.top + 2, g_rcContent.right - 38, g_rcContent.top + 2 + TITLE_H);
        DrawText(dc, "Memory", -1, &r, DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX);
        Gauges(dc);
    } else if (g_view == VIEW_STARTUP) {
        int off = 0;
        for (i = 0; i < g_startCount; i++) if (!g_starts[i].enabled) off++;
        wsprintf(text, "Startup (%d start with Windows, %d switched off)", g_startCount - off, off);
        Title(dc, &g_rcContent, text);
    } else if (g_queryMode) {
        int rows = (int)SendMessage(g_results, LB_GETCOUNT, 0, 0);
        if (rows) wsprintf(text, "Results (%d)", rows); else lstrcpy(text, "No matches");
        Title(dc, &g_rcContent, text);
    } else {
        for (i = 0; i < g_listCount; i++) {
            if (g_dragging && g_dropGroup == i) wsprintf(text, "Move to %s", g_catName[g_listCat[i]]);
            else wsprintf(text, "%s (%d)", g_catName[g_listCat[i]], g_catItems[g_listCat[i]]);
            Title(dc, &g_rcPanel[i], text);
            if (g_dragging && g_dropGroup == i) {
                HBRUSH b = CreateSolidBrush(GetSysColor(COLOR_HIGHLIGHT));
                r = g_rcPanel[i];
                FrameRect(dc, &r, b);
                InflateRect(&r, -1, -1);
                FrameRect(dc, &r, b);
                DeleteObject(b);
            }
        }
    }

    /* status bar */
    r = g_rcStatus;
    DrawEdge(dc, &r, BDR_SUNKENOUTER, BF_RECT);
    SelectObject(dc, g_font);
    SetTextColor(dc, RGB(0, 0, 0));
    r.left += 5; r.right -= 90;
    DrawText(dc, g_status, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
    if (g_view == VIEW_RUNNING) wsprintf(text, "%d running", g_taskCount);
    else if (g_view == VIEW_STARTUP) wsprintf(text, "%d entries", g_startCount);
    else wsprintf(text, "%d items", g_count);
    r = g_rcStatus; r.right -= 6;
    SetTextColor(dc, RGB(64, 64, 64));
    DrawText(dc, text, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX);
}

/* A small yellow star marks programs pinned to Favorites. */
static void Star(HDC dc, int cx, int cy)
{
    static const int px[10] = { 0, 2, 6, 3, 4, 0, -4, -3, -6, -2 };
    static const int py[10] = { -6, -2, -2, 1, 5, 3, 5, 1, -2, -2 };
    POINT p[10];
    HBRUSH fill = CreateSolidBrush(RGB(255, 204, 0)), oldBrush;
    HPEN line = CreatePen(PS_SOLID, 1, RGB(128, 96, 0)), oldPen;
    int i;
    for (i = 0; i < 10; i++) { p[i].x = cx + px[i]; p[i].y = cy + py[i]; }
    oldBrush = (HBRUSH)SelectObject(dc, fill);
    oldPen = (HPEN)SelectObject(dc, line);
    Polygon(dc, p, 10);
    SelectObject(dc, oldBrush); SelectObject(dc, oldPen);
    DeleteObject(fill); DeleteObject(line);
}

/* The X button at the end of a row of the Running list. */
static void TaskX(const RECT *row, RECT *out)
{
    int mid = (row->top + row->bottom) / 2;
    SetRect(out, row->right - 22, mid - 7, row->right - 6, mid + 7);
}

static void DrawTask(const DRAWITEMSTRUCT *d)
{
    TASK *k;
    RECT r = d->rcItem, c;
    int selected = (d->itemState & ODS_SELECTED) != 0;
    const char *note;
    HBRUSH b;
    if ((int)d->itemID < 0 || (int)d->itemData < 0 || (int)d->itemData >= g_taskCount) {
        FillRect(d->hDC, &r, (HBRUSH)(COLOR_WINDOW + 1));
        return;
    }
    k = &g_tasks[d->itemData];
    b = CreateSolidBrush(GetSysColor(selected ? COLOR_HIGHLIGHT : COLOR_WINDOW));
    FillRect(d->hDC, &r, b);
    DeleteObject(b);
    if (g_iconsRunning) DrawIconEx(d->hDC, r.left + 3, r.top + (r.bottom - r.top - 16) / 2, k->icon ? k->icon : g_defIcon, 16, 16, 0, NULL, DI_NORMAL);
    else Letter(d->hDC, r.left + 3, r.top + (r.bottom - r.top - 16) / 2, 16, k->title);
    SetBkMode(d->hDC, TRANSPARENT);
    SelectObject(d->hDC, g_font);
    if (selected) {
        TaskX(&r, &c);
        DrawFrameControl(d->hDC, &c, DFC_CAPTION, DFCS_CAPTIONCLOSE);
    }
    /* on the right: the memory it uses, then the file name or what is special about this row */
    c = r;
    c.right -= 30;
    if (k->memK) {
        char mem[32];
        if (k->memK < 10000) wsprintf(mem, "%lu K", k->memK);
        else wsprintf(mem, "%lu.%lu MB", k->memK / 1024, (k->memK % 1024) * 10 / 1024);
        SetTextColor(d->hDC, GetSysColor(selected ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
        DrawText(d->hDC, mem, -1, &c, DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX);
    }
    c.right -= 76;
    if (k->hung) {
        note = "Not responding";
        SetTextColor(d->hDC, selected ? RGB(255, 255, 128) : RGB(192, 0, 0));
    } else {
        note = k->hwnd ? k->exe : "in the background";
        SetTextColor(d->hDC, selected ? RGB(192, 192, 255) : RGB(128, 128, 128));
    }
    DrawText(d->hDC, note, -1, &c, DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX);
    r.left += 24;
    r.right -= 236;
    SetTextColor(d->hDC, GetSysColor(selected ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
    DrawText(d->hDC, k->title, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
}

/* A row of the Startup list: a tick box, the name, the command, where it starts from. */
static void DrawStart(const DRAWITEMSTRUCT *d)
{
    START *s;
    RECT r = d->rcItem, c;
    int selected = (d->itemState & ODS_SELECTED) != 0, mid = (d->rcItem.top + d->rcItem.bottom) / 2, nameW;
    HBRUSH b;
    if ((int)d->itemID < 0 || (int)d->itemData < 0 || (int)d->itemData >= g_startCount) {
        FillRect(d->hDC, &r, (HBRUSH)(COLOR_WINDOW + 1));
        return;
    }
    s = &g_starts[d->itemData];
    b = CreateSolidBrush(GetSysColor(selected ? COLOR_HIGHLIGHT : COLOR_WINDOW));
    FillRect(d->hDC, &r, b);
    DeleteObject(b);
    SetRect(&c, r.left + 5, mid - 6, r.left + 18, mid + 7);
    DrawFrameControl(d->hDC, &c, DFC_BUTTON, DFCS_BUTTONCHECK | (s->enabled ? DFCS_CHECKED : 0) | (s->source == SRC_WININI ? DFCS_INACTIVE : 0));
    if (g_iconsStartup) DrawIconEx(d->hDC, r.left + 25, mid - 8, s->icon ? s->icon : g_defIcon, 16, 16, 0, NULL, DI_NORMAL);
    else Letter(d->hDC, r.left + 25, mid - 8, 16, s->name);
    SetBkMode(d->hDC, TRANSPARENT);
    SelectObject(d->hDC, g_font);
    c = r;
    c.right -= 8;
    SetTextColor(d->hDC, selected ? RGB(192, 192, 255) : RGB(128, 128, 128));
    DrawText(d->hDC, SourceLabel(s->source), -1, &c, DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX);
    nameW = (r.right - r.left - 46 - 160) * 2 / 5;
    if (nameW < 120) nameW = 120;
    SetRect(&c, r.left + 46 + nameW + 10, r.top, r.right - 160, r.bottom);
    if (c.right > c.left + 30) DrawText(d->hDC, s->command, -1, &c, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
    SetRect(&c, r.left + 46, r.top, r.left + 46 + nameW, r.bottom);
    if (selected) SetTextColor(d->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
    else SetTextColor(d->hDC, GetSysColor(s->enabled ? COLOR_WINDOWTEXT : COLOR_GRAYTEXT));
    DrawText(d->hDC, s->name, -1, &c, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
}

/* The rows of the results that are not programs: the answer to a sum, and
 * what was typed as something to open or run. */
static HICON g_modeIcon = NULL, g_timerIcon = NULL;

static void DrawSpecial(const DRAWITEMSTRUCT *d)
{
    RECT r = d->rcItem, c;
    int selected = (d->itemState & ODS_SELECTED) != 0, calc = (d->itemData == ROW_CALC);
    char text[200];
    HICON icon;
    HBRUSH b = CreateSolidBrush(GetSysColor(selected ? COLOR_HIGHLIGHT : COLOR_WINDOW));
    FillRect(d->hDC, &r, b);
    DeleteObject(b);
    const char *note = calc ? "Enter copies the answer" : "Run";
    if (calc) {
        if (!g_calcIcon) IndexedIcons("calc.exe", 0, NULL, &g_calcIcon);
        icon = g_calcIcon;
        wsprintf(text, "= %s", g_calcText);
    } else if (d->itemData == ROW_MODE) {
        if (!g_modeIcon) IndexedIcons("desk.cpl", 0, NULL, &g_modeIcon);
        icon = g_modeIcon;
        CopyN(text, g_modeLabel, sizeof(text));
        note = "Screen";
    } else if (d->itemData == ROW_TIMER) {
        if (!g_timerIcon) IndexedIcons("shell32.dll", 27, NULL, &g_timerIcon);
        icon = g_timerIcon;
        CopyN(text, g_timerLabel, sizeof(text));
        note = "Countdown";
    } else {
        if (!g_runIcon) IndexedIcons("shell32.dll", 24, NULL, &g_runIcon);
        icon = SmallFileIcon(g_runFile);
        if (!icon) icon = g_runIcon;
        CopyN(text, g_runLabel, sizeof(text));
    }
    if (g_iconsPrograms) DrawIconEx(d->hDC, r.left + 3, r.top + (r.bottom - r.top - 16) / 2, icon ? icon : g_defIcon, 16, 16, 0, NULL, DI_NORMAL);
    else Letter(d->hDC, r.left + 3, r.top + (r.bottom - r.top - 16) / 2, 16, calc ? "Calculator" : note);
    SetBkMode(d->hDC, TRANSPARENT);
    SelectObject(d->hDC, g_font);
    c = r;
    c.right -= 8;
    SetTextColor(d->hDC, selected ? RGB(192, 192, 255) : RGB(128, 128, 128));
    DrawText(d->hDC, note, -1, &c, DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX);
    r.left += 24;
    r.right -= 150;
    if (calc) SelectObject(d->hDC, g_fontBold);
    SetTextColor(d->hDC, GetSysColor(selected ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
    DrawText(d->hDC, text, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
}

static void DrawRow(const DRAWITEMSTRUCT *d)
{
    ITEM *it;
    RECT r = d->rcItem;
    int selected = (d->itemState & ODS_SELECTED) != 0, results = (d->CtlID == ID_RESULTS);
    HBRUSH b;
    if (d->CtlID == ID_TASKS) { DrawTask(d); return; }
    if (d->CtlID == ID_STARTS) { DrawStart(d); return; }
    if ((int)d->itemID >= 0 && d->itemData >= ROW_CALC && d->itemData <= ROW_TIMER) { DrawSpecial(d); return; }
    if ((int)d->itemID < 0 || (int)d->itemData < 0 || (int)d->itemData >= g_count) {
        FillRect(d->hDC, &r, (HBRUSH)(COLOR_WINDOW + 1));
        return;
    }
    it = &g_items[d->itemData];
    b = CreateSolidBrush(GetSysColor(selected ? COLOR_HIGHLIGHT : COLOR_WINDOW));
    FillRect(d->hDC, &r, b);
    DeleteObject(b);
    if (g_iconsPrograms) DrawIconEx(d->hDC, r.left + 3, r.top + (r.bottom - r.top - 16) / 2, SmallIcon(it), 16, 16, 0, NULL, DI_NORMAL);
    else Letter(d->hDC, r.left + 3, r.top + (r.bottom - r.top - 16) / 2, 16, it->name);
    SetBkMode(d->hDC, TRANSPARENT);
    SelectObject(d->hDC, g_font);
    if (results) {
        RECT c = r;
        c.right -= 8;
        SetTextColor(d->hDC, selected ? RGB(192, 192, 255) : RGB(128, 128, 128));
        DrawText(d->hDC, g_catName[it->cat], -1, &c, DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX);
        r.right -= 130;
    }
    if (IsPinned(g_favs, g_favCount, it) >= 0) {
        Star(d->hDC, r.right - 9, (r.top + r.bottom) / 2);
        r.right -= 16;
    }
    r.left += 24;
    SetTextColor(d->hDC, GetSysColor(selected ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
    DrawText(d->hDC, it->name, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
}

/* ------------------------------------------------------------------------
 * The panel: input
 * --------------------------------------------------------------------- */
static int TileAt(int x, int y, int *box)
{
    int b, i;
    POINT p;
    p.x = x; p.y = y;
    for (b = 1; b <= 2; b++) {
        const RECT *rc = b == 1 ? &g_rcFav : &g_rcRec;
        int count = b == 1 ? g_favCount : g_recentCount;
        for (i = 0; i < count; i++) {
            RECT r;
            TileRect(rc, i, &r);
            if (PtInRect(&r, p)) { *box = b; return i; }
        }
    }
    return -1;
}

static void SetTileHover(int box, int tile)
{
    if (box == g_hoverBox && tile == g_hoverTile) return;
    g_hoverBox = box; g_hoverTile = tile;
    InvalidateRect(g_panel, &g_rcFav, FALSE);
    InvalidateRect(g_panel, &g_rcRec, FALSE);
}

static int RowAt(HWND list, LPARAM pos)
{
    DWORD r = (DWORD)SendMessage(list, LB_ITEMFROMPOINT, 0, pos);
    if (HIWORD(r)) return -1;
    return (int)SendMessage(list, LB_GETITEMDATA, LOWORD(r), 0);
}

static int g_pressRow = -1;         /* the row the mouse button went down on */

/* The results and the Running list keep their selection when the mouse leaves. */
static int KeepsSelection(HWND list) { return list == g_results || list == g_taskList || list == g_startList; }

/* A click in the Startup list: on the tick box it switches the entry off or on. */
static void StartClick(HWND h, LPARAM pos)
{
    DWORD r = (DWORD)SendMessage(h, LB_ITEMFROMPOINT, 0, pos);
    if (HIWORD(r)) return;
    SendMessage(h, LB_SETCURSEL, LOWORD(r), 0);
    if ((short)LOWORD(pos) < 22) ToggleStartRow((int)SendMessage(h, LB_GETITEMDATA, LOWORD(r), 0));
}

/* A click in the Running list: on the X it closes the window, elsewhere it switches to it. */
static void TaskClick(HWND h, LPARAM pos)
{
    DWORD r = (DWORD)SendMessage(h, LB_ITEMFROMPOINT, 0, pos);
    RECT row, x;
    POINT p;
    int t;
    if (HIWORD(r)) return;
    t = (int)SendMessage(h, LB_GETITEMDATA, LOWORD(r), 0);
    SendMessage(h, LB_GETITEMRECT, LOWORD(r), (LPARAM)&row);
    SendMessage(h, LB_SETCURSEL, LOWORD(r), 0);
    TaskX(&row, &x);
    p.x = (short)LOWORD(pos); p.y = (short)HIWORD(pos);
    if (p.x >= x.left - 3) CloseTask(t); else SwitchTo(t);
}

static LRESULT CALLBACK ListProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_MOUSEMOVE: {
        DWORD r;
        int row;
        if (g_dragItem >= 0 && GetCapture() == h) { DragMove(); if (g_dragging) return 0; }
        r = (DWORD)SendMessage(h, LB_ITEMFROMPOINT, 0, l);
        row = HIWORD(r) ? -1 : LOWORD(r);
        if (g_hoverList && g_hoverList != h && !KeepsSelection(g_hoverList)) SendMessage(g_hoverList, LB_SETCURSEL, (WPARAM)-1, 0);
        g_hoverList = h;
        if (row >= 0 && row != (int)SendMessage(h, LB_GETCURSEL, 0, 0)) SendMessage(h, LB_SETCURSEL, row, 0);
        if (row < 0 && !KeepsSelection(h)) SendMessage(h, LB_SETCURSEL, (WPARAM)-1, 0);
        SetTileHover(0, -1);
        return 0;
    }
    case WM_LBUTTONDOWN:
        g_pressRow = RowAt(h, l);
        if (h != g_taskList && h != g_startList) DragArm(h, g_pressRow);
        return 0;
    case WM_LBUTTONDBLCLK:
        if (h == g_startList) ToggleStartRow(RowAt(h, l));
        return 0;
    case WM_RBUTTONDOWN:
        return 0;
    case WM_RBUTTONUP: {
        /* the row under the mouse is picked, then its menu opens */
        DWORD r = (DWORD)SendMessage(h, LB_ITEMFROMPOINT, 0, l);
        int data;
        if (HIWORD(r) || g_dragItem >= 0) return 0;
        SendMessage(h, LB_SETCURSEL, LOWORD(r), 0);
        data = (int)SendMessage(h, LB_GETITEMDATA, LOWORD(r), 0);
        if (h == g_taskList) TaskMenu(data);
        else if (h == g_startList) StartMenu(data);
        else ItemMenu(data, -1);
        return 0;
    }
    case WM_LBUTTONUP: {
        int pressed = g_pressRow, i;
        g_pressRow = -1;
        if (h == g_taskList) { TaskClick(h, l); return 0; }
        if (h == g_startList) { StartClick(h, l); return 0; }
        if (DragDrop()) return 0;
        i = RowAt(h, l);
        if (i >= 0 && i == pressed) Activate(i);
        return 0;
    }
    case WM_CAPTURECHANGED:
        if (g_dragItem >= 0) DragReset();
        return 0;
    case WM_KEYDOWN:
    case WM_CHAR:
        SetFocus(g_edit);
        return SendMessage(g_edit, m, w, l);
    }
    return CallWindowProc(g_oldList, h, m, w, l);
}

static LRESULT CALLBACK EditProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_KEYDOWN:
        switch (w) {
        case VK_ESCAPE:
            if (g_dragItem >= 0) DragReset();
            else if (GetWindowTextLength(h)) SetWindowText(h, "");
            else HidePanel();
            return 0;
        case VK_TAB: {
            /* Tab goes to the next view, Shift+Tab to the one before. */
            char note[80];
            SetView((g_view + (GetKeyState(VK_SHIFT) < 0 ? VIEW_COUNT - 1 : 1)) % VIEW_COUNT);
            if (g_keyLogs < 60) { g_keyLogs++; wsprintf(note, "Tab: now showing view %d", g_view); Log(note); }
            return 0;
        }
        case VK_SPACE:
            if (g_view == VIEW_STARTUP && GetWindowTextLength(h) == 0) { ToggleStartRow(SelectedStart()); return 0; }
            break;
        case VK_DELETE:
            if (g_view == VIEW_RUNNING) {
                /* Del still edits the text, unless there is nothing after the caret to delete. */
                DWORD from = 0, to = 0;
                int shift = GetKeyState(VK_SHIFT) < 0;
                SendMessage(h, EM_GETSEL, (WPARAM)&from, (LPARAM)&to);
                if (shift || (from == to && (int)to >= GetWindowTextLength(h))) {
                    if (shift) KillTask(SelectedTask()); else CloseTask(SelectedTask());
                    return 0;
                }
            }
            break;
        case VK_RETURN:
            if (g_view == VIEW_RUNNING) { SwitchTo(SelectedTask()); return 0; }
            if (g_view == VIEW_STARTUP) { ToggleStartRow(SelectedStart()); return 0; }
            if (g_queryMode) {
                int row = (int)SendMessage(g_results, LB_GETCURSEL, 0, 0);
                if (row >= 0) Activate((int)SendMessage(g_results, LB_GETITEMDATA, row, 0));
            }
            return 0;
        case VK_F2:
            if (g_queryMode && g_resCount) {
                int row = (int)SendMessage(g_results, LB_GETCURSEL, 0, 0);
                if (row >= 0) ToggleFav((int)SendMessage(g_results, LB_GETITEMDATA, row, 0));
            }
            return 0;
        case VK_F1:
            if (g_view == VIEW_RUNNING) Details(); else Help();
            return 0;
        case VK_F5:
            if (g_view == VIEW_RUNNING) { RefreshTasks(); Flash("The list of running programs is up to date."); return 0; }
            if (g_view == VIEW_STARTUP) { BuildStarts(); UpdateQuery(); Flash("The list of startup programs is up to date."); return 0; }
            Rescan(0);
            UpdateQuery();
            return 0;
        case VK_DOWN:
        case VK_UP:
        case VK_PRIOR:
        case VK_NEXT:
            if (g_view != VIEW_PROGRAMS) {
                HWND list = g_view == VIEW_RUNNING ? g_taskList : g_startList;
                int n = (int)SendMessage(list, LB_GETCOUNT, 0, 0);
                int row = (int)SendMessage(list, LB_GETCURSEL, 0, 0), step = (w == VK_DOWN || w == VK_UP) ? 1 : 8;
                if (!n) return 0;
                row += (w == VK_DOWN || w == VK_NEXT) ? step : -step;
                if (row < 0) row = 0;
                if (row >= n) row = n - 1;
                SendMessage(list, LB_SETCURSEL, row, 0);
                return 0;
            }
            if (g_queryMode) {
                int n = (int)SendMessage(g_results, LB_GETCOUNT, 0, 0);
                int row = (int)SendMessage(g_results, LB_GETCURSEL, 0, 0), step = (w == VK_DOWN || w == VK_UP) ? 1 : 8;
                if (!n) return 0;
                row += (w == VK_DOWN || w == VK_NEXT) ? step : -step;
                if (row < 0) row = 0;
                if (row >= n) row = n - 1;
                SendMessage(g_results, LB_SETCURSEL, row, 0);
            }
            return 0;
        }
        break;
    case WM_CHAR:
        if (w == VK_RETURN || w == VK_ESCAPE || w == VK_TAB) return 0;      /* no beep */
        if (w == ' ' && g_view == VIEW_STARTUP && GetWindowTextLength(h) == 0) return 0;
        break;
    case WM_PAINT: {
        LRESULT r = CallWindowProc(g_oldEdit, h, m, w, l);
        if (GetWindowTextLength(h) == 0) {                                   /* grey hint when empty */
            HDC dc = GetDC(h);
            RECT rc;
            HFONT old = (HFONT)SelectObject(dc, g_fontEdit);
            SendMessage(h, EM_GETRECT, 0, (LPARAM)&rc);
            rc.left += 4;
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(128, 128, 128));
            DrawText(dc, g_view == VIEW_RUNNING ? "Type to find a running program..." :
                         (g_view == VIEW_STARTUP ? "Type to find a startup program..." : "Type to search programs, settings and documents..."),
                     -1, &rc, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
            SelectObject(dc, old);
            ReleaseDC(h, dc);
        }
        return r;
    }
    case WM_MOUSEWHEEL: {
        POINT p;
        HWND under;
        GetCursorPos(&p);
        under = WindowFromPoint(p);
        if (under && under != h && GetParent(under) == g_panel) return SendMessage(under, m, w, l);
        if (g_view == VIEW_RUNNING) return SendMessage(g_taskList, m, w, l);
        if (g_view == VIEW_STARTUP) return SendMessage(g_startList, m, w, l);
        if (g_queryMode) return SendMessage(g_results, m, w, l);
        return 0;
    }
    }
    return CallWindowProc(g_oldEdit, h, m, w, l);
}

static LRESULT CALLBACK PanelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps), mem = CreateCompatibleDC(dc);
        HBITMAP bm = CreateCompatibleBitmap(dc, g_panelW, g_panelH), old = (HBITMAP)SelectObject(mem, bm);
        Paint(mem);
        BitBlt(dc, 0, 0, g_panelW, g_panelH, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteObject(bm);
        DeleteDC(mem);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_PRINTCLIENT:
        Paint((HDC)wp);
        return 0;
    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT *m = (MEASUREITEMSTRUCT *)lp;
        m->itemHeight = (m->CtlID == ID_RESULTS || m->CtlID == ID_TASKS || m->CtlID == ID_STARTS) ? RESULT_H : ROW_H;
        return TRUE;
    }
    case WM_DRAWITEM:
        DrawRow((const DRAWITEMSTRUCT *)lp);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wp) == ID_EDIT && HIWORD(wp) == EN_CHANGE) UpdateQuery();
        return 0;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        SetBkColor((HDC)wp, GetSysColor(COLOR_WINDOW));
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    case WM_MOUSEMOVE: {
        int box = 0, tile;
        if (g_dragItem >= 0 && GetCapture() == hwnd) { DragMove(); if (g_dragging) return 0; }
        tile = TileAt((short)LOWORD(lp), (short)HIWORD(lp), &box);
        SetTileHover(tile >= 0 ? box : 0, tile);
        if (g_hoverList && !KeepsSelection(g_hoverList)) { SendMessage(g_hoverList, LB_SETCURSEL, (WPARAM)-1, 0); g_hoverList = NULL; }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        /* Pressing on a tile may be the start of a drag, except on its X button. */
        POINT p;
        RECT r, x;
        int box = 0, tile;
        p.x = (short)LOWORD(lp); p.y = (short)HIWORD(lp);
        tile = TileAt(p.x, p.y, &box);
        if (tile < 0) return 0;
        TileRect(box == 1 ? &g_rcFav : &g_rcRec, tile, &r);
        TileX(&r, &x);
        if (PtInRect(&x, p)) return 0;
        DragArm(hwnd, FindById(box == 1 ? g_favs[tile] : g_recent[tile]));
        return 0;
    }
    case WM_LBUTTONUP: {
        POINT p;
        RECT r, x;
        int box = 0, tile, armed = g_dragItem, item;
        if (DragDrop()) return 0;
        p.x = (short)LOWORD(lp); p.y = (short)HIWORD(lp);
        if (PtInRect(&g_rcClose, p)) { HidePanel(); return 0; }
        if (PtInRect(&g_rcCaption, p) && p.x < 140) { About(); return 0; }
        for (tile = 0; tile < VIEW_COUNT; tile++)
            if (PtInRect(&g_rcTab[tile], p)) { SetView(tile); SetFocus(g_edit); return 0; }
        if (g_view == VIEW_RUNNING && PtInRect(&g_rcGauge, p)) { Details(); return 0; }
        tile = TileAt(p.x, p.y, &box);
        if (tile < 0) { SetFocus(g_edit); return 0; }
        item = FindById(box == 1 ? g_favs[tile] : g_recent[tile]);
        TileRect(box == 1 ? &g_rcFav : &g_rcRec, tile, &r);
        TileX(&r, &x);
        if (PtInRect(&x, p)) { if (box == 1) Unpin(item); else ForgetRecent(tile); }
        else if (item == armed) Launch(item);
        return 0;
    }
    case WM_RBUTTONUP: {
        int box = 0, tile = TileAt((short)LOWORD(lp), (short)HIWORD(lp), &box);
        if (tile >= 0 && g_dragItem < 0) ItemMenu(FindById(box == 1 ? g_favs[tile] : g_recent[tile]), box == 2 ? tile : -1);
        return 0;
    }
    case WM_CAPTURECHANGED:
        if (g_dragItem >= 0) DragReset();
        return 0;
    case WM_SETFOCUS:
        SetFocus(g_edit);
        return 0;
    case WM_TIMER:
        if (wp == 1) InvalidateRect(hwnd, &g_rcCaption, FALSE);
        if (wp == 2) { KillTimer(hwnd, 2); if (!g_dragging) { SetHint(); InvalidateRect(hwnd, &g_rcStatus, FALSE); } }
        if (wp == 3) WatchClosing();
        if (wp == 4 && g_view == VIEW_RUNNING) {
            ReadStats();
            RedrawGauges();
        }
        return 0;
    case WM_KEYDOWN:
    case WM_CHAR:
        if (g_keyLogs < 60) { g_keyLogs++; Log("A key reached the panel instead of the search box. It was passed on."); }
        SetFocus(g_edit);
        return SendMessage(g_edit, msg, wp, lp);
    case WM_SYSKEYDOWN:
        /* With no window holding the keyboard, keys arrive here under this name. */
        if (!(lp & 0x20000000)) {
            if (g_keyLogs < 60) { g_keyLogs++; Log("A key arrived while no window held the keyboard. It was passed on."); }
            SetFocus(g_edit);
            return SendMessage(g_edit, WM_KEYDOWN, wp, lp);
        }
        break;
    case WM_ACTIVATE:
        if (LOWORD(wp) != WA_INACTIVE) { SetFocus(g_edit); return 0; }        /* the keyboard goes to the search box */
        if (LOWORD(wp) == WA_INACTIVE && !g_testMode) {
            HWND other = (HWND)lp;
            if (g_modal) return 0;
            if (other && (other == g_settings || GetWindow(other, GW_OWNER) == hwnd)) return 0;
            HidePanel();
        }
        return 0;
    case WM_CLOSE:
        HidePanel();
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

static HWND MakeList(int id, int rowHeight)
{
    HWND h = CreateWindowEx(WS_EX_CLIENTEDGE, "LISTBOX", "",
        WS_CHILD | WS_VSCROLL | LBS_OWNERDRAWFIXED | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY,
        0, 0, 10, 10, g_panel, (HMENU)id, g_inst, NULL);
    (void)rowHeight;
    SendMessage(h, WM_SETFONT, (WPARAM)g_font, 0);
    g_oldList = (WNDPROC)SetWindowLong(h, GWL_WNDPROC, (LONG)ListProc);
    return h;
}

static void CreatePanel(void)
{
    LOGFONT lf;
    int i;
    GetObject(GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf);
    g_font = CreateFontIndirect(&lf);
    lf.lfWeight = FW_BOLD;
    g_fontBold = CreateFontIndirect(&lf);
    lf.lfWeight = FW_NORMAL; lf.lfHeight = -16;
    lstrcpy(lf.lfFaceName, "Tahoma");
    g_fontEdit = CreateFontIndirect(&lf);

    g_back = CreateWindowEx(WS_EX_TOOLWINDOW | (g_testMode ? 0 : WS_EX_TOPMOST), CLASS_BACK, "",
        WS_POPUP, 0, 0, 100, 100, NULL, NULL, g_inst, NULL);
    g_panel = CreateWindowEx(WS_EX_TOOLWINDOW | (g_testMode ? 0 : WS_EX_TOPMOST), CLASS_PANEL, APP_NAME,
        WS_POPUP | WS_CLIPCHILDREN, 0, 0, 100, 100, NULL, NULL, g_inst, NULL);
    g_edit = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, 0, 10, 10, g_panel, (HMENU)ID_EDIT, g_inst, NULL);
    SendMessage(g_edit, WM_SETFONT, (WPARAM)g_fontEdit, 0);
    SendMessage(g_edit, EM_LIMITTEXT, 100, 0);
    g_oldEdit = (WNDPROC)SetWindowLong(g_edit, GWL_WNDPROC, (LONG)EditProc);
    g_results = MakeList(ID_RESULTS, RESULT_H);
    g_taskList = MakeList(ID_TASKS, RESULT_H);
    g_startList = MakeList(ID_STARTS, RESULT_H);
    for (i = 0; i < MAX_CATS; i++) g_list[i] = MakeList(ID_LIST0 + i, ROW_H);
}

/* ------------------------------------------------------------------------
 * Tray icon and its menu
 * --------------------------------------------------------------------- */
static void UpdateTray(int add)
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

static LRESULT CALLBACK SettingsProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
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
#define ABOUT_W 400
#define ABOUT_H 270

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

static LRESULT CALLBACK AboutProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
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
static HWND MakeBox(const char *cls, const char *title, int cw, int ch)
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

static void About(void)
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
#define OPT_W 360
#define OPT_H 332

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

static LRESULT CALLBACK OptionsProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
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

static void OpenOptions(void)
{
    if (g_options) { SetForegroundWindow(g_options); return; }
    HidePanel();
    g_options = MakeBox(CLASS_OPT, APP_NAME " Options", OPT_W, OPT_H);
    if (g_options && !g_testMode) SetForegroundWindow(g_options);
}

static void OpenSettings(void)
{
    if (g_settings) { SetForegroundWindow(g_settings); return; }
    HidePanel();
    g_settings = MakeBox(CLASS_SET, APP_NAME " Hotkeys", 334, 267);
    if (g_settings) SetForegroundWindow(g_settings);
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
 * Test modes
 * --------------------------------------------------------------------- */
static int SelfTest(void)
{
    char in[MAX_PATH], out[MAX_PATH], line[400];
    FILE *fi, *fo;
    int i;
    PathIn(in, "TESTS.TXT");
    PathIn(out, "SELFTEST.OUT");
    fi = fopen(in, "r");
    fo = fopen(out, "w");
    if (!fi || !fo) return 1;
    while (fgets(line, sizeof(line), fi)) {
        char *a, *b;
        Trim(line);
        if (line[0] == 'C' && line[1] == '|') {            /* C|folder|name */
            char lname[100], lfolder[200];
            int cat;
            a = line + 2;
            b = strchr(a, '|'); if (!b) continue; *b++ = 0;
            CopyN(lfolder, a, sizeof(lfolder)); Lower(lfolder);
            CopyN(lname, b, sizeof(lname)); Lower(lname);
            cat = Categorize(lname, lfolder);
            AddItem(0, b, "C:\\X.LNK", cat < 0 ? CAT_OTHER : cat, lfolder, "");
            fprintf(fo, "C|%s|%s\n", b, cat < 0 ? "hide" : g_catName[cat]);
        } else if (line[0] == 'K' && line[1] == '|') {     /* K|name : is it skipped? */
            char lname[100];
            CopyN(lname, line + 2, sizeof(lname)); Lower(lname);
            fprintf(fo, "K|%s|%s\n", line + 2, MatchAny(lname, g_skip) ? "skip" : "keep");
        } else if (line[0] == 'S') {                        /* S : add the settings, sort */
            AddSettings();
            qsort(g_items, g_count, sizeof(ITEM), CompareItems);
        } else if (line[0] == 'Q' && line[1] == '|') {     /* Q|query */
            Search(line + 2);
            fprintf(fo, "Q|%s|", line + 2);
            for (i = 0; i < g_resCount && i < 3; i++) fprintf(fo, "%s%s", i ? " | " : "", g_items[g_res[i]].name);
            fprintf(fo, "\n");
        } else if (line[0] == 'D' && line[1] == '|') {     /* D|folder|name : a document */
            char lfolder[200];
            a = line + 2;
            b = strchr(a, '|'); if (!b) continue; *b++ = 0;
            CopyN(lfolder, a, sizeof(lfolder)); Lower(lfolder);
            AddItem(2, b, "C:\\X.DOC", CAT_DOCS, lfolder, "");
            qsort(g_items, g_count, sizeof(ITEM), CompareItems);
            fprintf(fo, "D|%s|%s\n", b, g_catName[CAT_DOCS]);
        } else if (line[0] == 'X' && line[1] == '|') {     /* X|sum : the calculator */
            char answer[48];
            fprintf(fo, "X|%s|%s\n", line + 2, Calculate(line + 2, answer) ? answer : "not a sum");
        } else if (line[0] == 'L' && line[1] == '|') {     /* L|times|name : the program was started that often */
            char lname[100];
            a = line + 2;
            b = strchr(a, '|'); if (!b) continue; *b++ = 0;
            CopyN(lname, b, sizeof(lname)); Lower(lname);
            for (i = 0; i < g_count; i++) if (strcmp(g_items[i].lname, lname) == 0) g_items[i].uses = atoi(a);
            fprintf(fo, "L|%s|%s\n", a, b);
        } else if (line[0] == 'H' && line[1] == '|') {     /* H|times|name : count that many starts */
            int n = 0;
            a = line + 2;
            b = strchr(a, '|'); if (!b) continue; *b++ = 0;
            for (i = 0; i < atoi(a); i++) n = CountUse(b);
            fprintf(fo, "H|%s|%s|%d|", a, b, n);
            for (i = 0; i < g_useCount; i++) fprintf(fo, "%s%s=%d", i ? ", " : "", g_uses[i].id, g_uses[i].n);
            fprintf(fo, "\n");
        } else if (line[0] == 'M' && line[1] == '|') {     /* M|folder|name|title|command : is it the uninstaller? */
            char part[4][200];
            int n = 0, m;
            for (a = strtok(line + 2, "|"); a && n < 4; a = strtok(NULL, "|")) { CopyN(part[n], a, 200); Lower(part[n]); n++; }
            if (n < 4) continue;
            if (strcmp(part[0], "-") == 0) part[0][0] = 0;
            m = UninstallMatches(part[0], part[1], part[2], part[3]);
            fprintf(fo, "M|%s|%s|%s\n", part[1], part[2], m == 2 ? "folder" : (m == 1 ? "name" : "no"));
        } else if (line[0] == 'O' && line[1] == '|') {     /* O|name|group : write it to a file like CATEGORY.TXT */
            char file[MAX_PATH], row[300];
            FILE *f;
            int n = 0;
            PathIn(file, "OVERRIDE.TMP");
            a = line + 2;
            b = strchr(a, '|'); if (!b) continue; *b++ = 0;
            if (strcmp(a, "!new") == 0) { DeleteFile(file); f = fopen(file, "w"); if (f) { fputs("# a comment stays\nOld Program=Games\n", f); fclose(f); } continue; }
            SetOverride(file, a, b);
            fprintf(fo, "O|%s|%s|", a, b);
            f = fopen(file, "r");
            while (f && fgets(row, sizeof(row), f)) { Trim(row); fprintf(fo, "%s%s", n++ ? " ; " : "", row); }
            if (f) fclose(f);
            fprintf(fo, "\n");
        } else if (line[0] == 'V' && line[1] == '|') {     /* V|text : is it a screen mode? */
            fprintf(fo, "V|%s|%s\n", line + 2, ParseMode(line + 2) ? g_modeLabel : "no");
        } else if (line[0] == 'Z' && line[1] == '|') {     /* Z|text : is it a countdown? */
            fprintf(fo, "Z|%s|%s\n", line + 2, ParseTimer(line + 2) ? g_timerLabel : "no");
        } else if (line[0] == 'B' && line[1] == '|') {     /* B|words : filter the startup programs */
            int n = 0;
            BuildStarts();
            fprintf(fo, "B|%s|", line + 2);
            for (i = 0; i < g_startCount; i++) {
                if (!StartMatches(&g_starts[i], line + 2)) continue;
                fprintf(fo, "%s%s%s", n++ ? " | " : "", g_starts[i].name, g_starts[i].enabled ? "" : " (off)");
            }
            fprintf(fo, "\n");
        } else if (line[0] == 'W' && line[1] == '|') {     /* W|text : can it be opened or run as typed? */
            fprintf(fo, "W|%s|%s\n", line + 2, RunTarget(line + 2) ? g_runLabel : "no");
        } else if (line[0] == 'P' && line[1] == '|') {     /* P|place|name : pin or move in Favorites */
            char lname[100], id[110];
            int r;
            a = line + 2;
            b = strchr(a, '|'); if (!b) continue; *b++ = 0;
            CopyN(lname, b, sizeof(lname)); Lower(lname);
            wsprintf(id, "app:%s", lname);
            r = PinMove(id, atoi(a));
            fprintf(fo, "P|%s|%s|%s|", a, b, r == 0 ? "added" : (r == 1 ? "moved" : "full"));
            for (i = 0; i < g_favCount; i++) fprintf(fo, "%s%s", i ? ", " : "", g_favs[i] + 4);
            fprintf(fo, "\n");
        } else if (line[0] == 'U' && line[1] == '|') {     /* U|place : unpin */
            PinRemove(atoi(line + 2));
            fprintf(fo, "U|%s|", line + 2);
            for (i = 0; i < g_favCount; i++) fprintf(fo, "%s%s", i ? ", " : "", g_favs[i] + 4);
            fprintf(fo, "\n");
        } else if (line[0] == 'R' && line[1] == '|') {     /* R|words : filter the running programs */
            int n = 0;
            BuildTasks();
            fprintf(fo, "R|%s|", line + 2);
            for (i = 0; i < g_taskCount; i++) {
                if (!TaskMatches(&g_tasks[i], line + 2)) continue;
                fprintf(fo, "%s%s", n++ ? " | " : "", g_tasks[i].title);
            }
            fprintf(fo, "\n");
        }
    }
    fclose(fi);
    /* A real scan of this computer's Start Menu. */
    free(g_items); g_items = NULL; g_count = g_cap = 0;
    Scan();
    fprintf(fo, "N|%d", g_count);
    for (i = 0; i < g_catCount; i++) if (g_catItems[i]) fprintf(fo, "|%s=%d", g_catName[i], g_catItems[i]);
    fprintf(fo, "\n");
    /* A real look at what is running on this computer. Nothing is changed. */
    g_testMode = 0;
    fprintf(fo, "Y|one entry of the program list takes %d bytes; %d entries, %d bytes in all without their text\n",
            (int)sizeof(ITEM), g_count, (int)(g_cap * sizeof(ITEM)));
    {
        POOL *p;
        long text = 0, blocks = 0;
        for (p = g_pool; p; p = p->next) { text += p->used; blocks += p->size; }
        fprintf(fo, "Y|their text takes %ld bytes, kept in %ld bytes of pool\n", text, blocks);
    }
    {
        MEMUSE use;
        MeasureProcess(GetCurrentProcessId(), &use, 0);
        fprintf(fo, "Y|this process: %lu K program and data, %lu K in %d libraries\n", use.ownK, use.libK, use.libs);
    }
    StartStats();
    BuildTasks();
    ReadStats();
    StopStats();
    BuildStarts();
    fprintf(fo, "A|%d entries that start with Windows were found\n", g_startCount);
    for (i = 0; i < g_startCount; i++)
        fprintf(fo, "A|%s|%s|icon=%s\n", SourceLabel(g_starts[i].source), g_starts[i].enabled ? "on" : "off", g_starts[i].icon ? "yes" : "no");
    g_testMode = 1;
    fprintf(fo, "G|processor=%d|memory=%lu of %lu K|cache=%lu K|resources free=%d|measured in %lu ms\n",
            g_cpu, g_memUsedK, g_memTotalK, g_memCacheK, g_resFree, g_measureMs);
    fprintf(fo, "T|%d|windows=%d|background=%d|programs seen=%d\n", g_taskCount, g_taskWindows, g_taskCount - g_taskWindows, g_procCount);
    for (i = 0; i < g_taskCount; i++)
        fprintf(fo, "T|%s|%s|icon=%s|memory=%lu K%s%s\n", g_tasks[i].hwnd ? "window" : "background", g_tasks[i].exe,
                g_tasks[i].icon ? "yes" : "no", g_tasks[i].memK, g_tasks[i].hung ? "|not responding" : "", g_tasks[i].locked ? "|locked" : "");
    fclose(fo);
    return 0;
}

/* Draws the screen as it would look with the panel open: backdrop plus panel.
 * A file name ending in PLAIN.BMP draws it without the dimmed backdrop. */
static int Shot(const char *file, const char *query)
{
    HDC screen, mem, out;
    HBITMAP bm, old, obm, oold;
    BITMAPINFO bi;
    BITMAPFILEHEADER fh;
    BYTE *px;
    FILE *f;
    int sw = ScreenW(), sh = ScreenH(), stride = (sw * 3 + 3) & ~3;
    /* "/noicons" and "/opacity N" in front of TEXT set those options first. */
    if (query && strncmp(query, "/noicons", 8) == 0) {
        g_iconsPrograms = g_iconsRunning = g_iconsStartup = 0;
        query += 8;
        while (*query == ' ') query++;
    }
    if (query && strncmp(query, "/opacity", 8) == 0) {
        query += 8;
        g_dimOpacity = atoi(query);
        while (*query == ' ') query++;
        while (*query >= '0' && *query <= '9') query++;
        while (*query == ' ') query++;
    }
    CreatePanel();
    Scan();
    FillLists();
    LoadState();
    PrunePins(g_favs, &g_favCount);
    PrunePins(g_recent, &g_recentCount);
    SetHint();
    Layout();
    /* TEXT can be words to search for, or one of these to show a feature:
     * /running [words], /hover and /recent (mouse on the first favorite or recent program),
     * /drag (a program over Favorites), /hotkeys (the Hotkeys box) */
    if (query && strncmp(query, "/tab", 4) == 0) {                 /* the Tab key: /tab once, /tab2 twice */
        SendMessage(g_panel, WM_SYSKEYDOWN, VK_TAB, 0);            /* as it arrives when no window holds the keyboard */
        if (query[4] == '2') SendMessage(g_edit, WM_KEYDOWN, VK_TAB, 0);
        Layout();
    } else if (query && strncmp(query, "/running", 8) == 0) {
        SetView(VIEW_RUNNING);
        query += 8;
        while (*query == ' ') query++;
        if (query[0]) { SetWindowText(g_edit, query); UpdateQuery(); }
        Layout();
    } else if (query && strncmp(query, "/startup", 8) == 0) {
        SetView(VIEW_STARTUP);
        Layout();
    } else if (query && strncmp(query, "/group", 6) == 0) {        /* a program dragged over another group */
        for (g_dragItem = 0; g_dragItem < g_count - 1 && g_items[g_dragItem].kind != 0; g_dragItem++) ;
        g_dragging = 1;
        g_dropGroup = g_listCount > 2 ? 2 : 0;
        lstrcpy(g_status, "Drop it on Favorites to pin it, or on another group to move it there. Esc: cancel");
    } else if (query && (strncmp(query, "/hotkeys", 8) == 0 || strncmp(query, "/about", 6) == 0 || strncmp(query, "/options", 8) == 0)) {
        /* drawn further down */
    } else if (query && strncmp(query, "/recent", 7) == 0) {
        g_hoverBox = 2; g_hoverTile = 0;
    } else if (query && strncmp(query, "/hover", 6) == 0) {
        g_hoverBox = 1; g_hoverTile = 0;
    } else if (query && strncmp(query, "/drag", 5) == 0) {
        g_dragItem = g_count / 2; g_dragging = 1; g_dropPos = g_favCount > 1 ? 1 : g_favCount;
        lstrcpy(g_status, "Drop it on Favorites to pin it. Esc: cancel");
    } else if (query && query[0]) { SetWindowText(g_edit, query); UpdateQuery(); Layout(); }
    if (lstrlen(file) >= 9 && lstrcmpi(file + lstrlen(file) - 9, "PLAIN.BMP") == 0) g_dimOn = 0;
    MakeBackdrop();
    /* Shown far off screen, without activating, so every control paints. */
    SetWindowPos(g_panel, NULL, -4000, -4000, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    ShowWindow(g_panel, SW_SHOWNOACTIVATE);
    UpdateWindow(g_panel);
    screen = GetDC(NULL);
    mem = CreateCompatibleDC(screen);
    bm = CreateCompatibleBitmap(screen, g_panelW, g_panelH);
    old = (HBITMAP)SelectObject(mem, bm);
    SendMessage(g_panel, WM_PRINT, (WPARAM)mem, PRF_CLIENT | PRF_CHILDREN | PRF_NONCLIENT | PRF_ERASEBKGND);
    out = CreateCompatibleDC(screen);
    obm = CreateCompatibleBitmap(screen, sw, sh);
    oold = (HBITMAP)SelectObject(out, obm);
    if (g_dimMode != DIM_NONE) {
        RECT all;
        SetRect(&all, 0, 0, sw, sh);
        PaintBackdrop(out, &all);
    } else FakeDesktop(out, sw, sh);
    BitBlt(out, g_panelX, g_panelY, g_panelW, g_panelH, mem, 0, 0, SRCCOPY);
    if (query && (strncmp(query, "/hotkeys", 8) == 0 || strncmp(query, "/about", 6) == 0 || strncmp(query, "/options", 8) == 0)) {
        RECT rc;
        HWND box;
        HDC bdc = CreateCompatibleDC(screen);
        HBITMAP bbm, bold;
        int w, h;
        ReadStats();
        if (query[1] == 'a') box = MakeBox(CLASS_ABOUT, "About " APP_NAME, ABOUT_W, ABOUT_H);
        else if (query[1] == 'o') box = MakeBox(CLASS_OPT, APP_NAME " Options", OPT_W, OPT_H);
        else box = MakeBox(CLASS_SET, APP_NAME " Hotkeys", 334, 267);
        GetWindowRect(box, &rc);
        w = rc.right - rc.left; h = rc.bottom - rc.top;
        ShowWindow(box, SW_SHOWNOACTIVATE);
        UpdateWindow(box);
        bbm = CreateCompatibleBitmap(screen, w, h);
        bold = (HBITMAP)SelectObject(bdc, bbm);
        SendMessage(box, WM_PRINT, (WPARAM)bdc, PRF_CLIENT | PRF_CHILDREN | PRF_NONCLIENT | PRF_ERASEBKGND);
        BitBlt(out, (sw - w) / 2, (sh - h) / 2, w, h, bdc, 0, 0, SRCCOPY);
        SelectObject(bdc, bold);
        DeleteObject(bbm);
        DeleteDC(bdc);
        DestroyWindow(box);
    }
    SelectObject(out, oold);
    SelectObject(mem, old);
    px = (BYTE *)malloc(stride * sh);
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = sw; bi.bmiHeader.biHeight = sh;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 24; bi.bmiHeader.biCompression = BI_RGB;
    GetDIBits(screen, obm, 0, sh, px, &bi, DIB_RGB_COLORS);
    ZeroMemory(&fh, sizeof(fh));
    fh.bfType = 0x4D42; fh.bfOffBits = sizeof(fh) + sizeof(BITMAPINFOHEADER);
    fh.bfSize = fh.bfOffBits + stride * sh;
    f = fopen(file, "wb");
    if (f) { fwrite(&fh, sizeof(fh), 1, f); fwrite(&bi.bmiHeader, sizeof(BITMAPINFOHEADER), 1, f); fwrite(px, 1, stride * sh, f); fclose(f); }
    free(px);
    DeleteObject(bm); DeleteObject(obm); DeleteDC(mem); DeleteDC(out); ReleaseDC(NULL, screen);
    DestroyWindow(g_panel);
    return f ? 0 : 1;
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
