/* search.c - Searchlight 98: searching the list. */
#include "slight98.h"

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

void Search(const char *query)
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
