/* input.c - Searchlight 98: mouse and keyboard on the panel. */
#include "slight98.h"

static WNDPROC g_oldEdit = NULL, g_oldList = NULL;
static POINT  g_dragStart;
static HCURSOR g_dragCursor = NULL;

/* ------------------------------------------------------------------------
 * Dragging a program to Favorites
 *
 * Pressing the mouse button on a program arms the drag. It becomes a drag
 * once the mouse has moved a few dots; until then it is still a click.
 * --------------------------------------------------------------------- */

void DragReset(void)
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

LRESULT CALLBACK PanelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
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

void CreatePanel(void)
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
