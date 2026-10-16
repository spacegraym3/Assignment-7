#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>

#include "common.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080


// prints the 7x6 flattened board array in the correct format
static void print_board(const int32_t board[]) {
 
    printf("\n");
    for (int col = 0; col < BOARD_COLUMNS; col++)
        printf(" %d", col);
    printf("\n");
 
    for (int row = 0; row < BOARD_ROWS; row++) {
        printf("|");
        for (int col = 0; col < BOARD_COLUMNS; col++) {
            int32_t cell = board[row * BOARD_COLUMNS + col];
            printf("%c|", cell == 0 ? 'X' : cell == 1 ? 'O' : '.');
        }
        printf("\n");
    }
    printf("\n");
}

static int send_request(int sock_fd, const struct sockaddr_in *server_addr,
                        int32_t type, int32_t game_id, int32_t column,
                        int32_t player) {
    struct client_request request = {
        .type = htonl(type),
        .game_id = htonl(game_id),
        .column = htonl(column),
        .player = htonl(player)
    };
    ssize_t sent = sendto(sock_fd, &request, sizeof(request), 0, (const struct sockaddr *)server_addr, sizeof(*server_addr));
    return 0;
}

static int receive_response(int sock_fd, struct server_response *response) {
    ssize_t received = recvfrom(sock_fd, response, sizeof(*response), 0, NULL, NULL);
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 1;
        }
        perror("recvfrom");
        return -1;
    }
    if ((size_t)received != sizeof(*response)) {
        fprintf(stderr, "Received an invalid response from the server\n");
        return -1;
    }

    response->status = (int32_t)ntohl((uint32_t)response->status);
    response->player = (int32_t)ntohl((uint32_t)response->player);
    response->winner = (int32_t)ntohl((uint32_t)response->winner);
    response->game_id = (int32_t)ntohl((uint32_t)response->game_id);
    response->current_player = (int32_t)ntohl((uint32_t)response->current_player);
    for (size_t i = 0; i < BOARD_ROWS * BOARD_COLUMNS; i++) {
        response->cells[i] = (int32_t)ntohl((uint32_t)response->cells[i]);
    }
    response->error[sizeof(response->error) - 1] = '\0';
    return 0;
}

int main() {
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return 1;
    }

    struct timeval receive_timeout = {
        .tv_sec = 0,
        .tv_usec = 500000
    };
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &receive_timeout,
                   sizeof(receive_timeout)) < 0) {
        perror("setsockopt");
        close(sock_fd);
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid server address: %s\n", SERVER_IP);
        close(sock_fd);
        return 1;
    }


    printf("Connect 4\n");
    printf("  1) Create a new game\n");
    printf("  2) Join a game\n");
    printf("> ");
    int choice = 0;
    if (scanf("%d", &choice) != 1 || (choice != 1 && choice != 2)) {
        fprintf(stderr, "Please choose 1 or 2\n");
        close(sock_fd);
        return 1;
    }

    int game_id = -1;
    int32_t request_type = CLIENT_CREATE_GAME;
    if (choice == 2) {
        printf("Game ID? >");
        if (scanf("%d", &game_id) != 1 || game_id < 0) {
            fprintf(stderr, "Please enter a valid game ID\n");
            close(sock_fd);
            return 1;
        }
        request_type = CLIENT_JOIN_GAME;
    }

    if (send_request(sock_fd, &server_addr, request_type, game_id, -1, -1) < 0) {
        close(sock_fd);
        return 1;
    }

    int player = -1;
    int displayed_state = 0;
    int32_t displayed_status = -1;
    int32_t displayed_winner = -1;
    int32_t displayed_current_player = -1;
    int32_t displayed_cells[BOARD_ROWS * BOARD_COLUMNS];
    int result = 0;
    for (;;) {
        struct server_response response;
        int receive_result = receive_response(sock_fd, &response);
        if (receive_result == 1) {
            int32_t retry_type = player < 0 ? request_type : CLIENT_RESUME;
            int32_t retry_game_id = game_id;
            int32_t retry_player = player < 0 ? -1 : player;
            if (send_request(sock_fd, &server_addr, retry_type, retry_game_id,
                             -1, retry_player) < 0) {
                result = 1;
                break;
            }
            continue;
        }
        if (receive_result < 0) {
            result = 1;
            break;
        }

        if (response.status == SERVER_ERROR) {
            fprintf(stderr, "Server error: %s\n", response.error);
            result = 1;
            break;
        }
        if (response.status != SERVER_WAITING &&
            response.status != SERVER_ACTIVE &&
            response.status != SERVER_FINISHED &&
            response.status != SERVER_TIE) {
            fprintf(stderr, "Received an unknown response from the server\n");
            result = 1;
            break;
        }

        if (player < 0) {
            player = response.player;
            if (player != 0 && player != 1) {
                fprintf(stderr, "Received an invalid player assignment\n");
                result = 1;
                break;
            }
            game_id = response.game_id;
        }

        int state_changed = !displayed_state ||
            response.status != displayed_status ||
            response.winner != displayed_winner ||
            response.current_player != displayed_current_player ||
            memcmp(response.cells, displayed_cells, sizeof(displayed_cells)) != 0;
        if (state_changed) {
            printf("\nCurrent Game\n");
            printf("---------------------\n");
            printf("GameID: %d\n", response.game_id);
            print_board(response.cells);

            displayed_state = 1;
            displayed_status = response.status;
            displayed_winner = response.winner;
            displayed_current_player = response.current_player;
            memcpy(displayed_cells, response.cells, sizeof(displayed_cells));
        }

        if (response.status == SERVER_FINISHED) {
            puts(response.winner == player ? "YOU WIN!" : "YOU LOSE!");
            break;
        }
        if (response.status == SERVER_TIE) {
            printf("TIE\n");
            break;
        }
        if (response.status == SERVER_WAITING) {
            if (state_changed) {
                printf("waiting for other player...\n");
            }
            continue;
        }

        if (response.current_player == player) {
            int column = -1;
            printf("\nInput Move Row: > ");
            if (scanf("%d", &column) != 1 ||
                column < 0 || column >= BOARD_COLUMNS) {
                fprintf(stderr, "Please enter a column from 0 to %d\n",
                        BOARD_COLUMNS - 1);
                result = 1;
                break;
            }
            if (send_request(sock_fd, &server_addr, CLIENT_MOVE,
                             response.game_id, column, player) < 0) {
                result = 1;
                break;
            }
        } else {
            if (state_changed) {
                printf("waiting for other player...\n");
            }
        }
    }

    close(sock_fd);

    return result;
}