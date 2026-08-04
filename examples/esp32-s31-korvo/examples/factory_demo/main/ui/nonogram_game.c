#include "nonogram_game.h"

#include "cJSON.h"

#include <stddef.h>

#define NONOGRAM_NEXT_ROUND_DELAY_MS 3000

typedef struct {
    uint8_t size;
    uint64_t answer_mask;
} nonogram_puzzle_t;

typedef struct {
    uint8_t size;
    uint8_t count;
    const nonogram_puzzle_t *puzzles;
} nonogram_puzzle_set_t;

/* Generated from nonogram_puzzles.json. */
static const nonogram_puzzle_t s_easy_puzzles[] = {
    {4, 0xD2C3ULL},
    {4, 0x7067ULL},
    {4, 0x9D96ULL},
    {4, 0x6CAAULL},
    {4, 0xBFF4ULL},
    {4, 0xF2D3ULL},
    {4, 0xD377ULL},
    {4, 0x09CFULL},
    {4, 0x0A22ULL},
    {4, 0x72E5ULL},
};

static const nonogram_puzzle_t s_normal_puzzles[] = {
    {5, 0x00D36CC4ULL},
    {5, 0x00037F24ULL},
    {5, 0x017D9389ULL},
    {5, 0x015D4E57ULL},
    {5, 0x0044307AULL},
    {5, 0x01B52779ULL},
    {5, 0x0056BD58ULL},
    {5, 0x01218E18ULL},
    {5, 0x007F5549ULL},
    {5, 0x00B00B63ULL},
};

static const nonogram_puzzle_t s_hard_puzzles[] = {
    {6, 0x03EFBE7DE0ULL},
    {6, 0x0D00549F46ULL},
    {6, 0x0E5F45DEF3ULL},
    {6, 0x04F62CABDDULL},
    {6, 0x0DE61DD837ULL},
    {6, 0x0649B44F37ULL},
    {6, 0x04106859E7ULL},
    {6, 0x0BE01EC6E4ULL},
    {6, 0x003E198EC0ULL},
    {6, 0x0514E5EBB9ULL},
};

static const nonogram_puzzle_set_t s_fallback_puzzle_sets[] = {
    {4, (uint8_t)(sizeof(s_easy_puzzles) / sizeof(s_easy_puzzles[0])), s_easy_puzzles},
    {5, (uint8_t)(sizeof(s_normal_puzzles) / sizeof(s_normal_puzzles[0])), s_normal_puzzles},
    {6, (uint8_t)(sizeof(s_hard_puzzles) / sizeof(s_hard_puzzles[0])), s_hard_puzzles},
};

extern const uint8_t nonogram_puzzles_json_start[] asm("_binary_nonogram_puzzles_json_start");
extern const uint8_t nonogram_puzzles_json_end[] asm("_binary_nonogram_puzzles_json_end");

static nonogram_puzzle_t s_loaded_easy_puzzles[16];
static nonogram_puzzle_t s_loaded_normal_puzzles[16];
static nonogram_puzzle_t s_loaded_hard_puzzles[16];
static nonogram_puzzle_set_t s_loaded_puzzle_sets[] = {
    {4, 0, s_loaded_easy_puzzles},
    {5, 0, s_loaded_normal_puzzles},
    {6, 0, s_loaded_hard_puzzles},
};
static bool s_puzzles_loaded;

static bool nonogram_json_answer_to_mask(const cJSON *answer, uint8_t size, uint64_t *mask)
{
    if(!cJSON_IsArray(answer) || cJSON_GetArraySize(answer) != size || !mask) {
        return false;
    }

    uint64_t parsed_mask = 0;
    for(uint8_t y = 0; y < size; y++) {
        const cJSON *row = cJSON_GetArrayItem(answer, y);
        if(!cJSON_IsArray(row) || cJSON_GetArraySize(row) != size) {
            return false;
        }

        for(uint8_t x = 0; x < size; x++) {
            const cJSON *cell = cJSON_GetArrayItem(row, x);
            if(!cJSON_IsNumber(cell)) {
                return false;
            }
            if(cell->valueint != 0) {
                parsed_mask |= 1ULL << ((uint8_t)(y * size + x));
            }
        }
    }

    *mask = parsed_mask;
    return true;
}

static void nonogram_load_json_section(const cJSON *root,
                                       const char *key,
                                       uint8_t size,
                                       nonogram_puzzle_t *puzzles,
                                       uint8_t max_count,
                                       nonogram_puzzle_set_t *set)
{
    const cJSON *section = cJSON_GetObjectItemCaseSensitive(root, key);
    if(!cJSON_IsArray(section)) {
        return;
    }

    set->count = 0;
    const cJSON *puzzle = NULL;
    cJSON_ArrayForEach(puzzle, section) {
        if(set->count >= max_count) {
            break;
        }

        const cJSON *answer = cJSON_GetObjectItemCaseSensitive(puzzle, "answer");
        uint64_t mask = 0;
        if(nonogram_json_answer_to_mask(answer, size, &mask)) {
            puzzles[set->count] = (nonogram_puzzle_t) {
                .size = size,
                .answer_mask = mask,
            };
            set->count++;
        }
    }
}

static void nonogram_ensure_puzzles_loaded(void)
{
    if(s_puzzles_loaded) {
        return;
    }

    const char *json = (const char *)nonogram_puzzles_json_start;
    const char *json_end = (const char *)nonogram_puzzles_json_end;
    if(json && json_end && json_end > json) {
        cJSON *root = cJSON_ParseWithLength(json, (size_t)(json_end - json));
        if(root) {
            nonogram_load_json_section(root, "4x4", 4, s_loaded_easy_puzzles,
                                       (uint8_t)(sizeof(s_loaded_easy_puzzles) / sizeof(s_loaded_easy_puzzles[0])),
                                       &s_loaded_puzzle_sets[0]);
            nonogram_load_json_section(root, "5x5", 5, s_loaded_normal_puzzles,
                                       (uint8_t)(sizeof(s_loaded_normal_puzzles) / sizeof(s_loaded_normal_puzzles[0])),
                                       &s_loaded_puzzle_sets[1]);
            nonogram_load_json_section(root, "6x6", 6, s_loaded_hard_puzzles,
                                       (uint8_t)(sizeof(s_loaded_hard_puzzles) / sizeof(s_loaded_hard_puzzles[0])),
                                       &s_loaded_puzzle_sets[2]);
            cJSON_Delete(root);
        }
    }

    s_puzzles_loaded = true;
}

static const nonogram_puzzle_set_t *nonogram_choose_set(nonogram_difficulty_t difficulty,
                                                        const nonogram_puzzle_set_t *sets)
{
    if(difficulty == NONOGRAM_DIFFICULTY_EASY) {
        return &sets[0];
    }
    if(difficulty == NONOGRAM_DIFFICULTY_HARD) {
        return &sets[2];
    }
    return &sets[1];
}

static const nonogram_puzzle_set_t *nonogram_get_puzzle_set(nonogram_difficulty_t difficulty)
{
    nonogram_ensure_puzzles_loaded();

    const nonogram_puzzle_set_t *loaded_set = nonogram_choose_set(difficulty, s_loaded_puzzle_sets);
    if(loaded_set->count > 0) {
        return loaded_set;
    }

    return nonogram_choose_set(difficulty, s_fallback_puzzle_sets);
}

static uint32_t nonogram_next_random(nonogram_game_t *game)
{
    uint32_t time_seed = game->random_seed_cb ? game->random_seed_cb(game->random_user_data) : game->elapsed_ms;
    game->random_seed ^= time_seed + 0x9E3779B9UL + (game->random_seed << 6) + (game->random_seed >> 2);
    game->random_seed = game->random_seed * 1664525UL + 1013904223UL;
    return game->random_seed;
}

static bool nonogram_was_used(const nonogram_game_t *game, uint8_t index)
{
    for(uint8_t i = 0; i < game->used_count; i++) {
        if(game->used_indices[i] == index) {
            return true;
        }
    }
    return false;
}

static bool nonogram_mask_has_cell(uint64_t mask, uint8_t size, uint8_t x, uint8_t y)
{
    return (mask & (1ULL << ((uint8_t)(y * size + x)))) != 0;
}

static void nonogram_select_new_puzzle(nonogram_game_t *game)
{
    const nonogram_puzzle_set_t *set = nonogram_get_puzzle_set(game->difficulty);
    uint8_t index = 0;

    if(game->used_count >= set->count) {
        game->used_count = 0;
    }

    for(uint8_t attempt = 0; attempt < set->count * 2; attempt++) {
        index = (uint8_t)(nonogram_next_random(game) % set->count);
        if(!nonogram_was_used(game, index)) {
            break;
        }
    }

    if(nonogram_was_used(game, index)) {
        for(uint8_t i = 0; i < set->count; i++) {
            if(!nonogram_was_used(game, i)) {
                index = i;
                break;
            }
        }
    }

    if(game->used_count < NONOGRAM_GAME_MAX_ROUNDS) {
        game->used_indices[game->used_count++] = index;
    }

    game->current_index = (int8_t)index;
    game->size = set->puzzles[index].size;
    game->answer_mask = set->puzzles[index].answer_mask;
    game->selected_mask = 0;
}

static void nonogram_emit_line_hints(const nonogram_game_t *game, bool row, uint8_t index)
{
    uint8_t clues[NONOGRAM_GAME_MAX_CLUES] = {0};
    uint8_t count = 0;
    uint8_t run = 0;

    for(uint8_t pos = 0; pos < game->size; pos++) {
        bool filled = row ? nonogram_mask_has_cell(game->answer_mask, game->size, pos, index) :
                            nonogram_mask_has_cell(game->answer_mask, game->size, index, pos);
        if(filled) {
            run++;
        } else if(run > 0) {
            if(count < NONOGRAM_GAME_MAX_CLUES) {
                clues[count++] = run;
            }
            run = 0;
        }
    }

    if(run > 0 && count < NONOGRAM_GAME_MAX_CLUES) {
        clues[count++] = run;
    }

    if(game->draw_hint_cb) {
        game->draw_hint_cb(row, index, clues, count, game->user_data);
    }
}

static void nonogram_prepare_next_round(nonogram_game_t *game)
{
    if(game->round >= NONOGRAM_GAME_MAX_ROUNDS) {
        game->state = NONOGRAM_GAME_FINISHED;
        nonogram_game_update_info(game);
        return;
    }

    game->round++;
    game->elapsed_ms = 0;
    game->solve_delay_ms = 0;
    game->state = NONOGRAM_GAME_RUNNING;
    nonogram_select_new_puzzle(game);
    nonogram_game_draw(game);
    nonogram_game_update_info(game);
}

static void nonogram_mark_solved(nonogram_game_t *game)
{
    if(game->round >= NONOGRAM_GAME_MAX_ROUNDS) {
        game->state = NONOGRAM_GAME_FINISHED;
    } else {
        game->state = NONOGRAM_GAME_SOLVED;
        game->solve_delay_ms = 0;
    }
    nonogram_game_update_info(game);
}

void nonogram_game_init(nonogram_game_t *game)
{
    if(!game) {
        return;
    }

    *game = (nonogram_game_t) {
        .difficulty = NONOGRAM_DIFFICULTY_NORMAL,
        .state = NONOGRAM_GAME_STOPPED,
        .state_before_pause = NONOGRAM_GAME_STOPPED,
        .size = 5,
        .round = 1,
        .current_index = -1,
        .random_seed = 0x9E3779B9UL,
    };
}

void nonogram_game_set_draw_callbacks(nonogram_game_t *game,
                                      nonogram_game_clear_cb_t clear_cb,
                                      nonogram_game_cell_cb_t draw_cell_cb,
                                      nonogram_game_hint_cb_t draw_hint_cb,
                                      nonogram_game_present_cb_t present_cb,
                                      void *user_data)
{
    if(!game) {
        return;
    }

    game->clear_cb = clear_cb;
    game->draw_cell_cb = draw_cell_cb;
    game->draw_hint_cb = draw_hint_cb;
    game->present_cb = present_cb;
    game->user_data = user_data;
}

void nonogram_game_set_info_callback(nonogram_game_t *game, nonogram_game_info_cb_t info_cb)
{
    if(game) {
        game->info_cb = info_cb;
    }
}

void nonogram_game_set_random_seed_callback(nonogram_game_t *game,
                                            nonogram_game_random_seed_cb_t random_seed_cb,
                                            void *user_data)
{
    if(!game) {
        return;
    }

    game->random_seed_cb = random_seed_cb;
    game->random_user_data = user_data;
}

void nonogram_game_start(nonogram_game_t *game)
{
    if(!game) {
        return;
    }

    game->state = NONOGRAM_GAME_RUNNING;
    game->state_before_pause = NONOGRAM_GAME_STOPPED;
    game->round = 1;
    game->used_count = 0;
    game->elapsed_ms = 0;
    game->solve_delay_ms = 0;
    nonogram_select_new_puzzle(game);
    nonogram_game_draw(game);
    nonogram_game_update_info(game);
}

void nonogram_game_restart(nonogram_game_t *game)
{
    nonogram_game_start(game);
}

void nonogram_game_pause(nonogram_game_t *game)
{
    if(game && (game->state == NONOGRAM_GAME_RUNNING || game->state == NONOGRAM_GAME_SOLVED)) {
        game->state_before_pause = game->state;
        game->state = NONOGRAM_GAME_PAUSED;
        nonogram_game_update_info(game);
    }
}

void nonogram_game_resume(nonogram_game_t *game)
{
    if(game && game->state == NONOGRAM_GAME_PAUSED) {
        game->state = game->state_before_pause == NONOGRAM_GAME_SOLVED ? NONOGRAM_GAME_SOLVED : NONOGRAM_GAME_RUNNING;
        nonogram_game_update_info(game);
    }
}

void nonogram_game_end(nonogram_game_t *game)
{
    if(game) {
        game->state = NONOGRAM_GAME_STOPPED;
        game->state_before_pause = NONOGRAM_GAME_STOPPED;
        nonogram_game_update_info(game);
    }
}

void nonogram_game_update(nonogram_game_t *game, uint32_t elapsed_ms)
{
    if(!game) {
        return;
    }

    if(game->state == NONOGRAM_GAME_SOLVED) {
        game->solve_delay_ms += elapsed_ms;
        if(game->solve_delay_ms >= NONOGRAM_NEXT_ROUND_DELAY_MS) {
            nonogram_prepare_next_round(game);
        }
        return;
    }

    if(game->state != NONOGRAM_GAME_RUNNING) {
        return;
    }

    uint32_t old_s = game->elapsed_ms / 1000U;
    game->elapsed_ms += elapsed_ms;
    if(old_s != game->elapsed_ms / 1000U) {
        nonogram_game_update_info(game);
    }
}

void nonogram_game_set_difficulty(nonogram_game_t *game, nonogram_difficulty_t difficulty)
{
    if(!game) {
        return;
    }

    game->difficulty = difficulty;
    nonogram_game_start(game);
}

bool nonogram_game_select_cell(nonogram_game_t *game, uint8_t x, uint8_t y)
{
    if(!game || game->state != NONOGRAM_GAME_RUNNING || x >= game->size || y >= game->size) {
        return false;
    }

    uint64_t bit = 1ULL << ((uint8_t)(y * game->size + x));
    game->selected_mask ^= bit;

    if(game->draw_cell_cb) {
        game->draw_cell_cb(x, y, (game->selected_mask & bit) != 0, game->user_data);
    }
    if(game->present_cb) {
        game->present_cb(game->user_data);
    }

    if(game->selected_mask == game->answer_mask) {
        nonogram_mark_solved(game);
        return true;
    }

    return false;
}

void nonogram_game_draw(const nonogram_game_t *game)
{
    if(!game) {
        return;
    }

    if(game->clear_cb) {
        game->clear_cb(game->size, game->user_data);
    }

    for(uint8_t i = 0; i < game->size; i++) {
        nonogram_emit_line_hints(game, true, i);
        nonogram_emit_line_hints(game, false, i);
    }

    if(game->draw_cell_cb) {
        for(uint8_t y = 0; y < game->size; y++) {
            for(uint8_t x = 0; x < game->size; x++) {
                bool selected = nonogram_mask_has_cell(game->selected_mask, game->size, x, y);
                game->draw_cell_cb(x, y, selected, game->user_data);
            }
        }
    }

    if(game->present_cb) {
        game->present_cb(game->user_data);
    }
}

void nonogram_game_update_info(nonogram_game_t *game)
{
    if(game && game->info_cb) {
        game->info_cb(game->difficulty, game->round, game->elapsed_ms / 1000U, game->state, game->user_data);
    }
}

uint8_t nonogram_game_get_size(const nonogram_game_t *game)
{
    return game ? game->size : 0;
}

uint8_t nonogram_game_get_round(const nonogram_game_t *game)
{
    return game ? game->round : 0;
}

uint32_t nonogram_game_get_elapsed_s(const nonogram_game_t *game)
{
    return game ? game->elapsed_ms / 1000U : 0;
}

nonogram_game_state_t nonogram_game_get_state(const nonogram_game_t *game)
{
    return game ? game->state : NONOGRAM_GAME_STOPPED;
}

nonogram_difficulty_t nonogram_game_get_difficulty(const nonogram_game_t *game)
{
    return game ? game->difficulty : NONOGRAM_DIFFICULTY_NORMAL;
}
