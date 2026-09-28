/* draw.c - Searchlight 98: drawing the panel. */
#include "slight98.h"

static HICON  g_calcIcon = NULL, g_runIcon = NULL;

void Gradient(HDC dc, const RECT *rc)
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

void TileRect(const RECT *box, int i, RECT *out)
{
    int w = (box->right - box->left - 8) / MAX_PINS;
    SetRect(out, box->left + 4 + i * w, box->top + TITLE_H + 5, box->left + 4 + (i + 1) * w - 2, box->bottom - 4);
}

/* The small X button in the corner of a Favorites or Recent tile. */
void TileX(const RECT *tile, RECT *out)
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
void RedrawGauges(void)
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

void Paint(HDC dc)
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
void TaskX(const RECT *row, RECT *out)
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

void DrawRow(const DRAWITEMSTRUCT *d)
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
