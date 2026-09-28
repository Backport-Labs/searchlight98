/* state.c - Searchlight 98: favorites, recent programs and usage counts, kept in STATE.TXT. */
#include "slight98.h"

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

void LoadState(void)
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
int CountUse(const char *id)
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
void ApplyUses(void)
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

void SaveState(void)
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

void PrunePins(char list[][110], int *count)
{
    int i, n = 0;
    for (i = 0; i < *count; i++) if (FindById(list[i]) >= 0) { if (n != i) lstrcpy(list[n], list[i]); n++; }
    *count = n;
}

/* Puts a program in Favorites in front of place "pos", or moves it there if
 * it is already pinned. Returns 0 = added, 1 = moved, 2 = Favorites is full. */
int PinMove(const char *which, int pos)
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

void PinRemove(int p)
{
    int i;
    if (p < 0 || p >= g_favCount) return;
    for (i = p; i < g_favCount - 1; i++) lstrcpy(g_favs[i], g_favs[i + 1]);
    g_favCount--;
}
