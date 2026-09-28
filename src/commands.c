/* commands.c - Searchlight 98: what can be typed besides a name: sums, things to run, screen modes, countdowns. */
#include "slight98.h"

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
int Calculate(const char *text, char *out)
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
int RunTarget(const char *text)
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

int ParseMode(const char *text)
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

int ParseTimer(const char *text)
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

int PowerAction(const char *path)
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
void AskPower(int action)
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

void StopCountdown(void)
{
    g_offAt = 0;
    KillTimer(g_main, 5);
    UpdateTray(0);
}

/* "shutdown in 30": Windows is shut down that many minutes from now. */
void StartCountdown(void)
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
void CountdownTick(void)
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
void ApplyMode(void)
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
