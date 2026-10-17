#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "save.h"

#define GAME_FILE_MAGIC UINT32_C(0x43344631)
#define GAME_FILE_VERSION 2
#define GAME_RECORD_WORDS (2 + BOARD_ROWS * BOARD_COLUMNS)
#define LEGACY_GAME_FILE_WORDS (4 + BOARD_ROWS * BOARD_COLUMNS)
#define GAME_FILE_HEADER_WORDS 3

static int write_all(int fd, const void *buffer, size_t size) {
    const unsigned char *bytes = buffer;
    size_t written = 0;
    while (written < size) {
        ssize_t result = write(fd, bytes + written, size - written);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (result == 0) {
            errno = EIO;
            return -1;
        }
        written += (size_t)result;
    }
    return 0;
}

static int read_all(int fd, void *buffer, size_t size) {
    unsigned char *bytes = buffer;
    size_t received = 0;
    while (received < size) {
        ssize_t result = read(fd, bytes + received, size - received);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (result == 0) {
            errno = EINVAL;
            return -1;
        }
        received += (size_t)result;
    }
    return 0;
}

static int valid_game(const struct game *game) {
    if (game->gameId < 0 || game->gameId >= MAX_GAMES ||
        (game->currentPlayer != 0 && game->currentPlayer != 1)) {
        return 0;
    }
    for (int row = 0; row < BOARD_ROWS; row++) {
        for (int col = 0; col < BOARD_COLUMNS; col++) {
            int cell = game->board.cells[row][col];
            if (cell != EMPTY_CELL && cell != 0 && cell != 1) {
                return 0;
            }
        }
    }
    return 1;
}

/*
Saves the game database to the specified filepath.
Returns 0 on success, -1 on failure.
*/
int savegame(struct gameDB* game_db, char* filepath) {
    if (game_db == NULL || filepath == NULL || filepath[0] == '\0' ||
        game_db->game_count > MAX_GAMES) {
        errno = EINVAL;
        return -1;
    }

    size_t word_count = GAME_FILE_HEADER_WORDS +
        game_db->game_count * GAME_RECORD_WORDS;
    uint32_t *words = malloc(word_count * sizeof(*words));
    if (words == NULL) {
        return -1;
    }
    words[0] = htonl(GAME_FILE_MAGIC);
    words[1] = htonl(GAME_FILE_VERSION);
    words[2] = htonl((uint32_t)game_db->game_count);

    int seen_game_ids[MAX_GAMES] = {0};
    for (size_t game_index = 0; game_index < game_db->game_count; game_index++) {
        const struct game *game = &game_db->games[game_index];
        if (!valid_game(game) || seen_game_ids[game->gameId]) {
            free(words);
            errno = EINVAL;
            return -1;
        }
        seen_game_ids[game->gameId] = 1;

        size_t record_index = GAME_FILE_HEADER_WORDS +
            game_index * GAME_RECORD_WORDS;
        words[record_index] = htonl((uint32_t)game->gameId);
        words[record_index + 1] = htonl((uint32_t)game->currentPlayer);
        for (int row = 0; row < BOARD_ROWS; row++) {
            for (int col = 0; col < BOARD_COLUMNS; col++) {
                int cell = game->board.cells[row][col];
                size_t cell_index = record_index + 2 +
                    (size_t)row * BOARD_COLUMNS + (size_t)col;
                words[cell_index] = htonl((uint32_t)(int32_t)cell);
            }
        }
    }

    size_t path_length = strlen(filepath);
    const char suffix[] = ".tmp.XXXXXX";
    if (path_length > SIZE_MAX - sizeof(suffix)) {
        free(words);
        errno = ENAMETOOLONG;
        return -1;
    }
    char *temporary_path = malloc(path_length + sizeof(suffix));
    if (temporary_path == NULL) {
        free(words);
        return -1;
    }
    snprintf(temporary_path, path_length + sizeof(suffix), "%s%s",
             filepath, suffix);

    int fd = mkstemp(temporary_path);
    if (fd < 0) {
        free(words);
        free(temporary_path);
        return -1;
    }

    int result = write_all(fd, words, word_count * sizeof(*words));
    free(words);
    if (result == 0 && fsync(fd) < 0) {
        result = -1;
    }
    int saved_errno = errno;
    if (close(fd) < 0 && result == 0) {
        result = -1;
        saved_errno = errno;
    }
    if (result == 0 && rename(temporary_path, filepath) < 0) {
        result = -1;
        saved_errno = errno;
    }
    if (result < 0) {
        unlink(temporary_path);
        errno = saved_errno;
    }

    free(temporary_path);
    return result;
}

static int check_no_trailing_data(int fd) {
    unsigned char extra;
    ssize_t trailing;
    do {
        trailing = read(fd, &extra, sizeof(extra));
    } while (trailing < 0 && errno == EINTR);
    if (trailing < 0) {
        return -1;
    }
    if (trailing != 0) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

static int decode_game(struct game *game, const uint32_t *words,
                       size_t record_index) {
    uint32_t game_id = ntohl(words[record_index]);
    uint32_t current_player = ntohl(words[record_index + 1]);
    if (game_id >= MAX_GAMES || current_player > 1) {
        errno = EINVAL;
        return -1;
    }

    game->gameId = (int)game_id;
    game->currentPlayer = (int)current_player;
    for (int row = 0; row < BOARD_ROWS; row++) {
        for (int col = 0; col < BOARD_COLUMNS; col++) {
            size_t cell_index = record_index + 2 +
                (size_t)row * BOARD_COLUMNS + (size_t)col;
            int32_t cell = (int32_t)ntohl(words[cell_index]);
            if (cell != EMPTY_CELL && cell != 0 && cell != 1) {
                errno = EINVAL;
                return -1;
            }
            game->board.cells[row][col] = cell;
        }
    }
    return 0;
}

/*
Loads a game database written by savegame.
Returns 0 on success, -1 on failure.
*/
int loadgame(struct gameDB* game_db, char* filepath) {
    if (game_db == NULL || filepath == NULL || filepath[0] == '\0') {
        errno = EINVAL;
        return -1;
    }

    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    uint32_t words[GAME_FILE_HEADER_WORDS +
                   MAX_GAMES * GAME_RECORD_WORDS];
    int result = read_all(fd, words, 2 * sizeof(*words));
    uint32_t version = result == 0 ? ntohl(words[1]) : 0;
    size_t game_count = 0;
    size_t total_words = 0;

    if (result == 0 && ntohl(words[0]) != GAME_FILE_MAGIC) {
        errno = EINVAL;
        result = -1;
    } else if (result == 0 && version == 1) {
        total_words = LEGACY_GAME_FILE_WORDS;
        result = read_all(fd, words + 2,
                          (total_words - 2) * sizeof(*words));
        game_count = 1;
    } else if (result == 0 && version == GAME_FILE_VERSION) {
        result = read_all(fd, words + 2, sizeof(*words));
        if (result == 0) {
            game_count = ntohl(words[2]);
            if (game_count > MAX_GAMES) {
                errno = EINVAL;
                result = -1;
            } else {
                total_words = GAME_FILE_HEADER_WORDS +
                    game_count * GAME_RECORD_WORDS;
                result = read_all(fd, words + 3,
                                  (total_words - 3) * sizeof(*words));
            }
        }
    } else if (result == 0) {
        errno = EINVAL;
        result = -1;
    }

    if (result == 0) {
        result = check_no_trailing_data(fd);
    }

    int saved_errno = errno;
    if (close(fd) < 0 && result == 0) {
        result = -1;
        saved_errno = errno;
    }
    if (result < 0) {
        errno = saved_errno;
        return -1;
    }

    struct gameDB loaded_game_db = {0};
    loaded_game_db.game_count = game_count;
    int seen_game_ids[MAX_GAMES] = {0};
    for (size_t game_index = 0; game_index < game_count; game_index++) {
        size_t record_index = version == 1 ? 2 :
            GAME_FILE_HEADER_WORDS + game_index * GAME_RECORD_WORDS;
        if (version == 1) {
            uint32_t game_id = ntohl(words[2]);
            uint32_t current_player = ntohl(words[3]);
            if (game_id >= MAX_GAMES || current_player > 1) {
                errno = EINVAL;
                return -1;
            }
            loaded_game_db.games[game_index].gameId = (int)game_id;
            loaded_game_db.games[game_index].currentPlayer = (int)current_player;
            for (int row = 0; row < BOARD_ROWS; row++) {
                for (int col = 0; col < BOARD_COLUMNS; col++) {
                    size_t cell_index = 4 +
                        (size_t)row * BOARD_COLUMNS + (size_t)col;
                    int32_t cell = (int32_t)ntohl(words[cell_index]);
                    if (cell != EMPTY_CELL && cell != 0 && cell != 1) {
                        errno = EINVAL;
                        return -1;
                    }
                    loaded_game_db.games[game_index].board.cells[row][col] = cell;
                }
            }
        } else if (decode_game(&loaded_game_db.games[game_index], words,
                               record_index) < 0) {
            return -1;
        }

        int game_id = loaded_game_db.games[game_index].gameId;
        if (seen_game_ids[game_id]) {
            errno = EINVAL;
            return -1;
        }
        seen_game_ids[game_id] = 1;
    }

    *game_db = loaded_game_db;
    return 0;
}
