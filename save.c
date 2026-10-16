#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"

#define GAME_FILE_MAGIC UINT32_C(0x43344631)
#define GAME_FILE_VERSION 1
#define GAME_FILE_WORDS (4 + BOARD_ROWS * BOARD_COLUMNS)

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

/*
Saves server game data to specified filepath.
Returns 0 on success, -1 on failure.
*/
int savegame(const struct game* game, const char* filepath) {
    if (game == NULL || filepath == NULL || filepath[0] == '\0' ||
        game->gameId < 0 ||
        (game->currentPlayer != 0 && game->currentPlayer != 1)) {
        errno = EINVAL;
        return -1;
    }

    uint32_t words[GAME_FILE_WORDS] = {
        htonl(GAME_FILE_MAGIC),
        htonl(GAME_FILE_VERSION),
        htonl((uint32_t)game->gameId),
        htonl((uint32_t)game->currentPlayer)
    };
    for (int row = 0; row < BOARD_ROWS; row++) {
        for (int col = 0; col < BOARD_COLUMNS; col++) {
            int cell = game->board.cells[row][col];
            if (cell != EMPTY_CELL && cell != 0 && cell != 1) {
                errno = EINVAL;
                return -1;
            }
            size_t index = 4 + (size_t)row * BOARD_COLUMNS + (size_t)col;
            words[index] = htonl((uint32_t)(int32_t)cell);
        }
    }

    size_t path_length = strlen(filepath);
    const char suffix[] = ".tmp.XXXXXX";
    if (path_length > SIZE_MAX - sizeof(suffix)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    char *temporary_path = malloc(path_length + sizeof(suffix));
    if (temporary_path == NULL) {
        return -1;
    }
    snprintf(temporary_path, path_length + sizeof(suffix), "%s%s",
             filepath, suffix);

    int fd = mkstemp(temporary_path);
    if (fd < 0) {
        free(temporary_path);
        return -1;
    }

    int result = write_all(fd, words, sizeof(words));
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

/*
Loads a file written by savegame into game.
Returns 0 on success, -1 on failure.
*/
int loadgame(struct game* game, const char* filepath) {
    if (game == NULL || filepath == NULL || filepath[0] == '\0') {
        errno = EINVAL;
        return -1;
    }

    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    uint32_t words[GAME_FILE_WORDS];
    int result = read_all(fd, words, sizeof(words));
    if (result == 0) {
        unsigned char extra;
        ssize_t trailing;
        do {
            trailing = read(fd, &extra, sizeof(extra));
        } while (trailing < 0 && errno == EINTR);
        if (trailing < 0) {
            result = -1;
        } else if (trailing != 0) {
            errno = EINVAL;
            result = -1;
        }
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

    if (ntohl(words[0]) != GAME_FILE_MAGIC ||
        ntohl(words[1]) != GAME_FILE_VERSION ||
        ntohl(words[2]) > INT32_MAX ||
        ntohl(words[3]) > 1) {
        errno = EINVAL;
        return -1;
    }

    struct game loaded_game = {
        .gameId = (int)ntohl(words[2]),
        .currentPlayer = (int)ntohl(words[3])
    };
    for (int row = 0; row < BOARD_ROWS; row++) {
        for (int col = 0; col < BOARD_COLUMNS; col++) {
            size_t index = 4 + (size_t)row * BOARD_COLUMNS + (size_t)col;
            int32_t cell = (int32_t)ntohl(words[index]);
            if (cell != EMPTY_CELL && cell != 0 && cell != 1) {
                errno = EINVAL;
                return -1;
            }
            loaded_game.board.cells[row][col] = cell;
        }
    }

    *game = loaded_game;
    return 0;
}
