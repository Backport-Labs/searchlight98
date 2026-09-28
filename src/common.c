/* common.c - Searchlight 98: shared state and small helpers. */
#include "slight98.h"

/* ------------------------------------------------------------------------
 * Shared state
 * --------------------------------------------------------------------- */

const char CLASS_PANEL[] = "Searchlight98Panel";
const char CLASS_BACK[]  = "Searchlight98Backdrop";
const char CLASS_SET[]   = "Searchlight98Settings";
const char CLASS_ABOUT[] = "Searchlight98About";
const char CLASS_OPT[]   = "Searchlight98Options";

ITEM *g_items = NULL;
int g_count = 0, g_cap = 0;
POOL *g_pool = NULL;
int g_iconsPrograms = 1, g_iconsRunning = 1, g_iconsStartup = 1, g_iconsLazy = 1;

char g_catName[MAX_CATS][40] = {
    "Games", "Internet", "Office", "Media & Graphics", "Development",
    "Utilities", "Accessories", "System", "Settings", "Other", "Documents"
};
int g_catCount = FIXED_CATS;
int g_catItems[MAX_CATS];

char g_favs[MAX_PINS][110], g_recent[MAX_PINS][110];
int g_favCount = 0, g_recentCount = 0;

char   g_dir[MAX_PATH];
HINSTANCE g_inst;
HWND   g_main = NULL, g_panel = NULL, g_settings = NULL, g_options = NULL;
HWND   g_edit = NULL, g_results = NULL, g_list[MAX_CATS], g_hoverList = NULL;
int    g_listCat[MAX_CATS], g_listCount = 0;
RECT   g_rcCaption, g_rcClose, g_rcFav, g_rcRec, g_rcStatus, g_rcContent, g_rcPanel[MAX_CATS];
int    g_panelW = 0, g_panelH = 0, g_panelX = 0, g_panelY = 0;
HWND   g_back = NULL;            /* full-screen backdrop behind the panel */
int    g_dimSW = 0, g_dimSH = 0, g_dimStride = 0, g_dimMode = DIM_NONE;
int    g_dimW = 0, g_dimH = 0, g_dimOn = 1, g_dimSlow = 0, g_dimOpacity = DIM_DEFAULT;
int    g_res[MAX_RESULTS], g_resCount = 0, g_queryMode = 0;
int    g_hoverBox = 0, g_hoverTile = -1;
char   g_status[200] = "";
HFONT  g_font, g_fontBold, g_fontEdit;
HICON  g_appIcon = NULL, g_appIconLarge = NULL, g_defIcon = NULL;
UINT   g_mods = MOD_CONTROL, g_vk = VK_SPACE;
int    g_hotkeyOk = 0, g_testMode = 0;
DWORD  g_lastScan = 0;
FILE  *g_log = NULL;
ShellLink *g_link = NULL;
PersistFile *g_linkFile = NULL;

TASK  *g_tasks = NULL;            /* these tables exist while the panel is open */
int    g_taskCount = 0, g_taskWindows = 0;
int    g_view = VIEW_PROGRAMS;
HWND   g_taskList = NULL, g_startList = NULL, g_about = NULL;
RECT   g_rcTab[VIEW_COUNT];
START *g_starts = NULL;
int    g_startCount = 0;
USE   *g_uses = NULL;
int    g_useCount = 0, g_useCap = 0;

/* Favorites on Ctrl+Alt+1 to 5, a screen mode or a countdown typed in the box. */
int    g_favKeys = 1, g_favKeysOn = 0;
int    g_modeOk = 0, g_modeW = 0, g_modeH = 0, g_modeBpp = 0;
int    g_timerOk = 0, g_timerAction = 0, g_timerMinutes = 0;
char   g_modeLabel[120], g_timerLabel[120];
DWORD  g_offAt = 0;              /* tick count at which Windows is shut down or restarted; 0 = no countdown */
int    g_offAction = 0;
char   g_offTime[16];
int    g_dropGroup = -1;         /* group a dragged program would move to */
int    g_keyLogs = 0;
HWND   g_closing = NULL;         /* window that was asked to close */
int    g_closeTicks = 0, g_modal = 0;
FILEICON *g_fileIcons = NULL;
int    g_fileIconCount = 0;

/* The gauges above the Running list. -1 = this version of Windows does not say. */
RECT   g_rcGauge;
int    g_cpu = -1, g_resFree = -1, g_statTicks = 0;
DWORD  g_memTotalK = 0, g_memUsedK = 0, g_memFreeK = 0, g_memCacheK = 0, g_memSwapK = 0, g_measureMs = 0;
int    g_resUser = -1, g_resGdi = -1, g_cacheKnown = 0, g_statBytes = 0;
DWORD  g_statRaw[STAT_COUNT];

/* The extra rows of the results list: a sum that was typed, something to run. */
int    g_calcOk = 0, g_runOk = 0, g_docCount = 0;
char   g_calcText[48], g_runFile[MAX_PATH], g_runArgs[120], g_runLabel[160];
UINT   g_mods2 = MOD_CONTROL | MOD_SHIFT, g_vk2 = VK_SPACE;
int    g_hotkey2Ok = 0;

/* Dragging a program to Favorites. */
int    g_dragItem = -1, g_dragging = 0, g_dropPos = DROP_NONE;
KEYNAME g_keys[80];
int g_keyCount = 0;

/* ------------------------------------------------------------------------
 * Small helpers
 * --------------------------------------------------------------------- */

void PathIn(char *out, const char *name) { lstrcpy(out, g_dir); lstrcat(out, "\\"); lstrcat(out, name); }

int FileExists(const char *p) { return GetFileAttributes(p) != 0xFFFFFFFF; }

void CopyN(char *dst, const char *src, int size) { lstrcpyn(dst, src, size); }

void Lower(char *s) { CharLowerBuff(s, lstrlen(s)); }

int IsWordChar(char c) { return IsCharAlphaNumeric(c) || c == '_'; }

/* Test modes use a fixed screen size of 1024 x 768, so their output is the same everywhere. */
int ScreenW(void) { return g_testMode ? 1024 : GetSystemMetrics(SM_CXSCREEN); }

int ScreenH(void) { return g_testMode ? 768 : GetSystemMetrics(SM_CYSCREEN); }

void Log(const char *text)
{
    SYSTEMTIME t; char stamp[32];
    if (!g_log) return;
    GetLocalTime(&t);
    wsprintf(stamp, "%02d:%02d:%02d  ", t.wHour, t.wMinute, t.wSecond);
    fputs(stamp, g_log); fputs(text, g_log); fputs("\n", g_log); fflush(g_log);
}

void Trim(char *s)
{
    char *p = s;
    int n;
    while (*p == ' ' || *p == '\t') p++;
    if (p != s) memmove(s, p, lstrlen(p) + 1);
    n = lstrlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) s[--n] = 0;
}
