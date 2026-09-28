/* icons.c - Searchlight 98: icons of programs and files. */
#include "slight98.h"
#include "icon.h"

/* Gives the memory of the icons back. They are loaded again when next shown. */
void FreeIcons(ITEM *items, int count)
{
    int i;
    for (i = 0; i < count; i++) {
        if (items[i].iconState & ICON_SMALL_OWN) DestroyIcon(items[i].small);
        if (items[i].iconState & ICON_LARGE_OWN) DestroyIcon(items[i].large);
        items[i].small = items[i].large = NULL;
        items[i].iconState = 0;
    }
}

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

void IndexedIcons(const char *file, int index, HICON *large, HICON *small)
{
    char full[MAX_PATH], *part;
    lstrcpy(full, file);
    if (!FileExists(full) && SearchPath(NULL, file, NULL, MAX_PATH, full, &part) == 0) return;
    ExtractIconExA(full, index, large, small, 1);
}

/* Where a shortcut leads. Gives the file itself if it is not a shortcut. */
void LinkTarget(const char *path, char *target)
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
HICON LargeIcon(ITEM *it)
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
HICON SmallIcon(ITEM *it)
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

void StartEager(void)
{
    if (g_testMode || !g_main) return;
    KillTimer(g_main, 6);
    if (!g_iconsPrograms || g_iconsLazy) return;
    g_eagerAt = 0;
    SetTimer(g_main, 6, 40, NULL);
}

void EagerTick(void)
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

HICON EmbeddedIcon(int size)
{
    if (size <= 16) return CreateIconFromResourceEx((PBYTE)ICON16, sizeof(ICON16), TRUE, 0x00030000, 16, 16, 0);
    return CreateIconFromResourceEx((PBYTE)ICON32, sizeof(ICON32), TRUE, 0x00030000, 32, 32, 0);
}

/* The small icon of a program file. Looked up once, then remembered. */
HICON SmallFileIcon(const char *path)
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
