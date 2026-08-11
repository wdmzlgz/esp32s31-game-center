/*
 * Jump Prince LVGL port.
 *
 * Original Jump Prince by Jakub Tomsu, copyright (c) 2013-2024.
 * This file contains modified gameplay logic for the factory_demo LVGL UI.
 * See THIRD_PARTY_NOTICES.md in the example root for the license notice.
 */

#include "jump_prince_game.h"

#include <math.h>

#define JP_SCREEN_COUNT 6
#define JP_PLAYER_HALF_W 0.3f
#define JP_PLAYER_HALF_H 0.4f
#define JP_GRAVITY 30.0f
#define JP_SPEED 200.0f
#define JP_JUMP_STRENGTH 15.0f
#define JP_BOUNCE_FACTOR_X 0.45f

typedef char jump_prince_tilemap_t[JUMP_PRINCE_MAP_H][JUMP_PRINCE_MAP_W + 1];

static const jump_prince_tilemap_t s_tilemaps[JP_SCREEN_COUNT] = {
    {
        "",
    },
    {
        "################",
        "#              #",
        "# #### #### #  #",
        "# #    #    #  #",
        "# # ## # ## #  #",
        "# #  # #  #    #",
        "# #### #### #  #",
        "#              #",
        "#              #",
        "#              #",
        "#              #",
        "#########      #",
    },
    {
        "#########      #",
        "#########    ###",
        "########      ##",
        "########      ##",
        "##########     #",
        "##########     #",
        "########      ##",
        "########      ##",
        "##########    ##",
        "######        ##",
        "###           ##",
        "###         ####",
    },
    {
        "###         ####",
        "###    ##   ####",
        "###         ####",
        "###          ###",
        "#####        ###",
        "###          ###",
        "#            ###",
        "##        ######",
        "##         #####",
        "##         #####",
        "######     #####",
        "#####      #####",
    },
    {
        "#####      #####",
        "###      #######",
        "##        ######",
        "##          ####",
        "######      ####",
        "######       ###",
        "######   #   ###",
        "#####    ##  ###",
        "#####        ###",
        "##           ###",
        "##        ######",
        "##    ##########",
    },
    {
        "##    ##########",
        "##            ##",
        "####          ##",
        "########       #",
        "#####          #",
        "##             #",
        "##       #######",
        "#        #######",
        "#         ######",
        "#####     ######",
        "#####     ######",
        "################",
    },
};

static float jp_clamp(float value, float min, float max)
{
    if(value < min) {
        return min;
    }
    if(value > max) {
        return max;
    }
    return value;
}

static jump_prince_vec2_t jp_vec2(float x, float y)
{
    jump_prince_vec2_t v = {x, y};
    return v;
}

static float jp_vec2_len(jump_prince_vec2_t v)
{
    return sqrtf(v.x * v.x + v.y * v.y);
}

static jump_prince_vec2_t jp_vec2_scale(jump_prince_vec2_t v, float scale)
{
    return jp_vec2(v.x * scale, v.y * scale);
}

static jump_prince_vec2_t jp_vec2_normalize(jump_prince_vec2_t v)
{
    float len = jp_vec2_len(v);
    if(len <= 0.0001f) {
        return jp_vec2(0.0f, 0.0f);
    }
    return jp_vec2_scale(v, 1.0f / len);
}

static int jp_floor_to_int(float value)
{
    return (int)floorf(value);
}

static int jp_get_screen_height_index(float height)
{
    return jp_floor_to_int(-height / JUMP_PRINCE_MAP_H);
}

static void jp_update_screen(jump_prince_game_t *game)
{
    int height_index = jp_get_screen_height_index(game->position.y);
    int screen_index = JP_SCREEN_COUNT - height_index - 2;
    if(screen_index < 0 || screen_index >= JP_SCREEN_COUNT) {
        screen_index = 0;
    }

    game->screen_index = screen_index;
    game->screen_offset_y = -(float)(height_index + 1) * JUMP_PRINCE_MAP_H;
}

static char jp_get_tile_from_map(uint8_t map_index, int x, int y, bool full_outside)
{
    if(full_outside) {
        if(x < 0 || x >= JUMP_PRINCE_MAP_W || y < 0 || y >= JUMP_PRINCE_MAP_H) {
            return '#';
        }
    } else {
        if(x < 0 || x >= JUMP_PRINCE_MAP_W) {
            return '#';
        }
        if(y < 0 || y >= JUMP_PRINCE_MAP_H) {
            return ' ';
        }
    }

    if(map_index >= JP_SCREEN_COUNT) {
        return ' ';
    }

    char tile = s_tilemaps[map_index][y][x];
    return tile == '\0' ? ' ' : tile;
}

static bool jp_tile_full_at(const jump_prince_game_t *game, int x, int y)
{
    return jp_get_tile_from_map((uint8_t)game->screen_index, x, y, false) == '#';
}

static void jp_get_tiles_overlapped(int *start_x,
                                    int *start_y,
                                    int *end_x,
                                    int *end_y,
                                    jump_prince_vec2_t center,
                                    jump_prince_vec2_t size)
{
    *start_x = jp_floor_to_int(center.x - size.x);
    *start_y = jp_floor_to_int(center.y - size.y);
    *end_x = jp_floor_to_int(center.x + size.x);
    *end_y = jp_floor_to_int(center.y + size.y);
}

static bool jp_box_colliding_with_tilemap(const jump_prince_game_t *game,
                                          float tilemap_height,
                                          jump_prince_vec2_t center,
                                          jump_prince_vec2_t size)
{
    center.y -= tilemap_height;

    int start_x = 0;
    int start_y = 0;
    int end_x = 0;
    int end_y = 0;
    jp_get_tiles_overlapped(&start_x, &start_y, &end_x, &end_y, center, size);

    for(int x = start_x; x <= end_x; x++) {
        for(int y = start_y; y <= end_y; y++) {
            if(!jp_tile_full_at(game, x, y)) {
                continue;
            }

            jump_prince_vec2_t box_pos = jp_vec2(0.5f + (float)x, 0.5f + (float)y);
            jump_prince_vec2_t size_sum = jp_vec2(size.x + 0.5f, size.y + 0.5f);
            jump_prince_vec2_t surf_dist = jp_vec2(fabsf(center.x - box_pos.x) - size_sum.x,
                                                   fabsf(center.y - box_pos.y) - size_sum.y);
            if(surf_dist.x <= 0.0f && surf_dist.y <= 0.0f) {
                return true;
            }
        }
    }

    return false;
}

static void jp_resolve_box_collision(jump_prince_game_t *game,
                                     float tilemap_height,
                                     jump_prince_vec2_t *center,
                                     jump_prince_vec2_t *velocity,
                                     jump_prince_vec2_t size)
{
    center->y -= tilemap_height;

    int start_x = 0;
    int start_y = 0;
    int end_x = 0;
    int end_y = 0;
    jp_get_tiles_overlapped(&start_x, &start_y, &end_x, &end_y, *center, size);

    for(int x = start_x; x <= end_x; x++) {
        for(int y = start_y; y <= end_y; y++) {
            if(!jp_tile_full_at(game, x, y)) {
                continue;
            }

            jump_prince_vec2_t box_pos = jp_vec2(0.5f + (float)x, 0.5f + (float)y);
            jump_prince_vec2_t size_sum = jp_vec2(size.x + 0.5f, size.y + 0.5f);
            jump_prince_vec2_t surf_dist = jp_vec2(fabsf(center->x - box_pos.x) - size_sum.x,
                                                   fabsf(center->y - box_pos.y) - size_sum.y);
            if(surf_dist.x > 0.0f || surf_dist.y > 0.0f) {
                continue;
            }

            bool is_x_empty = !jp_tile_full_at(game, x + (center->x > box_pos.x ? 1 : -1), y);
            bool is_y_empty = !jp_tile_full_at(game, x, y + (center->y > box_pos.y ? 1 : -1));
            if(!is_x_empty && !is_y_empty) {
                continue;
            }

            bool clip_x = is_x_empty;
            if(is_x_empty && is_y_empty) {
                clip_x = surf_dist.x > surf_dist.y;
            }

            if(clip_x) {
                if(center->x > box_pos.x) {
                    center->x = box_pos.x + size_sum.x;
                    if(velocity->x < 0.0f) {
                        velocity->x = -velocity->x * JP_BOUNCE_FACTOR_X;
                    }
                } else {
                    center->x = box_pos.x - size_sum.x;
                    if(velocity->x > 0.0f) {
                        velocity->x = -velocity->x * JP_BOUNCE_FACTOR_X;
                    }
                }
            } else {
                if(center->y > box_pos.y) {
                    center->y = box_pos.y + size_sum.y;
                    if(velocity->y < 0.0f) {
                        velocity->y = 0.0f;
                    }
                } else {
                    center->y = box_pos.y - size_sum.y;
                    if(velocity->y > 0.0f) {
                        velocity->y = 0.0f;
                    }
                }
            }
        }
    }

    center->y += tilemap_height;
}

static void jp_update_player(jump_prince_game_t *game, float delta)
{
    bool jump_released = game->prev_input_jump && !game->input_jump;

    game->velocity.y += JP_GRAVITY * delta;
    game->is_on_ground = jp_box_colliding_with_tilemap(game, game->screen_offset_y,
                                                       jp_vec2(game->position.x, game->position.y + JP_PLAYER_HALF_H),
                                                       jp_vec2(0.1f, 0.05f));

    if(game->is_on_ground) {
        game->velocity.x = 0.0f;

        if(jump_released) {
            float jump_strength = jp_clamp(game->jump_hold_time * 2.6f, 1.1f, 2.0f) / 2.0f;
            jump_prince_vec2_t dir = jp_vec2(0.0f, -1.0f);
            float x_move_strength = 0.75f - (jump_strength * 0.5f);

            if(game->input_right) {
                dir.x += x_move_strength;
                game->is_facing_right = true;
            }
            if(game->input_left) {
                dir.x -= x_move_strength;
                game->is_facing_right = false;
            }

            dir = jp_vec2_normalize(dir);
            game->velocity = jp_vec2_scale(dir, jump_strength * JP_JUMP_STRENGTH);
        }

        if(game->input_jump) {
            game->jump_hold_time += delta;
        } else {
            game->jump_hold_time = 0.0f;
            if(game->input_right) {
                game->velocity.x += JP_SPEED * delta;
                game->is_facing_right = true;
            }
            if(game->input_left) {
                game->velocity.x -= JP_SPEED * delta;
                game->is_facing_right = false;
            }
        }
    } else {
        game->jump_hold_time = 0.0f;
    }

    float speed = jp_vec2_len(game->velocity);
    if(speed > 25.0f) {
        game->velocity = jp_vec2_scale(jp_vec2_normalize(game->velocity), 25.0f);
    }

    game->position.x += game->velocity.x * delta;
    game->position.y += game->velocity.y * delta;
    game->prev_input_jump = game->input_jump;
    game->anim_time += delta;
}

void jump_prince_game_init(jump_prince_game_t *game)
{
    if(!game) {
        return;
    }
    *game = (jump_prince_game_t) {
        .state = JUMP_PRINCE_STOPPED,
        .is_facing_right = true,
    };
}

void jump_prince_game_start(jump_prince_game_t *game)
{
    if(!game) {
        return;
    }

    game->position = jp_vec2(8.0f, 6.0f);
    game->velocity = jp_vec2(0.0f, 0.0f);
    game->jump_hold_time = 0.0f;
    game->anim_time = 0.0f;
    game->is_on_ground = false;
    game->is_facing_right = true;
    game->input_left = false;
    game->input_right = false;
    game->input_jump = false;
    game->prev_input_jump = false;
    game->state = JUMP_PRINCE_RUNNING;
    jp_update_screen(game);
}

void jump_prince_game_restart(jump_prince_game_t *game)
{
    jump_prince_game_start(game);
}

void jump_prince_game_pause(jump_prince_game_t *game)
{
    if(game && game->state == JUMP_PRINCE_RUNNING) {
        game->state = JUMP_PRINCE_PAUSED;
    }
}

void jump_prince_game_resume(jump_prince_game_t *game)
{
    if(game && game->state == JUMP_PRINCE_PAUSED) {
        game->state = JUMP_PRINCE_RUNNING;
    }
}

void jump_prince_game_end(jump_prince_game_t *game)
{
    if(game) {
        game->state = JUMP_PRINCE_STOPPED;
    }
}

void jump_prince_game_set_input(jump_prince_game_t *game, bool left, bool right, bool jump)
{
    if(!game) {
        return;
    }

    game->input_left = left;
    game->input_right = right;
    game->input_jump = jump;
}

void jump_prince_game_update(jump_prince_game_t *game, uint32_t elapsed_ms)
{
    if(!game || game->state != JUMP_PRINCE_RUNNING) {
        return;
    }

    float delta = jp_clamp((float)elapsed_ms / 1000.0f, 0.0001f, 0.1f);
    jp_update_screen(game);
    jp_update_player(game, delta);
    jp_resolve_box_collision(game, game->screen_offset_y, &game->position, &game->velocity,
                             jp_vec2(JP_PLAYER_HALF_W, JP_PLAYER_HALF_H));
}

bool jump_prince_game_tile_full(const jump_prince_game_t *game, uint8_t x, uint8_t y)
{
    if(!game || x >= JUMP_PRINCE_MAP_W || y >= JUMP_PRINCE_MAP_H) {
        return false;
    }
    return jp_get_tile_from_map((uint8_t)game->screen_index, x, y, false) == '#';
}

bool jump_prince_game_tile_full_outside(const jump_prince_game_t *game, int x, int y)
{
    if(!game) {
        return false;
    }
    return jp_get_tile_from_map((uint8_t)game->screen_index, x, y, true) == '#';
}

void jump_prince_game_get_tile_sprite(const jump_prince_game_t *game,
                                      uint8_t x,
                                      uint8_t y,
                                      uint8_t *sprite_x,
                                      uint8_t *sprite_y)
{
    if(sprite_x) {
        *sprite_x = 0;
    }
    if(sprite_y) {
        *sprite_y = 0;
    }
    if(!game || !jump_prince_game_tile_full(game, x, y)) {
        return;
    }

    bool top = jump_prince_game_tile_full_outside(game, (int)x, (int)y - 1);
    bool bottom = jump_prince_game_tile_full_outside(game, (int)x, (int)y + 1);
    bool right = jump_prince_game_tile_full_outside(game, (int)x + 1, (int)y);
    bool left = jump_prince_game_tile_full_outside(game, (int)x - 1, (int)y);
    bool top_right = jump_prince_game_tile_full_outside(game, (int)x + 1, (int)y - 1);
    bool bottom_right = jump_prince_game_tile_full_outside(game, (int)x + 1, (int)y + 1);
    bool top_left = jump_prince_game_tile_full_outside(game, (int)x - 1, (int)y - 1);
    bool bottom_left = jump_prince_game_tile_full_outside(game, (int)x - 1, (int)y + 1);

    int sx = 1;
    int sy = 1;
    if(top) {
        sy += 1;
    }
    if(bottom) {
        sy -= 1;
    }
    if(right) {
        sx -= 1;
    }
    if(left) {
        sx += 1;
    }

    if(!top && !bottom && !right && !left) {
        sx = 3;
        sy = 3;
    }
    if(!left && !right && sx == 1) {
        sx = 3;
    }
    if(!top && !bottom && sy == 1) {
        sy = 3;
    }

    if(sx == 1 && sy == 1) {
        if(!top_right && bottom_right && top_left && bottom_left) {
            sx = 4;
            sy = 2;
        }
        if(top_right && !bottom_right && top_left && bottom_left) {
            sx = 4;
            sy = 0;
        }
        if(top_right && bottom_right && !top_left && bottom_left) {
            sx = 6;
            sy = 2;
        }
        if(top_right && bottom_right && top_left && !bottom_left) {
            sx = 6;
            sy = 0;
        }
    }

    if(sprite_x) {
        *sprite_x = (uint8_t)sx;
    }
    if(sprite_y) {
        *sprite_y = (uint8_t)sy;
    }
}

uint8_t jump_prince_game_get_player_sprite(const jump_prince_game_t *game)
{
    if(!game) {
        return 0;
    }

    if(game->is_on_ground) {
        if(game->jump_hold_time > 0.001f) {
            return 4;
        }
        if(fabsf(game->velocity.x) > 0.01f) {
            return 1 + (uint8_t)(((int)floorf(game->anim_time * 6.0f)) % 2);
        }
        return 0;
    }

    return game->velocity.y > 0.0f ? 5 : 6;
}

float jump_prince_game_player_screen_x(const jump_prince_game_t *game)
{
    return game ? game->position.x : 0.0f;
}

float jump_prince_game_player_screen_y(const jump_prince_game_t *game)
{
    return game ? game->position.y - game->screen_offset_y : 0.0f;
}

float jump_prince_game_jump_charge(const jump_prince_game_t *game)
{
    if(!game) {
        return 0.0f;
    }
    return jp_clamp(game->jump_hold_time * 2.6f, 0.0f, 2.0f) / 2.0f;
}

int jump_prince_game_get_screen_index(const jump_prince_game_t *game)
{
    return game ? game->screen_index : 0;
}

jump_prince_state_t jump_prince_game_get_state(const jump_prince_game_t *game)
{
    return game ? game->state : JUMP_PRINCE_STOPPED;
}
