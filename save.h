#include "common.h"

#include <stddef.h>

#define MAX_GAMES 100

struct gameDB {
    size_t game_count;
    struct game games[MAX_GAMES];
};

int savegame(struct gameDB* game_db, const char* filepath);
int loadgame(struct gameDB* game_db, const char* filepath);
