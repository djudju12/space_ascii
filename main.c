#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <sys/poll.h>
#include <termios.h>
#include <unistd.h>

#define WIDTH  60
#define HEIGHT 20

struct Game {
    struct pollfd fds;
    int map[HEIGHT][WIDTH];
    bool over;
};

struct Game game = {0};

void game_init() {
    game.fds.fd = STDIN_FILENO;
    game.fds.events = POLLIN;
}

bool should_stop() {
    return game.over;
}

struct termios old_term;

void disable_raw_mode(void) {
    tcsetattr(game.fds.fd, TCSAFLUSH, &old_term);
    printf("\e[?25h");
}

void enable_raw_mode(void) {
    tcgetattr(game.fds.fd, &old_term);
    atexit(disable_raw_mode);

    struct termios raw = old_term;
    raw.c_lflag &= ~(ECHO | ICANON);

    tcsetattr(game.fds.fd, TCSAFLUSH, &raw);
    printf("\e[?25l");
    printf("\033[2J");
}

int has_input(void) {
    return poll(&game.fds, 1, 0);
}

void update() {
    char c;
    if (has_input()) {
        read(STDIN_FILENO, &c, 1);
        switch (c) {
            default:
        }
    }
}

void draw() {
    printf("\033[0;0f");
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            printf("%c", '#');
        }
        printf("\n");
    }
}

void clear() {
}

int main(void) {
    enable_raw_mode();
    game_init();
    while (!should_stop()) {
        update();
        clear();
        draw();
    }

    return 0;
}