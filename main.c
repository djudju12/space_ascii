#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <time.h>

#include <sys/poll.h>
#include <termios.h>
#include <unistd.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(min, v, max) ((v) > (max) ? (max) : (v) < (min) ? (min) : (v))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define WIDTH  60
#define HEIGHT 20

struct Projectile{
    int x;
    float y;
    int vel_y;
};

typedef struct {
    int x, y;
    bool shooting;
    struct Projectile laser;
    int lifes;
} Entity;

#define ENEMY_COLS     11
#define ENEMY_ROWS      5
#define ENEMY_MAX_FOOTSTEPS 4

struct Game {
    struct pollfd fds;
    double last_frame;
    double enemy_step_acc;
    char map[HEIGHT][WIDTH];
    bool over;
    Entity enemies[ENEMY_COLS*ENEMY_ROWS];
    int score;
    int enemies_footsteps;
    int enemies_direction;
};

struct Game game = {0};

#define PLAYER_MAX_LIFES 3
Entity player    = {0};

void game_init() {
    game.fds.fd = STDIN_FILENO;
    game.fds.events = POLLIN;
    game.score = 0;
    player.x = WIDTH/2;
    player.y = HEIGHT - 2;
    player.lifes = PLAYER_MAX_LIFES;
    game.enemies_direction = 1;
    srand(time(NULL));

    const int x0 = 4;
    const int y0 = 2;
    const int padx = 3;
    const int pady = 2;
    for (int y = 0; y < ENEMY_ROWS; y++) {
        for (int x = 0; x < ENEMY_COLS; x++) {
            Entity *enemy = &game.enemies[x + y*ENEMY_COLS];
            enemy->lifes = 1;
            enemy->x = x*padx + x0;
            enemy->y = y*pady + y0;
        }
    }
}

bool should_stop() {
    return game.over;
}

double get_time_sec(void) {
    struct timespec spec;
    if (clock_gettime(CLOCK_MONOTONIC, &spec) == -1) exit(1);
    return spec.tv_sec + spec.tv_nsec/1e9;
}

double get_delta_time(void) {
    return get_time_sec() - game.last_frame;
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

// 1 = shoot up, -1 = shoot down
void shoot(Entity *entity, int direction) {
    const int vel_projectile = 8;
    entity->shooting = true;
    entity->laser.x = entity->x;
    entity->laser.y = entity->y - direction;
    entity->laser.vel_y = vel_projectile*direction;
}

void update_projectile(Entity *entity, double dt, char sprite) {
    entity->laser.y = entity->laser.y - entity->laser.vel_y*dt;
    int laser_y = entity->laser.y;
    game.map[laser_y][entity->laser.x] = sprite;
    const int max_y = entity->laser.vel_y > 0 ? 0 : HEIGHT - 1;
    if ((max_y == 0          && entity->laser.y < max_y) || // projectile going up
        (max_y == HEIGHT - 1 && entity->laser.y > max_y))   // projectile going down
    {
        entity->shooting = false;
    }
}

int choose_shooter(void) {
    int candidates[ENEMY_COLS] = {0};
    int cnt = 0;

    for (int y = ENEMY_ROWS - 1; y >= 0; y--) {
        for (int x = 0; x < ENEMY_COLS; x++) {
            int i = x + y*ENEMY_COLS;
            if (game.enemies[i].lifes > 0) {
                candidates[cnt++] = i;
            }
        }

        if (cnt > 0) break;
    }

    return candidates[rand() % cnt];
}

#define HIT_TARGET(shooter, victim)          \
    (shooter).shooting                    && \
    (int) (shooter).laser.y == (victim).y && \
    (shooter).laser.x == (victim).x       && \
    (victim).lifes > 0

void update() {
    char c;
    if (has_input()) {
        read(STDIN_FILENO, &c, 1);
        switch (c) {
            case 'a': {
                player.x = MAX(1, player.x - 1);
            } break;

            case 'd': {
                player.x = MIN(WIDTH - 2, player.x + 1);
            } break;

            case ' ': {
                if (!player.shooting) {
                    shoot(&player, 1);
                }
            } break;

            case 'q': {
                game.over = true;
            } break;

            default: {
                /* do nothing */
            } break;
        }
    }

    game.map[player.y][player.x - 1] = '-';
    game.map[player.y][player.x    ] = '^';
    game.map[player.y][player.x + 1] = '-';

    double dt = get_delta_time();
    game.enemy_step_acc += dt;
    game.last_frame = get_time_sec();
    if (player.shooting) {
        update_projectile(&player, dt, '*');
    }

    bool is_time_to_shoot = true;
    for (int i = 0; i < ENEMY_COLS*ENEMY_ROWS; i++) {
        Entity *enemy = &game.enemies[i];
        if (HIT_TARGET(player, *enemy)) {
            enemy->lifes -= 1;
            game.score += 1;
            player.shooting = false;
            player.laser.y = 0.0;
        }

        if (enemy->shooting) {
            if ((int) enemy->laser.y == player.y &&
                (
                    enemy->laser.x == player.x       ||
                    enemy->laser.x == (player.x - 1) ||
                    enemy->laser.x == (player.x + 1)
                ))
            {
                player.lifes = player.lifes - 1;
                enemy->shooting = false;
                enemy->laser.y = 0.0;
                game.over = player.lifes == 0;
            } else {
                update_projectile(enemy, dt, '.');
            }

            is_time_to_shoot = false;
        }

        if (game.enemy_step_acc > 0.5) enemy->x += game.enemies_direction;
        char enemy_body = enemy->lifes > 0 ? '@' : ' ';
        game.map[enemy->y][enemy->x] = enemy_body;
    }

    if (is_time_to_shoot) {
        int i_random_enemy = choose_shooter();
        shoot(&game.enemies[i_random_enemy], -1);
    }

    if (game.enemy_step_acc > 0.5) {
        game.enemies_footsteps += 1;
        game.enemy_step_acc = 0;
    }

    if (game.enemies_footsteps >= ENEMY_MAX_FOOTSTEPS) {
        game.enemies_footsteps = 0;
        game.enemies_direction = -game.enemies_direction;
    }

    usleep(16000);
}

void clear() {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            game.map[y][x] = ' ';
        }
    }
}

void draw() {
    printf("\033[0;0f");
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            printf("%c", game.map[y][x]);
        }
        printf("\n");
    }

    printf("SCORE: %d\t", game.score);
    printf("LIFES: ", player.lifes);
    for (int i = 1; i <= PLAYER_MAX_LIFES; i++) {
        if (player.lifes >= i) {
            printf("♡ ");
        } else {
            printf("x ");
        }
    }

    printf("\n");
}

int main(void) {
    enable_raw_mode();
    game_init();
    while (!should_stop()) {
        update();
        draw();
        clear();
    }

    return 0;
}