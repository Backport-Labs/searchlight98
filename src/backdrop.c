/* backdrop.c - Searchlight 98: the blurred, darkened picture behind the panel.
 *
 * While the panel is open, the rest of the screen is covered by a blurred,
 * darkened picture of itself, so the panel stands out. Windows 95 and 98
 * cannot make a window see-through. Instead, just before the panel opens,
 * the screen is copied at quarter size, blurred, darkened and stretched back
 * into a full-screen window that sits behind the panel.
 */
#include "slight98.h"

static BYTE  *g_dimBits = NULL;         /* the screen at quarter size, blurred and darkened */

#define DIM_SCALE  4       /* the blur works on a copy this many times smaller */

#define DIM_LIMIT  400     /* milliseconds; slower than this and the backdrop turns itself off */

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
void FakeDesktop(HDC dc, int w, int h)
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

void FreeBackdrop(void)
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
void MakeBackdrop(void)
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
void PaintBackdrop(HDC dc, const RECT *area)
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

LRESULT CALLBACK BackProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
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
