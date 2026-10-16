#include <arpa/inet.h>
#include <sys/socket.h>


#ifndef COMMON_H
#define COMMON_H

#define BOARD_ROWS 6
#define BOARD_COLUMNS 7
#define EMPTY_CELL -1

struct board {
    int cells[BOARD_ROWS][BOARD_COLUMNS];
};

struct game {
    struct board board;
    int gameId;
    int currentPlayer;
};

#endif