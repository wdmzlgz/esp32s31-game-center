#ifndef UI_SNAKE_GAME_H
#define UI_SNAKE_GAME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#define SNAKE_GAME_BOARD_SIZE 15
#define SNAKE_GAME_MAX_CELLS (SNAKE_GAME_BOARD_SIZE * SNAKE_GAME_BOARD_SIZE)

typedef enum {
    SNAKE_GAME_DIR_UP,
    SNAKE_GAME_DIR_RIGHT,
    SNAKE_GAME_DIR_DOWN,
    SNAKE_GAME_DIR_LEFT,
} snake_game_direction_t;

typedef enum {
    SNAKE_GAME_STOPPED,
    SNAKE_GAME_RUNNING,
    SNAKE_GAME_PAUSED,
    SNAKE_GAME_OVER,
} snake_game_state_t;

typedef struct {
    int8_t x;
    int8_t y;
} snake_game_cell_t;

typedef void (*snake_game_draw_cell_cb_t)(int8_t x, int8_t y, void *user_data);
typedef void (*snake_game_clear_cb_t)(void *user_data);
typedef void (*snake_game_present_cb_t)(void *user_data);
typedef void (*snake_game_info_cb_t)(uint16_t score, uint16_t length, uint32_t elapsed_s, void *user_data);

typedef struct {
    snake_game_cell_t body[SNAKE_GAME_MAX_CELLS];
    snake_game_cell_t food;
    uint16_t length;
    uint16_t score;
    uint32_t elapsed_ms;
    uint32_t step_accum_ms;
    uint32_t speed_ms;
    uint32_t random_seed;
    snake_game_direction_t direction;
    snake_game_direction_t pending_direction;
    snake_game_state_t state;
    snake_game_clear_cb_t clear_cb;
    snake_game_draw_cell_cb_t draw_head_cb;
    snake_game_draw_cell_cb_t draw_body_cb;
    snake_game_draw_cell_cb_t draw_food_cb;
    snake_game_present_cb_t present_cb;
    snake_game_info_cb_t info_cb;
    void *user_data;
} snake_game_t;

void snake_game_init(snake_game_t *game);
void snake_game_set_draw_callbacks(snake_game_t *game,
                                   snake_game_clear_cb_t clear_cb,
                                   snake_game_draw_cell_cb_t draw_head_cb,
                                   snake_game_draw_cell_cb_t draw_body_cb,
                                   snake_game_draw_cell_cb_t draw_food_cb,
                                   snake_game_present_cb_t present_cb,
                                   void *user_data);
void snake_game_set_info_callback(snake_game_t *game, snake_game_info_cb_t info_cb);
void snake_game_start(snake_game_t *game);
void snake_game_pause(snake_game_t *game);
void snake_game_resume(snake_game_t *game);
void snake_game_end(snake_game_t *game);
void snake_game_restart(snake_game_t *game);
void snake_game_update(snake_game_t *game, uint32_t elapsed_ms);
void snake_game_update_info(snake_game_t *game);
void snake_game_draw(const snake_game_t *game);
void snake_game_set_speed(snake_game_t *game, uint32_t speed_ms);
void snake_game_set_direction(snake_game_t *game, snake_game_direction_t direction);

uint16_t snake_game_get_score(const snake_game_t *game);
uint16_t snake_game_get_length(const snake_game_t *game);
uint32_t snake_game_get_elapsed_s(const snake_game_t *game);
snake_game_state_t snake_game_get_state(const snake_game_t *game);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
