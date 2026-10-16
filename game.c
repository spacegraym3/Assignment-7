#include "game.h"
#include <stdlib.h>
#include <stddef.h>

/*
Takes as input a pointer to a connect4 game board, a player (0/1), and a int column.
Places a new piece in `col` on the game board.
*/
int make_move(struct game* g, int player, int col) {
    
}
 
/*
Takes as input a pointer to a connect4 game board.
Returns 0 or 1 if player 0 or 1 has won respectively.
Returns -1 if there is no winner.
*/
int check_win(struct game* g) {
   
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

