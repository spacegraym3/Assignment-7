#include "common.h"

#define MAX_GAMES 100  /* must match the malloc in server.c main() */

int savegame(const struct game* game, const char* filepath);
int loadgame(struct game* game, const char* filepath);