#include "game.h"
#include <stdlib.h>
#include <stddef.h>

/*
Takes as input a pointer to a connect4 game board, a player (0/1), and a int column.
Places a new piece in `col` on the game board.
*/
int make_move(struct game* g, int player, int col) {
    if (g == NULL || player < 0 || player > 1 ||
        col < 0 || col >= BOARD_COLUMNS || player != g->currentPlayer) {
        return -1;
    }

    for (int row = BOARD_ROWS - 1; row >= 0; row--) {
        if (g->board.cells[row][col] == EMPTY_CELL) {
            g->board.cells[row][col] = player;
            g->currentPlayer = (player + 1) % 2;
            return 0;
        }
    }

    return -1;
}
 
/*
Takes as input a pointer to a connect4 game board.
Returns 0 or 1 if player 0 or 1 has won respectively.
Returns -1 if there is no winner.
*/
int check_win(struct game* g) {
    if (g == NULL) {
        return -1;
    }

    const int directions[][2] = {
        {0, 1},
        {1, 0},
        {1, 1},
        {1, -1}
    };

    for (int row = 0; row < BOARD_ROWS; row++) {
        for (int col = 0; col < BOARD_COLUMNS; col++) {
            int player = g->board.cells[row][col];
            if (player != 0 && player != 1) {
                continue;
            }

            for (size_t direction = 0;
                 direction < sizeof(directions) / sizeof(directions[0]);
                 direction++) {
                int end_row = row + 3 * directions[direction][0];
                int end_col = col + 3 * directions[direction][1];
                if (end_row < 0 || end_row >= BOARD_ROWS ||
                    end_col < 0 || end_col >= BOARD_COLUMNS) {
                    continue;
                }

                int connected = 1;
                for (int offset = 1; offset < 4; offset++) {
                    int check_row = row + offset * directions[direction][0];
                    int check_col = col + offset * directions[direction][1];
                    if (g->board.cells[check_row][check_col] != player) {
                        connected = 0;
                        break;
                    }
                }

                if (connected) {
                    return player;
                }
            }
        }
    }

    return -1;
}
 
/*
Initializes an existing game with an empty board.
*/
void new_game(struct game* g, int gameId) {
    for (int row = 0; row < BOARD_ROWS; row++) {
        for (int col = 0; col < BOARD_COLUMNS; col++) {
            g->board.cells[row][col] = EMPTY_CELL;
        }
    }

    g->gameId = gameId;
    g->currentPlayer = 0;
}
