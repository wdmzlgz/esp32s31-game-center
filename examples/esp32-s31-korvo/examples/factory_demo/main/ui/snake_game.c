#include "snake_game.h"

#include <stddef.h>

static bool cells_equal(snake_game_cell_t a, snake_game_cell_t b)
{
    return a.x == b.x && a.y == b.y;
}

static bool direction_is_opposite(snake_game_direction_t a, snake_game_direction_t b)
{
    return (a == SNAKE_GAME_DIR_UP && b == SNAKE_GAME_DIR_DOWN) ||
           (a == SNAKE_GAME_DIR_DOWN && b == SNAKE_GAME_DIR_UP) ||
           (a == SNAKE_GAME_DIR_LEFT && b == SNAKE_GAME_DIR_RIGHT) ||
           (a == SNAKE_GAME_DIR_RIGHT && b == SNAKE_GAME_DIR_LEFT);
}

static uint32_t next_random(snake_game_t *game)
{
    game->random_seed = game->random_seed * 1664525U + 1013904223U;
    return game->random_seed;
}

static bool snake_contains(const snake_game_t *game, snake_game_cell_t cell, uint16_t limit)
{
    for(uint16_t i = 0; i < limit; i++) {
        if(cells_equal(game->body[i], cell)) {
            return true;
        }
    }
    return false;
}

static void generate_food(snake_game_t *game)
{
    uint16_t free_cells = SNAKE_GAME_MAX_CELLS - game->length;
    if(free_cells == 0) {
        game->food.x = -1;
        game->food.y = -1;
        return;
    }

    uint16_t target = next_random(game) % free_cells;
    uint16_t seen = 0;
    for(int8_t y = 0; y < SNAKE_GAME_BOARD_SIZE; y++) {
        for(int8_t x = 0; x < SNAKE_GAME_BOARD_SIZE; x++) {
            snake_game_cell_t cell = {.x = x, .y = y};
            if(snake_contains(game, cell, game->length)) {
                continue;
            }
            if(seen == target) {
                game->food = cell;
                return;
            }
            seen++;
        }
    }
}

static void reset_game(snake_game_t *game)
{
    uint32_t seed = game->random_seed ? game->random_seed : 0xA341316CU;

    game->length = 3;
    game->score = 0;
    game->elapsed_ms = 0;
    game->step_accum_ms = 0;
    game->speed_ms = game->speed_ms ? game->speed_ms : 500;
    game->random_seed = seed;
    game->direction = SNAKE_GAME_DIR_RIGHT;
    game->pending_direction = SNAKE_GAME_DIR_RIGHT;
    game->state = SNAKE_GAME_STOPPED;

    game->body[0] = (snake_game_cell_t) {.x = 7, .y = 7};
    game->body[1] = (snake_game_cell_t) {.x = 6, .y = 7};
    game->body[2] = (snake_game_cell_t) {.x = 5, .y = 7};
    generate_food(game);
}

static snake_game_cell_t next_head_for_direction(snake_game_cell_t head, snake_game_direction_t direction)
{
    switch(direction) {
    case SNAKE_GAME_DIR_UP:
        head.y--;
        break;
    case SNAKE_GAME_DIR_RIGHT:
        head.x++;
        break;
    case SNAKE_GAME_DIR_DOWN:
        head.y++;
        break;
    case SNAKE_GAME_DIR_LEFT:
        head.x--;
        break;
    }
    return head;
}

static void move_snake(snake_game_t *game)
{
    game->direction = game->pending_direction;
    snake_game_cell_t next_head = next_head_for_direction(game->body[0], game->direction);

    if(next_head.x < 0 || next_head.x >= SNAKE_GAME_BOARD_SIZE ||
       next_head.y < 0 || next_head.y >= SNAKE_GAME_BOARD_SIZE) {
        game->state = SNAKE_GAME_OVER;
        return;
    }

    bool grows = cells_equal(next_head, game->food);
    uint16_t collision_limit = grows ? game->length : game->length - 1;
    if(snake_contains(game, next_head, collision_limit)) {
        game->state = SNAKE_GAME_OVER;
        return;
    }

    uint16_t target_length = game->length + (grows ? 1 : 0);
    if(target_length > SNAKE_GAME_MAX_CELLS) {
        target_length = SNAKE_GAME_MAX_CELLS;
    }

    for(int16_t i = target_length - 1; i > 0; i--) {
        game->body[i] = game->body[i - 1];
    }
    game->body[0] = next_head;
    game->length = target_length;

    if(grows) {
        game->score++;
        generate_food(game);
    }
}

void snake_game_init(snake_game_t *game)
{
    if(!game) {
        return;
    }

    snake_game_clear_cb_t clear_cb = game->clear_cb;
    snake_game_draw_cell_cb_t draw_head_cb = game->draw_head_cb;
    snake_game_draw_cell_cb_t draw_body_cb = game->draw_body_cb;
    snake_game_draw_cell_cb_t draw_food_cb = game->draw_food_cb;
    snake_game_present_cb_t present_cb = game->present_cb;
    snake_game_info_cb_t info_cb = game->info_cb;
    void *user_data = game->user_data;

    reset_game(game);
    game->clear_cb = clear_cb;
    game->draw_head_cb = draw_head_cb;
    game->draw_body_cb = draw_body_cb;
    game->draw_food_cb = draw_food_cb;
    game->present_cb = present_cb;
    game->info_cb = info_cb;
    game->user_data = user_data;
}

void snake_game_set_draw_callbacks(snake_game_t *game,
                                   snake_game_clear_cb_t clear_cb,
                                   snake_game_draw_cell_cb_t draw_head_cb,
                                   snake_game_draw_cell_cb_t draw_body_cb,
                                   snake_game_draw_cell_cb_t draw_food_cb,
                                   snake_game_present_cb_t present_cb,
                                   void *user_data)
{
    if(!game) {
        return;
    }
    game->clear_cb = clear_cb;
    game->draw_head_cb = draw_head_cb;
    game->draw_body_cb = draw_body_cb;
    game->draw_food_cb = draw_food_cb;
    game->present_cb = present_cb;
    game->user_data = user_data;
}

void snake_game_set_info_callback(snake_game_t *game, snake_game_info_cb_t info_cb)
{
    if(game) {
        game->info_cb = info_cb;
    }
}

void snake_game_start(snake_game_t *game)
{
    if(!game) {
        return;
    }
    reset_game(game);
    game->state = SNAKE_GAME_RUNNING;
    snake_game_update_info(game);
    snake_game_draw(game);
}

void snake_game_pause(snake_game_t *game)
{
    if(game && game->state == SNAKE_GAME_RUNNING) {
        game->state = SNAKE_GAME_PAUSED;
    }
}

void snake_game_resume(snake_game_t *game)
{
    if(game && game->state == SNAKE_GAME_PAUSED) {
        game->state = SNAKE_GAME_RUNNING;
    }
}

void snake_game_end(snake_game_t *game)
{
    if(game) {
        game->state = SNAKE_GAME_STOPPED;
    }
}

void snake_game_restart(snake_game_t *game)
{
    snake_game_start(game);
}

void snake_game_update(snake_game_t *game, uint32_t elapsed_ms)
{
    if(!game || game->state != SNAKE_GAME_RUNNING) {
        return;
    }

    game->elapsed_ms += elapsed_ms;
    game->step_accum_ms += elapsed_ms;

    bool moved = false;
    while(game->step_accum_ms >= game->speed_ms && game->state == SNAKE_GAME_RUNNING) {
        game->step_accum_ms -= game->speed_ms;
        move_snake(game);
        moved = true;
    }

    snake_game_update_info(game);
    if(moved) {
        snake_game_draw(game);
    }
}

void snake_game_update_info(snake_game_t *game)
{
    if(game && game->info_cb) {
        game->info_cb(game->score, game->length, game->elapsed_ms / 1000U, game->user_data);
    }
}

void snake_game_draw(const snake_game_t *game)
{
    if(!game) {
        return;
    }
    if(game->clear_cb) {
        game->clear_cb(game->user_data);
    }
    if(game->draw_food_cb && game->food.x >= 0 && game->food.y >= 0) {
        game->draw_food_cb(game->food.x, game->food.y, game->user_data);
    }
    for(int16_t i = game->length - 1; i >= 0; i--) {
        if(i == 0) {
            if(game->draw_head_cb) {
                game->draw_head_cb(game->body[i].x, game->body[i].y, game->user_data);
            }
        } else if(game->draw_body_cb) {
            game->draw_body_cb(game->body[i].x, game->body[i].y, game->user_data);
        }
    }
    if(game->present_cb) {
        game->present_cb(game->user_data);
    }
}

void snake_game_set_speed(snake_game_t *game, uint32_t speed_ms)
{
    if(game && speed_ms > 0) {
        game->speed_ms = speed_ms;
    }
}

void snake_game_set_direction(snake_game_t *game, snake_game_direction_t direction)
{
    if(!game || game->state != SNAKE_GAME_RUNNING) {
        return;
    }
    if(!direction_is_opposite(game->direction, direction)) {
        game->pending_direction = direction;
    }
}

uint16_t snake_game_get_score(const snake_game_t *game)
{
    return game ? game->score : 0;
}

uint16_t snake_game_get_length(const snake_game_t *game)
{
    return game ? game->length : 0;
}

uint32_t snake_game_get_elapsed_s(const snake_game_t *game)
{
    return game ? game->elapsed_ms / 1000U : 0;
}

snake_game_state_t snake_game_get_state(const snake_game_t *game)
{
    return game ? game->state : SNAKE_GAME_STOPPED;
}
