#include <stddef.h>

#include "common.h"

#define MAX_GAMES 100  /* must match the malloc in server.c main() */

struct gameDB {
    size_t game_count;
    struct game games[MAX_GAMES];
};

int savegame(struct gameDB* game_db, char* filepath);
int loadgame(struct gameDB* game_db, char* filepath);
