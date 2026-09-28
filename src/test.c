/* test.c - Searchlight 98: test modes: the self-test and rendering the panel to a bitmap. */
#include "slight98.h"

int SelfTest(void)
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
int Shot(const char *file, const char *query)
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
