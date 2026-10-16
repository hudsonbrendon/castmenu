/* Game list model. Entries come from /cd/LIST.INI (GDMENU Card Manager format,
 * so existing cards work unchanged) plus optional /cd/CASTMENU.INI extras.
 * Without LIST.INI the list is built by walking GDEMU slots (slow path). */
#pragma once
#include <stdint.h>

#define LIST_MAX 999
#define NAME_MAX_LEN 64
#define CAT_MAX 32
#define CAT_NAME_LEN 20
/* fixed categories come first; named ones (from CASTMENU.INI NN.category=) follow */
enum { CAT_ALL = 0, CAT_FAVORITES, CAT_RECENT, CAT_FIRST_NAMED };

typedef struct {
    uint16_t slot;              /* GDEMU folder number (02..999) */
    char name[NAME_MAX_LEN + 1];
    char disc[4];               /* "1/2" or "" */
    char region[4];             /* subset of "JUE" */
    char version[7];
    char date[9];               /* YYYYMMDD */
    char product[11];
    char genre[17];             /* CASTMENU.INI extras */
    char year[5];
    char publisher[20];
    uint8_t vga, is_gd, players, favorite, has_cover, hidden;
    uint8_t category;           /* index into gamelist_t.cat_names (1-based), 0 = none */
    uint32_t collections;       /* bit i = member of gamelist_t.col_names[i] (CASTMENU.INI NN.collection=) */
} game_t;

typedef enum { SORT_SLOT, SORT_NAME, SORT_DATE, SORT_REGION, SORT_COUNT } sort_mode_t;

typedef struct {
    game_t items[LIST_MAX];
    int count;
    /* view = indexes into items after sort/filter */
    uint16_t view[LIST_MAX];
    int view_count;
    sort_mode_t sort;
    char filter_letter;         /* 0 = none */
    char filter_region;         /* 0 = none */
    int  only_favorites;
    int  only_recent;           /* order by settings recent list */
    int  hide_multidisc_extra;  /* hide discs 2..n of a set */
    int  category;              /* CAT_ALL.. ; named categories filter by game_t.category */
    char cat_names[CAT_MAX][CAT_NAME_LEN];
    int  cat_count;             /* named categories (indexes 1..cat_count in game_t.category) */
    char col_names[32][CAT_NAME_LEN];   /* user collections: carousels after the categories */
    int  col_count;
    char search[24];            /* case-insensitive title filter (on-screen keyboard), "" = none */
} gamelist_t;

extern gamelist_t g_list;

/* progress(cb) is called during the slow scan path; 0..100 */
int  list_load(void (*progress)(int pct, const char *msg));
void list_rebuild_view(void);
game_t *list_view_item(int view_idx);
int  list_view_index_of_slot(uint16_t slot);
int  list_category_total(void);                /* CAT_FIRST_NAMED + cat_count */
const char *list_category_name(int cat);       /* localized for the fixed ones */
void list_set_category(int cat);               /* sets only_favorites/only_recent + category, rebuilds */
int  list_synopsis(const game_t *g, const char *lang, char *out, int cap); /* /cd/info/<lang>/NNN.txt, 0 ok */
/* Multi-disc: returns number of entries sharing product id with g, fills out[] */
int  list_disc_set(const game_t *g, game_t **out, int max);
void list_toggle_favorite(game_t *g);
const char *list_sort_name(sort_mode_t m);
