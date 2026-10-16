#include <arpa/inet.h>
#include <stdint.h>
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

enum client_message_type {
    CLIENT_CREATE_GAME = 1,
    CLIENT_JOIN_GAME,
    CLIENT_MOVE,
    CLIENT_RESUME
};

enum server_message_status {
    SERVER_WAITING = 1,
    SERVER_ACTIVE,
    SERVER_FINISHED,
    SERVER_TIE,
    SERVER_ERROR
};

struct client_request {
    int32_t type;
    int32_t game_id;
    int32_t column;
    int32_t player;
};

struct server_response {
    int32_t status;
    int32_t player;
    int32_t winner;
    int32_t game_id;
    int32_t current_player;
    int32_t cells[BOARD_ROWS * BOARD_COLUMNS];
    char error[128];
};

#endif