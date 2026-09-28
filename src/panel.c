/* panel.c - Searchlight 98: the panel: layout, what it shows, opening and closing. */
#include "slight98.h"

/* ------------------------------------------------------------------------
 * The panel: layout
 * --------------------------------------------------------------------- */

void FillLists(void)
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

void Layout(void)
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
 * The panel: what it shows
 * --------------------------------------------------------------------- */

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

int SelectedTask(void)
{
    int row = (int)SendMessage(g_taskList, LB_GETCURSEL, 0, 0), t;
    if (row < 0) return -1;
    t = (int)SendMessage(g_taskList, LB_GETITEMDATA, row, 0);
    return (t >= 0 && t < g_taskCount) ? t : -1;
}

/* Reads the running programs again and keeps the same row selected. */
void RefreshTasks(void)
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

int SelectedStart(void)
{
    int row = (int)SendMessage(g_startList, LB_GETCURSEL, 0, 0), s;
    if (row < 0) return -1;
    s = (int)SendMessage(g_startList, LB_GETITEMDATA, row, 0);
    return (s >= 0 && s < g_startCount) ? s : -1;
}

void Flash(const char *text)
{
    CopyN(g_status, text, sizeof(g_status));
    if (!g_panel) return;
    InvalidateRect(g_panel, &g_rcStatus, FALSE);
    SetTimer(g_panel, 2, 4000, NULL);
}

void Rescan(int quiet)
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

void UpdateQuery(void)
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
void SetView(int view)
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
 * The panel: opening and closing
 * --------------------------------------------------------------------- */

void HidePanel(void)
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

void ShowPanel(int view)
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

void TogglePanel(void)
{
    if (IsWindowVisible(g_panel) && GetForegroundWindow() == g_panel) HidePanel(); else ShowPanel(VIEW_PROGRAMS);
}

/* The second hotkey opens the running programs. Pressed again while they are
 * showing it steps to the next one, so it can be used like Alt+Tab. */
void RunningHotkey(void)
{
    int n, row;
    if (g_modal) return;
    if (!IsWindowVisible(g_panel) || GetForegroundWindow() != g_panel) { ShowPanel(VIEW_RUNNING); return; }
    if (g_view != VIEW_RUNNING) { SetView(VIEW_RUNNING); return; }
    n = (int)SendMessage(g_taskList, LB_GETCOUNT, 0, 0);
    row = (int)SendMessage(g_taskList, LB_GETCURSEL, 0, 0);
    if (n > 0) SendMessage(g_taskList, LB_SETCURSEL, (row + 1) % n, 0);
}
