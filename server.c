#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>

#include "game.h"
#include "save.h"

#define PORT 8080

struct server_game {
    int in_use;
    int player_count;
    int connected[2];
    int recovered;
    struct game game;
    struct sockaddr_in players[2];
};

static int same_client(const struct sockaddr_in *left,
                       const struct sockaddr_in *right) {
    return left->sin_family == right->sin_family &&
           left->sin_port == right->sin_port &&
           left->sin_addr.s_addr == right->sin_addr.s_addr;
}

static int send_response(int sock_fd, const struct sockaddr_in *client_addr,
                         const struct server_game *server_game, int player,
                         int status, int winner, const char *error) {
    struct server_response response = {
        .status = (int32_t)htonl((uint32_t)status),
        .player = (int32_t)htonl((uint32_t)player),
        .winner = (int32_t)htonl((uint32_t)winner),
        .game_id = (int32_t)htonl((uint32_t)server_game->game.gameId),
        .current_player = (int32_t)htonl((uint32_t)server_game->game.currentPlayer)
    };

    for (int row = 0; row < BOARD_ROWS; row++) {
        for (int col = 0; col < BOARD_COLUMNS; col++) {
            int index = row * BOARD_COLUMNS + col;
            response.cells[index] = (int32_t)htonl(
                (uint32_t)server_game->game.board.cells[row][col]);
        }
    }
    if (error != NULL) {
        snprintf(response.error, sizeof(response.error), "%s", error);
    }

    ssize_t sent = sendto(sock_fd, &response, sizeof(response), 0,
                          (const struct sockaddr *)client_addr,
                          sizeof(*client_addr));
    if (sent < 0) {
        perror("sendto");
        return -1;
    }
    if ((size_t)sent != sizeof(response)) {
        fprintf(stderr, "Failed to send complete response\n");
        return -1;
    }
    return 0;
}

static int find_client_player(const struct server_game *server_game,
                              const struct sockaddr_in *client_addr) {
    for (int player = 0; player < 2; player++) {
        if (server_game->connected[player] &&
            same_client(&server_game->players[player], client_addr)) {
            return player;
        }
    }
    return -1;
}

static int register_client(struct server_game *server_game, int player,
                           const struct sockaddr_in *client_addr) {
    if (player < 0 || player > 1) {
        return -1;
    }

    int existing_player = find_client_player(server_game, client_addr);
    if (existing_player >= 0) {
        return existing_player == player ? 0 : -1;
    }
    if (server_game->connected[player]) {
        return -1;
    }

    server_game->players[player] = *client_addr;
    server_game->connected[player] = 1;
    server_game->player_count++;
    return 1;
}

static void send_game_state(int sock_fd, const struct server_game *server_game,
                            int status, int winner) {
    for (int player = 0; player < 2; player++) {
        if (!server_game->connected[player]) {
            continue;
        }
        if (send_response(sock_fd, &server_game->players[player],
                          server_game, player, status, winner, NULL) < 0) {
            fprintf(stderr, "Could not notify player %d in game %d\n",
                    player, server_game->game.gameId);
        }
    }
}

static void send_game_error(int sock_fd, const struct server_game *server_game,
                            const char *message) {
    for (int player = 0; player < 2; player++) {
        if (!server_game->connected[player]) {
            continue;
        }
        if (send_response(sock_fd, &server_game->players[player],
                          server_game, player, SERVER_ERROR, -1, message) < 0) {
            fprintf(stderr, "Could not notify player %d in game %d: %s\n",
                    player, server_game->game.gameId, message);
        }
    }
}

static int board_is_full(const struct game *game) {
    for (int col = 0; col < BOARD_COLUMNS; col++) {
        if (game->board.cells[0][col] == EMPTY_CELL) {
            return 0;
        }
    }
    return 1;
}


int main(int argc, char* argv[]) {
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(sock_fd);
        return 1;
    }

    printf("Message board server listening on port %d\n", PORT);

    struct server_game games[MAX_GAMES] = {0};
    int next_game_id = 0;
    int recovery_mode = argc > 1 && strcmp(argv[1], "1") == 0;
    if (recovery_mode) {
        char *database_path = "games.save";
        struct gameDB recovered_games;
        if (loadgame(&recovered_games, database_path) == 0) {
            for (size_t index = 0; index < recovered_games.game_count; index++) {
                struct game recovered_game = recovered_games.games[index];
                int game_id = recovered_game.gameId;
                games[game_id].in_use = 1;
                games[game_id].recovered = 1;
                games[game_id].game = recovered_game;
                if (game_id >= next_game_id) {
                    next_game_id = game_id + 1;
                }
            }
        } else if (errno == ENOENT) {
            for (int game_id = 0; game_id < MAX_GAMES; game_id++) {
                char filepath[64];
                int path_length = snprintf(filepath, sizeof(filepath),
                                           "game_%d.save", game_id);
                if (path_length < 0 || (size_t)path_length >= sizeof(filepath)) {
                    fprintf(stderr, "Could not construct save path for game %d\n",
                            game_id);
                    continue;
                }

                struct gameDB legacy_games;
                if (loadgame(&legacy_games, filepath) < 0) {
                    if (errno != ENOENT) {
                        perror(filepath);
                    }
                    continue;
                }
                if (legacy_games.game_count != 1 ||
                    legacy_games.games[0].gameId != game_id) {
                    fprintf(stderr, "Save file %s contains the wrong game ID\n",
                            filepath);
                    continue;
                }

                games[game_id].in_use = 1;
                games[game_id].recovered = 1;
                games[game_id].game = legacy_games.games[0];
                next_game_id = game_id + 1;
            }
        } else {
            perror(database_path);
        }
    }

    for (;;) {
        struct client_request request;
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        ssize_t received = recvfrom(sock_fd, &request, sizeof(request), 0,
                                    (struct sockaddr *)&client_addr,
                                    &client_len);
        if (received < 0) {
            perror("recvfrom");
            continue;
        }
        if ((size_t)received != sizeof(request)) {
            fprintf(stderr, "Ignoring malformed client request\n");
            continue;
        }

        int type = (int32_t)ntohl((uint32_t)request.type);
        int game_id = (int32_t)ntohl((uint32_t)request.game_id);
        int column = (int32_t)ntohl((uint32_t)request.column);
        int requested_player = (int32_t)ntohl((uint32_t)request.player);
        struct server_game *server_game = NULL;
        int player = -1;

        if (type == CLIENT_CREATE_GAME) {
            int existing_game = -1;
            int existing_player = -1;
            for (int id = 0; id < next_game_id; id++) {
                if (!games[id].in_use) {
                    continue;
                }
                int connected_player = find_client_player(&games[id], &client_addr);
                if (connected_player >= 0) {
                    existing_game = id;
                    existing_player = connected_player;
                    break;
                }
            }
            if (existing_game >= 0) {
                server_game = &games[existing_game];
                int winner = check_win(&server_game->game);
                int status = winner >= 0 ? SERVER_FINISHED :
                    board_is_full(&server_game->game) ? SERVER_TIE :
                    server_game->player_count == 2 ? SERVER_ACTIVE : SERVER_WAITING;
                if (status == SERVER_ACTIVE) {
                    send_game_state(sock_fd, server_game, status, -1);
                } else if (send_response(sock_fd, &client_addr, server_game,
                                         existing_player, status, winner, NULL) < 0) {
                    fprintf(stderr, "Could not resend initial game state\n");
                }
                continue;
            }

            if (next_game_id >= MAX_GAMES) {
                struct server_game unavailable = {0};
                unavailable.game.gameId = -1;
                if (send_response(sock_fd, &client_addr, &unavailable, -1,
                                  SERVER_ERROR, -1, "Server has reached its game limit") < 0) {
                    fprintf(stderr, "Could not send game-limit response\n");
                }
                continue;
            }

            game_id = next_game_id++;
            server_game = &games[game_id];
            server_game->in_use = 1;
            server_game->player_count = 1;
            new_game(&server_game->game, game_id);
            server_game->players[0] = client_addr;
            server_game->connected[0] = 1;

            if (send_response(sock_fd, &client_addr, server_game, 0,
                              SERVER_WAITING, -1, NULL) < 0) {
                fprintf(stderr, "Could not send initial game state\n");
            }
            continue;
        }

        if (game_id < 0 || game_id >= next_game_id ||
            !games[game_id].in_use) {
            struct server_game unavailable = {0};
            unavailable.game.gameId = game_id;
            if (send_response(sock_fd, &client_addr, &unavailable, -1,
                              SERVER_ERROR, -1, "Game does not exist") < 0) {
                fprintf(stderr, "Could not send missing-game response\n");
            }
            continue;
        }
        server_game = &games[game_id];

        if (type == CLIENT_RESUME) {
            int previous_player_count = server_game->player_count;
            if (register_client(server_game, requested_player, &client_addr) < 0) {
                if (send_response(sock_fd, &client_addr, server_game, -1,
                                  SERVER_ERROR, -1,
                                  "Could not restore player connection") < 0) {
                    fprintf(stderr, "Could not send resume error response\n");
                }
                continue;
            }

            if (server_game->player_count == 2) {
                int winner = check_win(&server_game->game);
                int status = winner >= 0 ? SERVER_FINISHED :
                    board_is_full(&server_game->game) ? SERVER_TIE : SERVER_ACTIVE;
                if (previous_player_count < 2) {
                    send_game_state(sock_fd, server_game, status, winner);
                } else if (send_response(sock_fd, &client_addr, server_game,
                                         requested_player, status, winner,
                                         NULL) < 0) {
                    fprintf(stderr, "Could not send resumed game state\n");
                }
            }
            continue;
        }

        if (type == CLIENT_JOIN_GAME) {
            player = find_client_player(server_game, &client_addr);
            if (player >= 0) {
                int winner = check_win(&server_game->game);
                int status = winner >= 0 ? SERVER_FINISHED :
                    board_is_full(&server_game->game) ? SERVER_TIE :
                    server_game->player_count == 2 ? SERVER_ACTIVE : SERVER_WAITING;
                if (send_response(sock_fd, &client_addr, server_game, player,
                                  status, winner, NULL) < 0) {
                    fprintf(stderr, "Could not resend join state\n");
                }
                continue;
            }
            if (server_game->player_count != 1) {
                if (send_response(sock_fd, &client_addr, server_game, -1,
                                  SERVER_ERROR, -1, "Game already has two players") < 0) {
                    fprintf(stderr, "Could not send full-game response\n");
                }
                continue;
            }
            if (same_client(&server_game->players[0], &client_addr)) {
                if (send_response(sock_fd, &client_addr, server_game, -1,
                                  SERVER_ERROR, -1, "Cannot join your own game") < 0) {
                    fprintf(stderr, "Could not send join response\n");
                }
                continue;
            }

            server_game->players[1] = client_addr;
            server_game->connected[1] = 1;
            server_game->player_count = 2;
            send_game_state(sock_fd, server_game, SERVER_ACTIVE, -1);
            continue;
        }

        if (type != CLIENT_MOVE) {
            if (send_response(sock_fd, &client_addr, server_game, -1,
                              SERVER_ERROR, -1, "Unknown request type") < 0) {
                fprintf(stderr, "Could not send invalid-request response\n");
            }
            continue;
        }

        player = find_client_player(server_game, &client_addr);
        if (player >= 0 && player != requested_player) {
            player = -1;
        } else if (player < 0 && server_game->recovered &&
                   register_client(server_game, requested_player, &client_addr) >= 0) {
            player = requested_player;
        }
        if (player < 0 || (server_game->player_count != 2 &&
                           !server_game->recovered)) {
            if (send_response(sock_fd, &client_addr, server_game, -1,
                              SERVER_ERROR, -1, "Client is not a player in this game") < 0) {
                fprintf(stderr, "Could not send unauthorized-move response\n");
            }
            continue;
        }
        struct game previous_game = server_game->game;
        if (make_move(&server_game->game, player, column) < 0) {
            if (send_response(sock_fd, &client_addr, server_game, player,
                              SERVER_ACTIVE, -1, NULL) < 0) {
                fprintf(stderr, "Could not resend game state after invalid move\n");
            }
            continue;
        }

        struct gameDB saved_games = {0};
        for (int id = 0; id < next_game_id; id++) {
            if (games[id].in_use) {
                saved_games.games[saved_games.game_count++] = games[id].game;
            }
        }

        if (savegame(&saved_games, "games.save") < 0) {
            int save_errno = errno;
            server_game->game = previous_game;
            errno = save_errno;
            perror("savegame");
            send_game_error(sock_fd, server_game, "Could not save game state");
            continue;
        }

        int winner = check_win(&server_game->game);
        if (winner >= 0) {
            send_game_state(sock_fd, server_game, SERVER_FINISHED, winner);
        } else if (board_is_full(&server_game->game)) {
            send_game_state(sock_fd, server_game, SERVER_TIE, -1);
        } else {
            send_game_state(sock_fd, server_game, SERVER_ACTIVE, -1);
        }
    }

    close(sock_fd);

    return 0;
}