/*
 * Jump Prince LVGL port.
 *
 * Original Jump Prince by Jakub Tomsu, copyright (c) 2013-2024.
 * This header describes a modified interface for the factory_demo LVGL UI.
 * See THIRD_PARTY_NOTICES.md in the example root for the license notice.
 */

#ifndef UI_JUMP_PRINCE_GAME_H
#define UI_JUMP_PRINCE_GAME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#define JUMP_PRINCE_MAP_W 16
#define JUMP_PRINCE_MAP_H 12

typedef enum {
    JUMP_PRINCE_STOPPED,
    JUMP_PRINCE_RUNNING,
    JUMP_PRINCE_PAUSED,
} jump_prince_state_t;

typedef struct {
    float x;
    float y;
} jump_prince_vec2_t;

typedef struct {
    jump_prince_vec2_t position;
    jump_prince_vec2_t velocity;
    float jump_hold_time;
    float anim_time;
    bool is_on_ground;
    bool is_facing_right;
    bool input_left;
    bool input_right;
    bool input_jump;
    bool prev_input_jump;
    int screen_index;
    float screen_offset_y;
    jump_prince_state_t state;
} jump_prince_game_t;

void jump_prince_game_init(jump_prince_game_t *game);
void jump_prince_game_start(jump_prince_game_t *game);
void jump_prince_game_restart(jump_prince_game_t *game);
void jump_prince_game_pause(jump_prince_game_t *game);
void jump_prince_game_resume(jump_prince_game_t *game);
void jump_prince_game_end(jump_prince_game_t *game);
void jump_prince_game_set_input(jump_prince_game_t *game, bool left, bool right, bool jump);
void jump_prince_game_update(jump_prince_game_t *game, uint32_t elapsed_ms);

bool jump_prince_game_tile_full(const jump_prince_game_t *game, uint8_t x, uint8_t y);
bool jump_prince_game_tile_full_outside(const jump_prince_game_t *game, int x, int y);
void jump_prince_game_get_tile_sprite(const jump_prince_game_t *game,
                                      uint8_t x,
                                      uint8_t y,
                                      uint8_t *sprite_x,
                                      uint8_t *sprite_y);
uint8_t jump_prince_game_get_player_sprite(const jump_prince_game_t *game);
float jump_prince_game_player_screen_x(const jump_prince_game_t *game);
float jump_prince_game_player_screen_y(const jump_prince_game_t *game);
float jump_prince_game_jump_charge(const jump_prince_game_t *game);
int jump_prince_game_get_screen_index(const jump_prince_game_t *game);
jump_prince_state_t jump_prince_game_get_state(const jump_prince_game_t *game);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
