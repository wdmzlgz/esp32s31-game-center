#ifndef UI_NONOGRAM_GAME_H
#define UI_NONOGRAM_GAME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#define NONOGRAM_GAME_MAX_SIZE 6
#define NONOGRAM_GAME_MAX_CLUES 3
#define NONOGRAM_GAME_MAX_ROUNDS 5

typedef enum {
    NONOGRAM_DIFFICULTY_EASY,
    NONOGRAM_DIFFICULTY_NORMAL,
    NONOGRAM_DIFFICULTY_HARD,
} nonogram_difficulty_t;

typedef enum {
    NONOGRAM_GAME_STOPPED,
    NONOGRAM_GAME_RUNNING,
    NONOGRAM_GAME_SOLVED,
    NONOGRAM_GAME_PAUSED,
    NONOGRAM_GAME_FINISHED,
} nonogram_game_state_t;

typedef void (*nonogram_game_clear_cb_t)(uint8_t size, void *user_data);
typedef void (*nonogram_game_cell_cb_t)(uint8_t x, uint8_t y, bool selected, void *user_data);
typedef void (*nonogram_game_hint_cb_t)(bool row, uint8_t index, const uint8_t *clues, uint8_t count, void *user_data);
typedef void (*nonogram_game_present_cb_t)(void *user_data);
typedef void (*nonogram_game_info_cb_t)(nonogram_difficulty_t difficulty,
                                        uint8_t round,
                                        uint32_t elapsed_s,
                                        nonogram_game_state_t state,
                                        void *user_data);
typedef uint32_t (*nonogram_game_random_seed_cb_t)(void *user_data);

typedef struct {
    nonogram_difficulty_t difficulty;
    nonogram_game_state_t state;
    nonogram_game_state_t state_before_pause;
    uint8_t size;
    uint8_t round;
    uint8_t used_indices[NONOGRAM_GAME_MAX_ROUNDS];
    uint8_t used_count;
    int8_t current_index;
    uint32_t elapsed_ms;
    uint32_t solve_delay_ms;
    uint32_t random_seed;
    uint64_t answer_mask;
    uint64_t selected_mask;
    nonogram_game_clear_cb_t clear_cb;
    nonogram_game_cell_cb_t draw_cell_cb;
    nonogram_game_hint_cb_t draw_hint_cb;
    nonogram_game_present_cb_t present_cb;
    nonogram_game_info_cb_t info_cb;
    nonogram_game_random_seed_cb_t random_seed_cb;
    void *user_data;
    void *random_user_data;
} nonogram_game_t;

void nonogram_game_init(nonogram_game_t *game);
void nonogram_game_set_draw_callbacks(nonogram_game_t *game,
                                      nonogram_game_clear_cb_t clear_cb,
                                      nonogram_game_cell_cb_t draw_cell_cb,
                                      nonogram_game_hint_cb_t draw_hint_cb,
                                      nonogram_game_present_cb_t present_cb,
                                      void *user_data);
void nonogram_game_set_info_callback(nonogram_game_t *game, nonogram_game_info_cb_t info_cb);
void nonogram_game_set_random_seed_callback(nonogram_game_t *game,
                                            nonogram_game_random_seed_cb_t random_seed_cb,
                                            void *user_data);
void nonogram_game_start(nonogram_game_t *game);
void nonogram_game_restart(nonogram_game_t *game);
void nonogram_game_pause(nonogram_game_t *game);
void nonogram_game_resume(nonogram_game_t *game);
void nonogram_game_end(nonogram_game_t *game);
void nonogram_game_update(nonogram_game_t *game, uint32_t elapsed_ms);
void nonogram_game_set_difficulty(nonogram_game_t *game, nonogram_difficulty_t difficulty);
bool nonogram_game_select_cell(nonogram_game_t *game, uint8_t x, uint8_t y);
void nonogram_game_draw(const nonogram_game_t *game);
void nonogram_game_update_info(nonogram_game_t *game);

uint8_t nonogram_game_get_size(const nonogram_game_t *game);
uint8_t nonogram_game_get_round(const nonogram_game_t *game);
uint32_t nonogram_game_get_elapsed_s(const nonogram_game_t *game);
nonogram_game_state_t nonogram_game_get_state(const nonogram_game_t *game);
nonogram_difficulty_t nonogram_game_get_difficulty(const nonogram_game_t *game);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
