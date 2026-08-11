#include "ui_app.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include "lvgl.h"
#include "jump_prince_game.h"
#include "nonogram_game.h"
#include "snake_game.h"
#include "ui.h"

#define SNAKE_CELL_SIZE 25
#define SNAKE_TIMER_PERIOD_MS 50
#define SNAKE_SWIPE_THRESHOLD 8
#define RAPID_TAP_TIMER_PERIOD_MS 10
#define RAPID_TAP_MAX_ROUNDS 5
#define WHACK_HOLE_COUNT 9
#define WHACK_TIMER_PERIOD_MS 50
#define WHACK_GAME_DURATION_MS 30000
#define NONOGRAM_TIMER_PERIOD_MS 100
#define NONOGRAM_BOARD_PIXELS 300
#define NONOGRAM_HINT_PIXELS 75
#define JUMP_PRINCE_TIMER_PERIOD_MS 33
#define JUMP_PRINCE_TILE_PIXELS 25
#define JUMP_PRINCE_PLAY_W (JUMP_PRINCE_MAP_W * JUMP_PRINCE_TILE_PIXELS)
#define JUMP_PRINCE_PLAY_H (JUMP_PRINCE_MAP_H * JUMP_PRINCE_TILE_PIXELS)
#define JUMP_PRINCE_LEFT_BUTTON_RIGHT_X 160
#define JUMP_PRINCE_RIGHT_BUTTON_LEFT_X 320

extern const lv_image_dsc_t * const ui_img_jump_prince_player_images[2][7];
extern const lv_image_dsc_t * const ui_img_jump_prince_tile_images[7][6];

typedef enum {
    RAPID_TAP_STOPPED,
    RAPID_TAP_WAITING,
    RAPID_TAP_READY,
    RAPID_TAP_RESULT,
    RAPID_TAP_PAUSED,
    RAPID_TAP_FINISHED,
} rapid_tap_state_t;

typedef enum {
    WHACK_STOPPED,
    WHACK_RUNNING,
    WHACK_PAUSED,
    WHACK_FINISHED,
} whack_state_t;

typedef enum {
    SNAKE_CELL_EMPTY,
    SNAKE_CELL_HEAD,
    SNAKE_CELL_BODY,
    SNAKE_CELL_FOOD,
} snake_cell_state_t;

typedef struct {
    lv_obj_t *cells[SNAKE_GAME_BOARD_SIZE][SNAKE_GAME_BOARD_SIZE];
    snake_cell_state_t rendered_cells[SNAKE_GAME_BOARD_SIZE][SNAKE_GAME_BOARD_SIZE];
    snake_cell_state_t pending_cells[SNAKE_GAME_BOARD_SIZE][SNAKE_GAME_BOARD_SIZE];
    lv_timer_t *timer;
    lv_point_t press_point;
    uint32_t last_tick;
    bool music_on;
    bool tools_open;
    bool touch_tracking;
    bool direction_sent;
} snake_ui_t;

typedef struct {
    lv_timer_t *timer;
    rapid_tap_state_t state;
    rapid_tap_state_t state_before_pause;
    uint32_t wait_target_ms;
    uint32_t wait_elapsed_ms;
    uint32_t ready_elapsed_ms;
    uint8_t round;
    uint8_t false_count;
    int32_t best_ms;
    bool music_on;
    bool tools_open;
} rapid_tap_ui_t;

typedef struct {
    lv_obj_t *holes[WHACK_HOLE_COUNT];
    lv_obj_t *images[WHACK_HOLE_COUNT];
    lv_timer_t *timer;
    whack_state_t state;
    whack_state_t state_before_pause;
    uint32_t elapsed_ms;
    uint32_t spawn_elapsed_ms;
    uint32_t spawn_target_ms;
    int32_t score;
    uint16_t combo;
    int8_t active_index;
    int8_t last_index;
    bool active_is_bomb;
    bool music_on;
    bool tools_open;
} whack_ui_t;

typedef struct {
    lv_obj_t *cells[NONOGRAM_GAME_MAX_SIZE][NONOGRAM_GAME_MAX_SIZE];
    lv_obj_t *row_hint_labels[NONOGRAM_GAME_MAX_SIZE];
    lv_obj_t *col_hint_labels[NONOGRAM_GAME_MAX_SIZE];
    lv_obj_t *solved_label;
    lv_timer_t *timer;
    uint8_t current_size;
    uint32_t last_tick;
    bool music_on;
    bool tools_open;
} nonogram_ui_t;

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *play_area;
    lv_obj_t *tiles[JUMP_PRINCE_MAP_H][JUMP_PRINCE_MAP_W];
    lv_obj_t *player;
    lv_obj_t *charge_bars[3];
    lv_obj_t *screen_label;
    lv_obj_t *control_pad;
    lv_timer_t *timer;
    lv_point_t control_point;
    uint32_t last_tick;
    int rendered_screen;
    bool input_left;
    bool input_right;
    bool input_jump;
    bool touch_active;
    bool touch_jump_active;
    bool release_clear_inputs;
    int8_t touch_direction;
} jump_prince_ui_t;

static snake_game_t s_snake_game;
static snake_ui_t s_snake_ui;
static rapid_tap_ui_t s_rapid_tap;
static whack_ui_t s_whack;
static nonogram_game_t s_nonogram_game;
static nonogram_ui_t s_nonogram;
static jump_prince_game_t s_jump_prince_game;
static jump_prince_ui_t s_jump_prince_ui = {
    .rendered_screen = -1,
};
static lv_obj_t *s_snake_pause_overlay;
static lv_obj_t *s_snake_input_layer;
static lv_obj_t *s_rapid_tap_pause_overlay;
static lv_obj_t *s_whack_pause_overlay;
static lv_obj_t *s_nonogram_pause_overlay;

static void load_screen(lv_obj_t *screen)
{
    if(screen) {
        lv_screen_load(screen);
    }
}

static void set_clickable(lv_obj_t *obj, lv_event_cb_t cb, void *user_data)
{
    if(!obj) {
        return;
    }
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(obj, cb, LV_EVENT_CLICKED, user_data);
}

static void set_music_style(lv_obj_t *obj, bool on)
{
    if(obj) {
        lv_obj_set_style_bg_color(obj, lv_color_hex(on ? 0xDFE7FC : 0xFEFDF9), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

static void consume_click_cb(lv_event_t *event)
{
    (void)event;
}

static void set_tools_container_clickable(lv_obj_t *obj)
{
    if(!obj) {
        return;
    }
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(obj, consume_click_cb, LV_EVENT_CLICKED, NULL);
}

static void create_pause_overlay(lv_obj_t *screen, lv_obj_t **overlay, lv_event_cb_t cb)
{
    *overlay = lv_obj_create(screen);
    lv_obj_remove_style_all(*overlay);
    lv_obj_set_size(*overlay, 480, 480);
    lv_obj_set_pos(*overlay, 0, 0);
    lv_obj_add_flag(*overlay, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(*overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(*overlay, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(*overlay, cb, LV_EVENT_CLICKED, NULL);
}

static void snake_clear_cell(lv_obj_t *cell)
{
    if(!cell) {
        return;
    }
    lv_obj_clean(cell);
    lv_obj_set_style_radius(cell, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(cell, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void snake_apply_cell_state(uint8_t x, uint8_t y, snake_cell_state_t state)
{
    lv_obj_t *cell = s_snake_ui.cells[y][x];
    if(!cell) {
        return;
    }

    snake_clear_cell(cell);

    switch(state) {
    case SNAKE_CELL_HEAD:
        lv_obj_set_style_radius(cell, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(cell, lv_color_hex(0x426E3F), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(cell, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        break;
    case SNAKE_CELL_BODY:
        lv_obj_set_style_radius(cell, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(cell, lv_color_hex(0x6EA65D), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(cell, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        break;
    case SNAKE_CELL_FOOD: {
        lv_obj_t *food = lv_obj_create(cell);
        lv_obj_remove_style_all(food);
        lv_obj_set_size(food, 12, 12);
        lv_obj_set_align(food, LV_ALIGN_CENTER);
        lv_obj_remove_flag(food, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(food, LV_RADIUS_CIRCLE, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(food, lv_color_hex(0xDF4B3F), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(food, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        break;
    }
    case SNAKE_CELL_EMPTY:
    default:
        break;
    }
}

static void snake_clear_board(void *user_data)
{
    (void)user_data;
    for(uint8_t y = 0; y < SNAKE_GAME_BOARD_SIZE; y++) {
        for(uint8_t x = 0; x < SNAKE_GAME_BOARD_SIZE; x++) {
            s_snake_ui.pending_cells[y][x] = SNAKE_CELL_EMPTY;
        }
    }
}

static void snake_mark_pending_cell(int8_t x, int8_t y, snake_cell_state_t state)
{
    if(x < 0 || y < 0 || x >= SNAKE_GAME_BOARD_SIZE || y >= SNAKE_GAME_BOARD_SIZE) {
        return;
    }

    s_snake_ui.pending_cells[y][x] = state;
}

static void snake_draw_head(int8_t x, int8_t y, void *user_data)
{
    (void)user_data;
    snake_mark_pending_cell(x, y, SNAKE_CELL_HEAD);
}

static void snake_draw_body(int8_t x, int8_t y, void *user_data)
{
    (void)user_data;
    snake_mark_pending_cell(x, y, SNAKE_CELL_BODY);
}

static void snake_draw_food(int8_t x, int8_t y, void *user_data)
{
    (void)user_data;
    snake_mark_pending_cell(x, y, SNAKE_CELL_FOOD);
}

static void snake_present_board(void *user_data)
{
    (void)user_data;
    for(uint8_t y = 0; y < SNAKE_GAME_BOARD_SIZE; y++) {
        for(uint8_t x = 0; x < SNAKE_GAME_BOARD_SIZE; x++) {
            if(s_snake_ui.rendered_cells[y][x] == s_snake_ui.pending_cells[y][x]) {
                continue;
            }
            snake_apply_cell_state(x, y, s_snake_ui.pending_cells[y][x]);
            s_snake_ui.rendered_cells[y][x] = s_snake_ui.pending_cells[y][x];
        }
    }
}

static void snake_update_info(uint16_t score, uint16_t length, uint32_t elapsed_s, void *user_data)
{
    (void)user_data;
    char text[16];

    snprintf(text, sizeof(text), "%u", (unsigned)score);
    lv_label_set_text(ui_SnakeLabelGameScoreData, text);

    snprintf(text, sizeof(text), "%u", (unsigned)length);
    lv_label_set_text(ui_SnakeLabelGameLengthData, text);

    snprintf(text, sizeof(text), "%02lu:%02lu", (unsigned long)(elapsed_s / 60U), (unsigned long)(elapsed_s % 60U));
    lv_label_set_text(ui_SnakeLabelGameTimeData, text);
}

static void create_snake_board(void)
{
    if(!ui_SnakeContainerPlayArea || s_snake_ui.cells[0][0]) {
        return;
    }

    lv_obj_set_size(ui_SnakeContainerPlayArea, SNAKE_CELL_SIZE * SNAKE_GAME_BOARD_SIZE,
                    SNAKE_CELL_SIZE * SNAKE_GAME_BOARD_SIZE);
    lv_obj_add_flag(ui_SnakeContainerPlayArea, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(ui_SnakeContainerPlayArea, LV_OBJ_FLAG_SCROLLABLE);

    for(uint8_t y = 0; y < SNAKE_GAME_BOARD_SIZE; y++) {
        for(uint8_t x = 0; x < SNAKE_GAME_BOARD_SIZE; x++) {
            lv_obj_t *cell = lv_obj_create(ui_SnakeContainerPlayArea);
            lv_obj_remove_style_all(cell);
            lv_obj_set_size(cell, SNAKE_CELL_SIZE, SNAKE_CELL_SIZE);
            lv_obj_set_pos(cell, x * SNAKE_CELL_SIZE, y * SNAKE_CELL_SIZE);
            lv_obj_remove_flag(cell, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_opa(cell, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(cell, lv_color_hex(0x93B77C), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(cell, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(cell, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            s_snake_ui.cells[y][x] = cell;
        }
    }
}

static void snake_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if(snake_game_get_state(&s_snake_game) != SNAKE_GAME_RUNNING) {
        s_snake_ui.last_tick = lv_tick_get();
        return;
    }

    uint32_t now = lv_tick_get();
    uint32_t elapsed = s_snake_ui.last_tick ? lv_tick_elaps(s_snake_ui.last_tick) : SNAKE_TIMER_PERIOD_MS;
    s_snake_ui.last_tick = now;
    snake_game_update(&s_snake_game, elapsed);
}

static void snake_start_game(void)
{
    create_snake_board();
    snake_game_set_speed(&s_snake_game, 500);
    snake_game_restart(&s_snake_game);
    s_snake_ui.last_tick = lv_tick_get();
}

static void snake_show_tools(void)
{
    if(!ui_SnakeContainerTools || !s_snake_pause_overlay) {
        return;
    }
    s_snake_ui.tools_open = true;
    snake_game_pause(&s_snake_game);
    lv_obj_remove_flag(s_snake_pause_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(ui_SnakeContainerTools, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_snake_pause_overlay);
    lv_obj_move_foreground(ui_SnakeContainerTools);
}

static void snake_hide_tools_and_resume(void)
{
    if(!ui_SnakeContainerTools || !s_snake_pause_overlay) {
        return;
    }
    s_snake_ui.tools_open = false;
    lv_obj_add_flag(s_snake_pause_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_SnakeContainerTools, LV_OBJ_FLAG_HIDDEN);
    snake_game_resume(&s_snake_game);
    s_snake_ui.last_tick = lv_tick_get();
}

static void snake_menu_clicked_cb(lv_event_t *event)
{
    (void)event;
    snake_show_tools();
}

static void snake_overlay_clicked_cb(lv_event_t *event)
{
    (void)event;
    snake_hide_tools_and_resume();
}

static void snake_exit_clicked_cb(lv_event_t *event)
{
    (void)event;
    snake_game_end(&s_snake_game);
    snake_hide_tools_and_resume();
    load_screen(ui_GameScreen);
}

static void snake_music_clicked_cb(lv_event_t *event)
{
    (void)event;
    s_snake_ui.music_on = !s_snake_ui.music_on;
    set_music_style(ui_SnakeContainerToolMusic, s_snake_ui.music_on);
}

static void snake_restart_clicked_cb(lv_event_t *event)
{
    (void)event;
    snake_hide_tools_and_resume();
    snake_start_game();
}

static void snake_touch_pressed_cb(lv_event_t *event)
{
    lv_indev_t *indev = lv_event_get_indev(event);
    if(!indev) {
        indev = lv_indev_active();
    }
    if(!indev) {
        return;
    }

    lv_indev_get_point(indev, &s_snake_ui.press_point);
    s_snake_ui.touch_tracking = true;
    s_snake_ui.direction_sent = false;
}

static bool snake_try_set_direction_from_point(lv_point_t point)
{
    if(!s_snake_ui.touch_tracking) {
        return false;
    }

    int32_t dx = point.x - s_snake_ui.press_point.x;
    int32_t dy = point.y - s_snake_ui.press_point.y;
    int32_t abs_dx = dx >= 0 ? dx : -dx;
    int32_t abs_dy = dy >= 0 ? dy : -dy;

    if(abs_dx < SNAKE_SWIPE_THRESHOLD && abs_dy < SNAKE_SWIPE_THRESHOLD) {
        return false;
    }

    if(abs_dy > abs_dx && dy < 0) {
        snake_game_set_direction(&s_snake_game, SNAKE_GAME_DIR_UP);
    } else if(abs_dx >= abs_dy && dx > 0) {
        snake_game_set_direction(&s_snake_game, SNAKE_GAME_DIR_RIGHT);
    } else if(abs_dy > abs_dx && dy > 0) {
        snake_game_set_direction(&s_snake_game, SNAKE_GAME_DIR_DOWN);
    } else if(abs_dx >= abs_dy && dx < 0) {
        snake_game_set_direction(&s_snake_game, SNAKE_GAME_DIR_LEFT);
    }

    return true;
}

static void snake_touch_pressing_cb(lv_event_t *event)
{
    if(s_snake_ui.direction_sent) {
        return;
    }

    lv_indev_t *indev = lv_event_get_indev(event);
    if(!indev) {
        indev = lv_indev_active();
    }
    if(!indev) {
        return;
    }

    lv_point_t point;
    lv_indev_get_point(indev, &point);
    s_snake_ui.direction_sent = snake_try_set_direction_from_point(point);
}

static void snake_touch_released_cb(lv_event_t *event)
{
    if(!s_snake_ui.touch_tracking) {
        return;
    }

    lv_indev_t *indev = lv_event_get_indev(event);
    if(!indev) {
        indev = lv_indev_active();
    }
    if(indev && !s_snake_ui.direction_sent) {
        lv_point_t release_point;
        lv_indev_get_point(indev, &release_point);
        s_snake_ui.direction_sent = snake_try_set_direction_from_point(release_point);
    }

    s_snake_ui.touch_tracking = false;
    s_snake_ui.direction_sent = false;
}

static void set_snake_swipe_target(lv_obj_t *obj)
{
    if(!obj) {
        return;
    }
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(obj, snake_touch_pressed_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(obj, snake_touch_pressing_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(obj, snake_touch_released_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(obj, snake_touch_released_cb, LV_EVENT_PRESS_LOST, NULL);
}

static void create_snake_input_layer(void)
{
    if(s_snake_input_layer || !ui_SnakeContainerPlayAreaBackground) {
        return;
    }

    s_snake_input_layer = lv_obj_create(ui_SnakeContainerPlayAreaBackground);
    lv_obj_remove_style_all(s_snake_input_layer);
    lv_obj_set_size(s_snake_input_layer, 400, 400);
    lv_obj_set_align(s_snake_input_layer, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(s_snake_input_layer, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    set_snake_swipe_target(s_snake_input_layer);
    lv_obj_move_foreground(s_snake_input_layer);
}

static void snake_screen_loaded_cb(lv_event_t *event)
{
    (void)event;
    snake_start_game();
}

static void rapid_tap_update_info(void)
{
    char text[16];

    snprintf(text, sizeof(text), "%u/%u", (unsigned)s_rapid_tap.round, RAPID_TAP_MAX_ROUNDS);
    lv_label_set_text(ui_ReactionLabelGameRoundData, text);

    snprintf(text, sizeof(text), "ROUND %u/%u", (unsigned)s_rapid_tap.round, RAPID_TAP_MAX_ROUNDS);
    lv_label_set_text(ui_ReactionLabelPlayAreaRound, text);

    if(s_rapid_tap.best_ms < 0) {
        lv_label_set_text(ui_ReactionLabelGameBestData, "--");
    } else {
        snprintf(text, sizeof(text), "%ldms", (long)s_rapid_tap.best_ms);
        lv_label_set_text(ui_ReactionLabelGameBestData, text);
    }

    snprintf(text, sizeof(text), "%u", (unsigned)s_rapid_tap.false_count);
    lv_label_set_text(ui_ReactionLabelGameFalseData, text);
}

static uint32_t rapid_tap_next_wait(void)
{
    return lv_rand(2000, 5000);
}

static void rapid_tap_begin_round(void)
{
    if(s_rapid_tap.round == 0 || s_rapid_tap.round > RAPID_TAP_MAX_ROUNDS) {
        s_rapid_tap.round = 1;
    }

    s_rapid_tap.state = RAPID_TAP_WAITING;
    s_rapid_tap.wait_target_ms = rapid_tap_next_wait();
    s_rapid_tap.wait_elapsed_ms = 0;
    s_rapid_tap.ready_elapsed_ms = 0;

    lv_obj_set_style_bg_color(ui_ReactionContainerPlayAreaBackground, lv_color_hex(0xEED964),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ui_ReactionLabelPlayAreaMainTitle, "Wait");
    lv_label_set_text(ui_ReactionLabelPlayAreaSubtitle, "Tap when it turns green");
    rapid_tap_update_info();
}

static void rapid_tap_start_game(void)
{
    s_rapid_tap.round = 1;
    s_rapid_tap.false_count = 0;
    s_rapid_tap.best_ms = -1;
    rapid_tap_begin_round();
}

static void rapid_tap_finish_game(void)
{
    s_rapid_tap.state = RAPID_TAP_FINISHED;
    lv_label_set_text(ui_ReactionLabelPlayAreaMainTitle, "Done");
    if(s_rapid_tap.best_ms < 0) {
        lv_label_set_text(ui_ReactionLabelPlayAreaSubtitle, "No valid reaction");
    } else {
        char text[32];
        snprintf(text, sizeof(text), "Best %ldms", (long)s_rapid_tap.best_ms);
        lv_label_set_text(ui_ReactionLabelPlayAreaSubtitle, text);
    }
}

static void rapid_tap_advance_round_or_finish(void)
{
    if(s_rapid_tap.round >= RAPID_TAP_MAX_ROUNDS) {
        rapid_tap_finish_game();
        return;
    }

    s_rapid_tap.round++;
    rapid_tap_begin_round();
}

static void rapid_tap_show_result(uint32_t reaction_ms)
{
    char text[24];

    if(s_rapid_tap.best_ms < 0 || reaction_ms < (uint32_t)s_rapid_tap.best_ms) {
        s_rapid_tap.best_ms = reaction_ms;
    }

    if(reaction_ms < 100) {
        snprintf(text, sizeof(text), "%lums", (unsigned long)reaction_ms);
        lv_label_set_text(ui_ReactionLabelPlayAreaMainTitle, text);
        lv_label_set_text(ui_ReactionLabelPlayAreaSubtitle, "Lightning Fast! Are you even human?");
    } else if(reaction_ms < 300) {
        snprintf(text, sizeof(text), "%lums", (unsigned long)reaction_ms);
        lv_label_set_text(ui_ReactionLabelPlayAreaMainTitle, text);
        lv_label_set_text(ui_ReactionLabelPlayAreaSubtitle, "Great Reaction!");
    } else if(reaction_ms < 600) {
        snprintf(text, sizeof(text), "%lums", (unsigned long)reaction_ms);
        lv_label_set_text(ui_ReactionLabelPlayAreaMainTitle, text);
        lv_label_set_text(ui_ReactionLabelPlayAreaSubtitle, "Nice Try!");
    } else {
        lv_label_set_text(ui_ReactionLabelPlayAreaMainTitle, "Wake Up!");
        lv_label_set_text(ui_ReactionLabelPlayAreaSubtitle, "");
    }

    s_rapid_tap.state = RAPID_TAP_RESULT;
    s_rapid_tap.wait_elapsed_ms = 0;
    rapid_tap_update_info();
}

static void rapid_tap_false_start(void)
{
    s_rapid_tap.false_count++;
    s_rapid_tap.state = RAPID_TAP_RESULT;
    s_rapid_tap.wait_elapsed_ms = 0;
    lv_obj_set_style_bg_color(ui_ReactionContainerPlayAreaBackground, lv_color_hex(0xEF8670),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ui_ReactionLabelPlayAreaMainTitle, "Oops!");
    lv_label_set_text(ui_ReactionLabelPlayAreaSubtitle, "The green is not ready!");
    rapid_tap_update_info();
}

static void rapid_tap_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if(s_rapid_tap.state == RAPID_TAP_WAITING) {
        s_rapid_tap.wait_elapsed_ms += RAPID_TAP_TIMER_PERIOD_MS;
        if(s_rapid_tap.wait_elapsed_ms >= s_rapid_tap.wait_target_ms) {
            s_rapid_tap.state = RAPID_TAP_READY;
            s_rapid_tap.ready_elapsed_ms = 0;
            lv_obj_set_style_bg_color(ui_ReactionContainerPlayAreaBackground, lv_color_hex(0x65C873),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(ui_ReactionLabelPlayAreaMainTitle, "Tap!");
            lv_label_set_text(ui_ReactionLabelPlayAreaSubtitle, "Now!");
        }
    } else if(s_rapid_tap.state == RAPID_TAP_READY) {
        s_rapid_tap.ready_elapsed_ms += RAPID_TAP_TIMER_PERIOD_MS;
    } else if(s_rapid_tap.state == RAPID_TAP_RESULT) {
        s_rapid_tap.wait_elapsed_ms += RAPID_TAP_TIMER_PERIOD_MS;
        if(s_rapid_tap.wait_elapsed_ms >= 1500) {
            rapid_tap_advance_round_or_finish();
        }
    }
}

static void rapid_tap_play_area_clicked_cb(lv_event_t *event)
{
    (void)event;
    if(s_rapid_tap.state == RAPID_TAP_WAITING) {
        rapid_tap_false_start();
    } else if(s_rapid_tap.state == RAPID_TAP_READY) {
        rapid_tap_show_result(s_rapid_tap.ready_elapsed_ms);
    }
}

static void rapid_tap_show_tools(void)
{
    if(!ui_ReactionContainerTools || !s_rapid_tap_pause_overlay) {
        return;
    }
    s_rapid_tap.tools_open = true;
    if(s_rapid_tap.state != RAPID_TAP_PAUSED) {
        s_rapid_tap.state_before_pause = s_rapid_tap.state;
        s_rapid_tap.state = RAPID_TAP_PAUSED;
    }
    lv_obj_remove_flag(s_rapid_tap_pause_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(ui_ReactionContainerTools, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_rapid_tap_pause_overlay);
    lv_obj_move_foreground(ui_ReactionContainerTools);
}

static void rapid_tap_hide_tools_and_resume(void)
{
    if(!ui_ReactionContainerTools || !s_rapid_tap_pause_overlay) {
        return;
    }
    s_rapid_tap.tools_open = false;
    lv_obj_add_flag(s_rapid_tap_pause_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_ReactionContainerTools, LV_OBJ_FLAG_HIDDEN);
    if(s_rapid_tap.state == RAPID_TAP_PAUSED) {
        s_rapid_tap.state = s_rapid_tap.state_before_pause;
    }
}

static void rapid_tap_menu_clicked_cb(lv_event_t *event)
{
    (void)event;
    rapid_tap_show_tools();
}

static void rapid_tap_overlay_clicked_cb(lv_event_t *event)
{
    (void)event;
    rapid_tap_hide_tools_and_resume();
}

static void rapid_tap_exit_clicked_cb(lv_event_t *event)
{
    (void)event;
    s_rapid_tap.state = RAPID_TAP_STOPPED;
    rapid_tap_hide_tools_and_resume();
    load_screen(ui_GameScreen);
}

static void rapid_tap_music_clicked_cb(lv_event_t *event)
{
    (void)event;
    s_rapid_tap.music_on = !s_rapid_tap.music_on;
    set_music_style(ui_ReactionContainerToolMusic, s_rapid_tap.music_on);
}

static void rapid_tap_restart_clicked_cb(lv_event_t *event)
{
    (void)event;
    rapid_tap_hide_tools_and_resume();
    rapid_tap_start_game();
}

static void rapid_tap_screen_loaded_cb(lv_event_t *event)
{
    (void)event;
    rapid_tap_start_game();
}

static void whack_update_info(void)
{
    char text[16];

    snprintf(text, sizeof(text), "%ld", (long)s_whack.score);
    lv_label_set_text(ui_WhackLabelGameScoreData, text);

    snprintf(text, sizeof(text), "%u", (unsigned)s_whack.combo);
    lv_label_set_text(ui_WhackLabelGameComboData, text);

    snprintf(text, sizeof(text), "%lus", (unsigned long)(s_whack.elapsed_ms / 1000U));
    lv_label_set_text(ui_WhackLabelGameTimeData, text);
}

static uint32_t whack_next_spawn_interval(void)
{
    return lv_rand(500, 1000);
}

static void whack_clear_active_target(void)
{
    if(s_whack.active_index >= 0 && s_whack.active_index < WHACK_HOLE_COUNT) {
        lv_image_set_src(s_whack.images[s_whack.active_index], &ui_img_hole_png);
    }
    s_whack.active_index = -1;
}

static int8_t whack_random_hole_index(void)
{
    if(WHACK_HOLE_COUNT <= 1) {
        return 0;
    }

    int8_t index = 0;
    do {
        index = (int8_t)lv_rand(0, WHACK_HOLE_COUNT - 1);
    } while(index == s_whack.last_index);

    return index;
}

static void whack_spawn_new_target(void)
{
    if(s_whack.state != WHACK_RUNNING) {
        return;
    }

    whack_clear_active_target();

    int8_t index = whack_random_hole_index();
    s_whack.active_index = index;
    s_whack.last_index = index;
    s_whack.active_is_bomb = lv_rand(0, 99) < 30;
    s_whack.spawn_elapsed_ms = 0;
    s_whack.spawn_target_ms = whack_next_spawn_interval();

    lv_image_set_src(s_whack.images[index], s_whack.active_is_bomb ? &ui_img_bomb_png : &ui_img_mole_png);
}

static void whack_finish_game(void)
{
    s_whack.state = WHACK_FINISHED;
    s_whack.elapsed_ms = WHACK_GAME_DURATION_MS;
    whack_clear_active_target();
    whack_update_info();
}

static void whack_start_game(void)
{
    s_whack.state = WHACK_RUNNING;
    s_whack.elapsed_ms = 0;
    s_whack.spawn_elapsed_ms = 0;
    s_whack.spawn_target_ms = whack_next_spawn_interval();
    s_whack.score = 0;
    s_whack.combo = 0;
    s_whack.active_index = -1;
    s_whack.last_index = -1;
    s_whack.active_is_bomb = false;

    for(uint8_t i = 0; i < WHACK_HOLE_COUNT; i++) {
        lv_image_set_src(s_whack.images[i], &ui_img_hole_png);
    }

    whack_update_info();
    whack_spawn_new_target();
}

static void whack_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if(s_whack.state != WHACK_RUNNING) {
        return;
    }

    s_whack.elapsed_ms += WHACK_TIMER_PERIOD_MS;
    if(s_whack.elapsed_ms >= WHACK_GAME_DURATION_MS) {
        whack_finish_game();
        return;
    }

    s_whack.spawn_elapsed_ms += WHACK_TIMER_PERIOD_MS;
    if(s_whack.spawn_elapsed_ms >= s_whack.spawn_target_ms) {
        whack_spawn_new_target();
    }

    whack_update_info();
}

static void whack_hole_clicked_cb(lv_event_t *event)
{
    if(s_whack.state != WHACK_RUNNING) {
        return;
    }

    int8_t index = (int8_t)(uintptr_t)lv_event_get_user_data(event);
    if(index != s_whack.active_index) {
        return;
    }

    if(s_whack.active_is_bomb) {
        s_whack.score -= 200;
        s_whack.combo = 0;
    } else {
        s_whack.score += 100;
        s_whack.combo++;
    }

    whack_update_info();
    whack_spawn_new_target();
}

static void whack_show_tools(void)
{
    if(!ui_WhackContainerTools || !s_whack_pause_overlay) {
        return;
    }
    s_whack.tools_open = true;
    if(s_whack.state != WHACK_PAUSED) {
        s_whack.state_before_pause = s_whack.state;
        s_whack.state = WHACK_PAUSED;
    }
    lv_obj_remove_flag(s_whack_pause_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(ui_WhackContainerTools, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_whack_pause_overlay);
    lv_obj_move_foreground(ui_WhackContainerTools);
}

static void whack_hide_tools_and_resume(void)
{
    if(!ui_WhackContainerTools || !s_whack_pause_overlay) {
        return;
    }
    s_whack.tools_open = false;
    lv_obj_add_flag(s_whack_pause_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_WhackContainerTools, LV_OBJ_FLAG_HIDDEN);
    if(s_whack.state == WHACK_PAUSED) {
        s_whack.state = s_whack.state_before_pause;
    }
}

static void whack_menu_clicked_cb(lv_event_t *event)
{
    (void)event;
    whack_show_tools();
}

static void whack_overlay_clicked_cb(lv_event_t *event)
{
    (void)event;
    whack_hide_tools_and_resume();
}

static void whack_exit_clicked_cb(lv_event_t *event)
{
    (void)event;
    whack_hide_tools_and_resume();
    s_whack.state = WHACK_STOPPED;
    whack_clear_active_target();
    load_screen(ui_GameScreen);
}

static void whack_music_clicked_cb(lv_event_t *event)
{
    (void)event;
    s_whack.music_on = !s_whack.music_on;
    set_music_style(ui_WhackContainerToolMusic, s_whack.music_on);
}

static void whack_restart_clicked_cb(lv_event_t *event)
{
    (void)event;
    whack_hide_tools_and_resume();
    whack_start_game();
}

static void whack_screen_loaded_cb(lv_event_t *event)
{
    (void)event;
    whack_start_game();
}

static void nonogram_format_hint_text(char *text, size_t size, const uint8_t *clues, uint8_t count, bool vertical)
{
    if(size == 0) {
        return;
    }

    text[0] = '\0';
    if(count == 0) {
        snprintf(text, size, "0");
        return;
    }

    for(uint8_t i = 0; i < count; i++) {
        char part[6];
        snprintf(part, sizeof(part), "%u", (unsigned)clues[i]);
        size_t used = strlen(text);
        if(used + 1 >= size) {
            break;
        }
        strncat(text, part, size - used - 1);
        if(i + 1 < count) {
            used = strlen(text);
            if(used + 1 >= size) {
                break;
            }
            strncat(text, vertical ? "\n" : " ", size - used - 1);
        }
    }
}

static nonogram_difficulty_t nonogram_difficulty_from_dropdown(void)
{
    uint16_t selected = lv_dropdown_get_selected(ui_NonogramDropdownGameDifficulty);
    if(selected == 0) {
        return NONOGRAM_DIFFICULTY_EASY;
    }
    if(selected == 2) {
        return NONOGRAM_DIFFICULTY_HARD;
    }
    return NONOGRAM_DIFFICULTY_NORMAL;
}

static uint16_t nonogram_dropdown_from_difficulty(nonogram_difficulty_t difficulty)
{
    if(difficulty == NONOGRAM_DIFFICULTY_EASY) {
        return 0;
    }
    if(difficulty == NONOGRAM_DIFFICULTY_HARD) {
        return 2;
    }
    return 1;
}

static void nonogram_delete_hint_labels(void)
{
    for(uint8_t i = 0; i < NONOGRAM_GAME_MAX_SIZE; i++) {
        if(s_nonogram.row_hint_labels[i]) {
            lv_obj_del(s_nonogram.row_hint_labels[i]);
            s_nonogram.row_hint_labels[i] = NULL;
        }
        if(s_nonogram.col_hint_labels[i]) {
            lv_obj_del(s_nonogram.col_hint_labels[i]);
            s_nonogram.col_hint_labels[i] = NULL;
        }
    }
}

static void nonogram_create_solved_label(void)
{
    if(s_nonogram.solved_label || !ui_NonogramContainerPlayArea) {
        return;
    }

    s_nonogram.solved_label = lv_label_create(ui_NonogramContainerPlayArea);
    lv_label_set_text(s_nonogram.solved_label, "Correct!");
    lv_obj_add_flag(s_nonogram.solved_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_nonogram.solved_label, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_align(s_nonogram.solved_label, LV_ALIGN_CENTER);
    lv_obj_set_style_text_font(s_nonogram.solved_label, &lv_font_montserrat_32, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(s_nonogram.solved_label, lv_color_hex(0x2F6F3A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(s_nonogram.solved_label, lv_color_hex(0xFEFDF9), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s_nonogram.solved_label, 220, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_hor(s_nonogram.solved_label, 18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_ver(s_nonogram.solved_label, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(s_nonogram.solved_label, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(s_nonogram.solved_label, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(s_nonogram.solved_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(s_nonogram.solved_label, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void nonogram_set_solved_prompt_visible(bool visible)
{
    nonogram_create_solved_label();
    if(!s_nonogram.solved_label) {
        return;
    }

    if(visible) {
        lv_obj_remove_flag(s_nonogram.solved_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_nonogram.solved_label);
    } else {
        lv_obj_add_flag(s_nonogram.solved_label, LV_OBJ_FLAG_HIDDEN);
    }
}

static void nonogram_apply_cell_style(lv_obj_t *cell, bool selected)
{
    lv_obj_set_style_radius(cell, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(cell, lv_color_hex(selected ? 0x405F3D : 0xFEFDF9), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(cell, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(cell, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(cell, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(cell, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void nonogram_cell_clicked_cb(lv_event_t *event)
{
    uint8_t packed = (uint8_t)(uintptr_t)lv_event_get_user_data(event);
    uint8_t x = packed % NONOGRAM_GAME_MAX_SIZE;
    uint8_t y = packed / NONOGRAM_GAME_MAX_SIZE;
    nonogram_game_select_cell(&s_nonogram_game, x, y);
}

static void nonogram_clear_board(uint8_t size, void *user_data)
{
    (void)user_data;
    if(!ui_NonogramContainerPaintArea || !ui_NonogramContainerPlayArea || size == 0) {
        return;
    }

    lv_obj_clean(ui_NonogramContainerPaintArea);
    memset(s_nonogram.cells, 0, sizeof(s_nonogram.cells));
    nonogram_delete_hint_labels();
    nonogram_set_solved_prompt_visible(false);

    s_nonogram.current_size = size;
    lv_obj_set_size(ui_NonogramContainerPaintArea, NONOGRAM_BOARD_PIXELS, NONOGRAM_BOARD_PIXELS);
    lv_obj_remove_flag(ui_NonogramContainerPaintArea, LV_OBJ_FLAG_SCROLLABLE);

    uint16_t cell_size = NONOGRAM_BOARD_PIXELS / size;
    for(uint8_t y = 0; y < size; y++) {
        for(uint8_t x = 0; x < size; x++) {
            lv_obj_t *cell = lv_obj_create(ui_NonogramContainerPaintArea);
            lv_obj_remove_style_all(cell);
            lv_obj_set_size(cell, cell_size, cell_size);
            lv_obj_set_pos(cell, x * cell_size, y * cell_size);
            lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_remove_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
            nonogram_apply_cell_style(cell, false);
            lv_obj_add_event_cb(cell, nonogram_cell_clicked_cb, LV_EVENT_CLICKED,
                                (void *)(uintptr_t)(y * NONOGRAM_GAME_MAX_SIZE + x));
            s_nonogram.cells[y][x] = cell;
        }
    }
}

static void nonogram_draw_cell(uint8_t x, uint8_t y, bool selected, void *user_data)
{
    (void)user_data;
    if(x >= NONOGRAM_GAME_MAX_SIZE || y >= NONOGRAM_GAME_MAX_SIZE || !s_nonogram.cells[y][x]) {
        return;
    }

    nonogram_apply_cell_style(s_nonogram.cells[y][x], selected);
}

static void nonogram_draw_hint(bool row, uint8_t index, const uint8_t *clues, uint8_t count, void *user_data)
{
    (void)user_data;
    if(!ui_NonogramContainerPlayArea || index >= NONOGRAM_GAME_MAX_SIZE || s_nonogram.current_size == 0) {
        return;
    }

    uint16_t cell_size = NONOGRAM_BOARD_PIXELS / s_nonogram.current_size;
    lv_obj_t **slot = row ? &s_nonogram.row_hint_labels[index] : &s_nonogram.col_hint_labels[index];
    lv_obj_t *label = lv_label_create(ui_NonogramContainerPlayArea);
    char text[16];

    nonogram_format_hint_text(text, sizeof(text), clues, count, !row);
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, lv_color_hex(0x1F2A1F), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(label, row ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN | LV_STATE_DEFAULT);

    if(row) {
        lv_obj_set_size(label, NONOGRAM_HINT_PIXELS - 8, 24);
        lv_obj_set_pos(label, 0, NONOGRAM_HINT_PIXELS + index * cell_size + (cell_size - 24) / 2);
    } else {
        uint16_t line_count = count == 0 ? 1 : count;
        uint16_t label_height = line_count * 20;
        lv_obj_set_size(label, cell_size, label_height);
        lv_obj_set_pos(label, NONOGRAM_HINT_PIXELS + index * cell_size, NONOGRAM_HINT_PIXELS - label_height - 4);
        lv_obj_set_style_text_line_space(label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    *slot = label;
}

static void nonogram_present_board(void *user_data)
{
    (void)user_data;
}

static uint32_t nonogram_time_seed_cb(void *user_data)
{
    (void)user_data;
    return lv_tick_get();
}

static void nonogram_update_info(nonogram_difficulty_t difficulty,
                                 uint8_t round,
                                 uint32_t elapsed_s,
                                 nonogram_game_state_t state,
                                 void *user_data)
{
    (void)user_data;

    char text[16];
    lv_dropdown_set_selected(ui_NonogramDropdownGameDifficulty, nonogram_dropdown_from_difficulty(difficulty));

    snprintf(text, sizeof(text), "%u/%u", (unsigned)round, NONOGRAM_GAME_MAX_ROUNDS);
    lv_label_set_text(ui_NonogramLabelGameRoundData, text);

    snprintf(text, sizeof(text), "%lus", (unsigned long)elapsed_s);
    lv_label_set_text(ui_NonogramLabelGameTimeData, text);

    nonogram_set_solved_prompt_visible(state == NONOGRAM_GAME_SOLVED);
}

static void nonogram_start_game(void)
{
    nonogram_game_restart(&s_nonogram_game);
    s_nonogram.last_tick = lv_tick_get();
}

static void nonogram_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    nonogram_game_state_t state = nonogram_game_get_state(&s_nonogram_game);
    if(state != NONOGRAM_GAME_RUNNING && state != NONOGRAM_GAME_SOLVED) {
        s_nonogram.last_tick = lv_tick_get();
        return;
    }

    uint32_t now = lv_tick_get();
    uint32_t elapsed = s_nonogram.last_tick ? lv_tick_elaps(s_nonogram.last_tick) : NONOGRAM_TIMER_PERIOD_MS;
    s_nonogram.last_tick = now;
    nonogram_game_update(&s_nonogram_game, elapsed);
}

static void nonogram_show_tools(void)
{
    if(!ui_NonogramContainerTools || !s_nonogram_pause_overlay) {
        return;
    }
    s_nonogram.tools_open = true;
    nonogram_game_pause(&s_nonogram_game);
    lv_obj_remove_flag(s_nonogram_pause_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(ui_NonogramContainerTools, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_nonogram_pause_overlay);
    lv_obj_move_foreground(ui_NonogramContainerTools);
}

static void nonogram_hide_tools_and_resume(void)
{
    if(!ui_NonogramContainerTools || !s_nonogram_pause_overlay) {
        return;
    }
    s_nonogram.tools_open = false;
    lv_obj_add_flag(s_nonogram_pause_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_NonogramContainerTools, LV_OBJ_FLAG_HIDDEN);
    nonogram_game_resume(&s_nonogram_game);
    s_nonogram.last_tick = lv_tick_get();
}

static void nonogram_menu_clicked_cb(lv_event_t *event)
{
    (void)event;
    nonogram_show_tools();
}

static void nonogram_overlay_clicked_cb(lv_event_t *event)
{
    (void)event;
    nonogram_hide_tools_and_resume();
}

static void nonogram_exit_clicked_cb(lv_event_t *event)
{
    (void)event;
    nonogram_game_end(&s_nonogram_game);
    nonogram_hide_tools_and_resume();
    load_screen(ui_GameScreen);
}

static void nonogram_music_clicked_cb(lv_event_t *event)
{
    (void)event;
    s_nonogram.music_on = !s_nonogram.music_on;
    set_music_style(ui_NonogramContainerToolMusic, s_nonogram.music_on);
}

static void nonogram_restart_clicked_cb(lv_event_t *event)
{
    (void)event;
    nonogram_hide_tools_and_resume();
    nonogram_start_game();
}

static void nonogram_difficulty_changed_cb(lv_event_t *event)
{
    (void)event;
    nonogram_game_set_difficulty(&s_nonogram_game, nonogram_difficulty_from_dropdown());
    s_nonogram.last_tick = lv_tick_get();
}

static void nonogram_screen_loaded_cb(lv_event_t *event)
{
    (void)event;
    nonogram_start_game();
}

static void jump_prince_apply_input(void)
{
    jump_prince_game_set_input(&s_jump_prince_game, s_jump_prince_ui.input_left,
                               s_jump_prince_ui.input_right, s_jump_prince_ui.input_jump);
}

static void jump_prince_set_control_style(lv_obj_t *btn, uint32_t color)
{
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, 96, 58);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(btn, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(btn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(btn, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(btn, 120, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_offset_x(btn, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_offset_y(btn, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
}

static lv_obj_t *jump_prince_create_text_button(lv_obj_t *parent,
                                                const char *text,
                                                int32_t x,
                                                int32_t y,
                                                uint32_t color)
{
    lv_obj_t *btn = lv_obj_create(parent);
    jump_prince_set_control_style(btn, color);
    lv_obj_set_pos(btn, x, y);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_align(label, LV_ALIGN_CENTER);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, lv_color_hex(0x111111), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return btn;
}

static void jump_prince_draw_map(void)
{
    int screen_index = jump_prince_game_get_screen_index(&s_jump_prince_game);
    if(screen_index == s_jump_prince_ui.rendered_screen) {
        return;
    }

    s_jump_prince_ui.rendered_screen = screen_index;
    for(uint8_t y = 0; y < JUMP_PRINCE_MAP_H; y++) {
        for(uint8_t x = 0; x < JUMP_PRINCE_MAP_W; x++) {
            lv_obj_t *tile = s_jump_prince_ui.tiles[y][x];
            if(!tile) {
                continue;
            }

            if(jump_prince_game_tile_full(&s_jump_prince_game, x, y)) {
                uint8_t sprite_x = 0;
                uint8_t sprite_y = 0;
                jump_prince_game_get_tile_sprite(&s_jump_prince_game, x, y, &sprite_x, &sprite_y);
                if(sprite_x < 7 && sprite_y < 6) {
                    lv_image_set_src(tile, ui_img_jump_prince_tile_images[sprite_x][sprite_y]);
                }
                lv_obj_remove_flag(tile, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(tile, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

static void jump_prince_draw_player(void)
{
    if(!s_jump_prince_ui.player) {
        return;
    }

    uint8_t sprite = jump_prince_game_get_player_sprite(&s_jump_prince_game);
    if(sprite > 6) {
        sprite = 0;
    }
    lv_image_set_src(s_jump_prince_ui.player,
                     s_jump_prince_game.is_facing_right ? ui_img_jump_prince_player_images[0][sprite]
                                                        : ui_img_jump_prince_player_images[1][sprite]);

    int32_t x = (int32_t)(jump_prince_game_player_screen_x(&s_jump_prince_game) * JUMP_PRINCE_TILE_PIXELS) -
                JUMP_PRINCE_TILE_PIXELS / 2;
    int32_t y = (int32_t)(jump_prince_game_player_screen_y(&s_jump_prince_game) * JUMP_PRINCE_TILE_PIXELS) -
                (JUMP_PRINCE_TILE_PIXELS * 10) / 16;

    lv_obj_set_pos(s_jump_prince_ui.player, x, y);

    int32_t charge_w = (int32_t)(jump_prince_game_jump_charge(&s_jump_prince_game) * 70.0f);
    int8_t active_bar = s_jump_prince_ui.touch_direction < 0 ? 0 : (s_jump_prince_ui.touch_direction > 0 ? 2 : 1);
    for(uint8_t i = 0; i < 3; i++) {
        lv_obj_set_width(s_jump_prince_ui.charge_bars[i],
                         s_jump_prince_ui.input_jump && i == active_bar ? charge_w : 0);
    }

    char text[20];
    snprintf(text, sizeof(text), "Stage %d", jump_prince_game_get_screen_index(&s_jump_prince_game));
    lv_label_set_text(s_jump_prince_ui.screen_label, text);
}

static void jump_prince_draw(void)
{
    jump_prince_draw_map();
    jump_prince_draw_player();
}

static void jump_prince_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if(jump_prince_game_get_state(&s_jump_prince_game) != JUMP_PRINCE_RUNNING) {
        s_jump_prince_ui.last_tick = lv_tick_get();
        return;
    }

    uint32_t now = lv_tick_get();
    uint32_t elapsed = s_jump_prince_ui.last_tick ? lv_tick_elaps(s_jump_prince_ui.last_tick) : JUMP_PRINCE_TIMER_PERIOD_MS;
    s_jump_prince_ui.last_tick = now;
    jump_prince_game_update(&s_jump_prince_game, elapsed);
    if(s_jump_prince_ui.release_clear_inputs) {
        s_jump_prince_ui.input_left = false;
        s_jump_prince_ui.input_right = false;
        s_jump_prince_ui.input_jump = false;
        s_jump_prince_ui.release_clear_inputs = false;
        jump_prince_apply_input();
    }
    jump_prince_draw();
}

static void jump_prince_control_update_from_point(const lv_point_t *point, bool release)
{
    if(!point) {
        return;
    }

    bool jump = s_jump_prince_ui.touch_jump_active;
    s_jump_prince_ui.input_left = s_jump_prince_ui.touch_direction < 0;
    s_jump_prince_ui.input_right = s_jump_prince_ui.touch_direction > 0;
    s_jump_prince_ui.input_jump = release ? false : jump;
    s_jump_prince_ui.release_clear_inputs = release && jump;
    jump_prince_apply_input();
}

static void jump_prince_control_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t *indev = lv_indev_get_act();
    lv_point_t point = s_jump_prince_ui.control_point;
    if(indev) {
        lv_indev_get_point(indev, &point);
        s_jump_prince_ui.control_point = point;
    }

    if(code == LV_EVENT_PRESSED) {
        s_jump_prince_ui.touch_active = true;
        s_jump_prince_ui.touch_jump_active = true;
        if(point.x < JUMP_PRINCE_LEFT_BUTTON_RIGHT_X) {
            s_jump_prince_ui.touch_direction = -1;
        } else if(point.x > JUMP_PRINCE_RIGHT_BUTTON_LEFT_X) {
            s_jump_prince_ui.touch_direction = 1;
        } else {
            s_jump_prince_ui.touch_direction = 0;
        }
        jump_prince_control_update_from_point(&point, false);
    } else if(code == LV_EVENT_PRESSING) {
        if(s_jump_prince_ui.touch_active) {
            jump_prince_control_update_from_point(&point, false);
        }
    } else if(code == LV_EVENT_RELEASED) {
        jump_prince_control_update_from_point(&point, true);
        s_jump_prince_ui.touch_active = false;
        s_jump_prince_ui.touch_jump_active = false;
        s_jump_prince_ui.touch_direction = 0;
    } else if(code == LV_EVENT_PRESS_LOST) {
        s_jump_prince_ui.input_left = false;
        s_jump_prince_ui.input_right = false;
        s_jump_prince_ui.input_jump = false;
        s_jump_prince_ui.touch_active = false;
        s_jump_prince_ui.touch_jump_active = false;
        s_jump_prince_ui.touch_direction = 0;
        s_jump_prince_ui.release_clear_inputs = false;
        jump_prince_apply_input();
    }
}

static void jump_prince_restart_clicked_cb(lv_event_t *event)
{
    (void)event;
    s_jump_prince_ui.input_left = false;
    s_jump_prince_ui.input_right = false;
    s_jump_prince_ui.input_jump = false;
    s_jump_prince_ui.touch_active = false;
    s_jump_prince_ui.touch_jump_active = false;
    s_jump_prince_ui.release_clear_inputs = false;
    s_jump_prince_ui.touch_direction = 0;
    s_jump_prince_ui.rendered_screen = -1;
    jump_prince_game_restart(&s_jump_prince_game);
    jump_prince_apply_input();
    s_jump_prince_ui.last_tick = lv_tick_get();
    jump_prince_draw();
}

static void jump_prince_exit_clicked_cb(lv_event_t *event)
{
    (void)event;
    jump_prince_game_end(&s_jump_prince_game);
    load_screen(ui_GameScreen);
}

static void jump_prince_screen_loaded_cb(lv_event_t *event)
{
    (void)event;
    jump_prince_restart_clicked_cb(NULL);
}

static void game_jump_prince_clicked_cb(lv_event_t *event)
{
    (void)event;
    load_screen(s_jump_prince_ui.screen);
}

static void setup_jump_prince_screen(void)
{
    jump_prince_game_init(&s_jump_prince_game);

    s_jump_prince_ui.screen = lv_obj_create(NULL);
    lv_obj_remove_flag(s_jump_prince_ui.screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_jump_prince_ui.screen, lv_color_hex(0x0F052D), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s_jump_prince_ui.screen, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *title = lv_label_create(s_jump_prince_ui.screen);
    lv_label_set_text(title, "Jump Prince");
    lv_obj_set_width(title, 480);
    lv_obj_set_pos(title, 0, 14);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFEFDF9), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *exit_btn = jump_prince_create_text_button(s_jump_prince_ui.screen, "Exit", 18, 12, 0xFEFDF9);
    lv_obj_set_size(exit_btn, 72, 42);
    lv_obj_add_event_cb(exit_btn, jump_prince_exit_clicked_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *restart_btn = jump_prince_create_text_button(s_jump_prince_ui.screen, "Restart", 374, 12, 0xDFE7FC);
    lv_obj_set_size(restart_btn, 88, 42);
    lv_obj_add_event_cb(restart_btn, jump_prince_restart_clicked_cb, LV_EVENT_CLICKED, NULL);

    s_jump_prince_ui.screen_label = lv_label_create(s_jump_prince_ui.screen);
    lv_label_set_text(s_jump_prince_ui.screen_label, "Stage 5");
    lv_obj_set_pos(s_jump_prince_ui.screen_label, 196, 50);
    lv_obj_set_style_text_font(s_jump_prince_ui.screen_label, &lv_font_montserrat_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(s_jump_prince_ui.screen_label, lv_color_hex(0xD8F5A2), LV_PART_MAIN | LV_STATE_DEFAULT);

    s_jump_prince_ui.play_area = lv_obj_create(s_jump_prince_ui.screen);
    lv_obj_remove_style_all(s_jump_prince_ui.play_area);
    lv_obj_set_size(s_jump_prince_ui.play_area, JUMP_PRINCE_PLAY_W, JUMP_PRINCE_PLAY_H);
    lv_obj_set_pos(s_jump_prince_ui.play_area, 40, 78);
    lv_obj_set_style_bg_color(s_jump_prince_ui.play_area, lv_color_hex(0x180E45), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s_jump_prince_ui.play_area, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(s_jump_prince_ui.play_area, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_flag(s_jump_prince_ui.play_area, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    for(uint8_t y = 0; y < JUMP_PRINCE_MAP_H; y++) {
        for(uint8_t x = 0; x < JUMP_PRINCE_MAP_W; x++) {
            lv_obj_t *tile = lv_image_create(s_jump_prince_ui.play_area);
            lv_image_set_src(tile, ui_img_jump_prince_tile_images[1][1]);
            lv_obj_set_pos(tile, x * JUMP_PRINCE_TILE_PIXELS, y * JUMP_PRINCE_TILE_PIXELS);
            lv_obj_remove_flag(tile, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
            s_jump_prince_ui.tiles[y][x] = tile;
        }
    }

    s_jump_prince_ui.player = lv_image_create(s_jump_prince_ui.play_area);
    lv_image_set_src(s_jump_prince_ui.player, ui_img_jump_prince_player_images[0][0]);
    lv_obj_remove_flag(s_jump_prince_ui.player, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *left_btn = jump_prince_create_text_button(s_jump_prince_ui.screen, "Left", 40, 400, 0xC7F0BD);
    lv_obj_t *jump_btn = jump_prince_create_text_button(s_jump_prince_ui.screen, "Jump", 192, 400, 0xF8DFA5);
    lv_obj_t *right_btn = jump_prince_create_text_button(s_jump_prince_ui.screen, "Right", 344, 400, 0xC7F0BD);
    lv_obj_remove_flag(left_btn, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(jump_btn, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(right_btn, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    const int32_t charge_bg_x[3] = {50, 202, 354};
    for(uint8_t i = 0; i < 3; i++) {
        lv_obj_t *charge_bg = lv_obj_create(s_jump_prince_ui.screen);
        lv_obj_remove_style_all(charge_bg);
        lv_obj_set_size(charge_bg, 76, 8);
        lv_obj_set_pos(charge_bg, charge_bg_x[i], 388);
        lv_obj_set_style_bg_color(charge_bg, lv_color_hex(0x352B69), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(charge_bg, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(charge_bg, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_remove_flag(charge_bg, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

        s_jump_prince_ui.charge_bars[i] = lv_obj_create(charge_bg);
        lv_obj_remove_style_all(s_jump_prince_ui.charge_bars[i]);
        lv_obj_set_size(s_jump_prince_ui.charge_bars[i], 0, 8);
        lv_obj_set_pos(s_jump_prince_ui.charge_bars[i], 0, 0);
        lv_obj_set_style_bg_color(s_jump_prince_ui.charge_bars[i], lv_color_hex(0xF7C35F),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(s_jump_prince_ui.charge_bars[i], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(s_jump_prince_ui.charge_bars[i], 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_remove_flag(s_jump_prince_ui.charge_bars[i], LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    }

    s_jump_prince_ui.control_pad = lv_obj_create(s_jump_prince_ui.screen);
    lv_obj_remove_style_all(s_jump_prince_ui.control_pad);
    lv_obj_set_size(s_jump_prince_ui.control_pad, 480, 98);
    lv_obj_set_pos(s_jump_prince_ui.control_pad, 0, 382);
    lv_obj_add_flag(s_jump_prince_ui.control_pad, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_jump_prince_ui.control_pad, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(s_jump_prince_ui.control_pad, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(s_jump_prince_ui.control_pad, jump_prince_control_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_jump_prince_ui.control_pad, jump_prince_control_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(s_jump_prince_ui.control_pad, jump_prince_control_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(s_jump_prince_ui.control_pad, jump_prince_control_event_cb, LV_EVENT_PRESS_LOST, NULL);

    lv_obj_add_event_cb(s_jump_prince_ui.screen, jump_prince_screen_loaded_cb, LV_EVENT_SCREEN_LOADED, NULL);
    s_jump_prince_ui.timer = lv_timer_create(jump_prince_timer_cb, JUMP_PRINCE_TIMER_PERIOD_MS, NULL);
}

static void game_snake_clicked_cb(lv_event_t *event)
{
    (void)event;
    load_screen(ui_SnakeScreen);
}

static void game_rapid_tap_clicked_cb(lv_event_t *event)
{
    (void)event;
    load_screen(ui_ReactionScreen);
}

static void game_whack_clicked_cb(lv_event_t *event)
{
    (void)event;
    load_screen(ui_WhackScreen);
}

static void game_nonogram_clicked_cb(lv_event_t *event)
{
    (void)event;
    load_screen(ui_NonogramScreen);
}

static void setup_snake_screen(void)
{
    snake_game_init(&s_snake_game);
    snake_game_set_draw_callbacks(&s_snake_game, snake_clear_board, snake_draw_head, snake_draw_body,
                                  snake_draw_food, snake_present_board, NULL);
    snake_game_set_info_callback(&s_snake_game, snake_update_info);
    create_snake_board();
    create_snake_input_layer();
    create_pause_overlay(ui_SnakeScreen, &s_snake_pause_overlay, snake_overlay_clicked_cb);
    set_tools_container_clickable(ui_SnakeContainerTools);

    s_snake_ui.music_on = true;
    set_music_style(ui_SnakeContainerToolMusic, s_snake_ui.music_on);

    set_clickable(ui_SnakeContainerToolMenu, snake_menu_clicked_cb, NULL);
    set_clickable(ui_SnakeImageToolMenu, snake_menu_clicked_cb, NULL);
    set_clickable(ui_SnakeContainerToolExit, snake_exit_clicked_cb, NULL);
    set_clickable(ui_SnakeImageToolExit, snake_exit_clicked_cb, NULL);
    set_clickable(ui_SnakeContainerToolMusic, snake_music_clicked_cb, NULL);
    set_clickable(ui_SnakeImageToolMusic, snake_music_clicked_cb, NULL);
    set_clickable(ui_SnakeContainerToolRestart, snake_restart_clicked_cb, NULL);
    set_clickable(ui_SnakeImageToolRestart, snake_restart_clicked_cb, NULL);

    lv_obj_add_event_cb(ui_SnakeScreen, snake_screen_loaded_cb, LV_EVENT_SCREEN_LOADED, NULL);

    s_snake_ui.timer = lv_timer_create(snake_timer_cb, SNAKE_TIMER_PERIOD_MS, NULL);
}

static void setup_rapid_tap_screen(void)
{
    create_pause_overlay(ui_ReactionScreen, &s_rapid_tap_pause_overlay, rapid_tap_overlay_clicked_cb);
    set_tools_container_clickable(ui_ReactionContainerTools);

    s_rapid_tap.music_on = true;
    s_rapid_tap.best_ms = -1;
    set_music_style(ui_ReactionContainerToolMusic, s_rapid_tap.music_on);

    set_clickable(ui_ReactionContainerToolMenu, rapid_tap_menu_clicked_cb, NULL);
    set_clickable(ui_ReactionImageToolMenu, rapid_tap_menu_clicked_cb, NULL);
    set_clickable(ui_ReactionContainerToolExit, rapid_tap_exit_clicked_cb, NULL);
    set_clickable(ui_ReactionImageToolExit, rapid_tap_exit_clicked_cb, NULL);
    set_clickable(ui_ReactionContainerToolMusic, rapid_tap_music_clicked_cb, NULL);
    set_clickable(ui_ReactionImageToolMusic, rapid_tap_music_clicked_cb, NULL);
    set_clickable(ui_ReactionContainerToolRestart, rapid_tap_restart_clicked_cb, NULL);
    set_clickable(ui_ReactionImageToolRestart, rapid_tap_restart_clicked_cb, NULL);
    set_clickable(ui_ReactionContainerPlayAreaBackground, rapid_tap_play_area_clicked_cb, NULL);

    lv_obj_add_event_cb(ui_ReactionScreen, rapid_tap_screen_loaded_cb, LV_EVENT_SCREEN_LOADED, NULL);

    s_rapid_tap.timer = lv_timer_create(rapid_tap_timer_cb, RAPID_TAP_TIMER_PERIOD_MS, NULL);
}

static void setup_whack_screen(void)
{
    s_whack.holes[0] = ui_WhackContainerPlayAreaHole1;
    s_whack.holes[1] = ui_WhackContainerPlayAreaHole2;
    s_whack.holes[2] = ui_WhackContainerPlayAreaHole3;
    s_whack.holes[3] = ui_WhackContainerPlayAreaHole4;
    s_whack.holes[4] = ui_WhackContainerPlayAreaHole5;
    s_whack.holes[5] = ui_WhackContainerPlayAreaHole6;
    s_whack.holes[6] = ui_WhackContainerPlayAreaHole7;
    s_whack.holes[7] = ui_WhackContainerPlayAreaHole8;
    s_whack.holes[8] = ui_WhackContainerPlayAreaHole9;

    s_whack.images[0] = ui_WhackImagePlayAreaHole1;
    s_whack.images[1] = ui_WhackImagePlayAreaHole2;
    s_whack.images[2] = ui_WhackImagePlayAreaHole3;
    s_whack.images[3] = ui_WhackImagePlayAreaHole4;
    s_whack.images[4] = ui_WhackImagePlayAreaHole5;
    s_whack.images[5] = ui_WhackImagePlayAreaHole6;
    s_whack.images[6] = ui_WhackImagePlayAreaHole7;
    s_whack.images[7] = ui_WhackImagePlayAreaHole8;
    s_whack.images[8] = ui_WhackImagePlayAreaHole9;

    create_pause_overlay(ui_WhackScreen, &s_whack_pause_overlay, whack_overlay_clicked_cb);
    set_tools_container_clickable(ui_WhackContainerTools);

    s_whack.music_on = true;
    s_whack.active_index = -1;
    s_whack.last_index = -1;
    set_music_style(ui_WhackContainerToolMusic, s_whack.music_on);

    set_clickable(ui_WhackContainerToolMenu, whack_menu_clicked_cb, NULL);
    set_clickable(ui_WhackImageToolMenu, whack_menu_clicked_cb, NULL);
    set_clickable(ui_WhackContainerToolExit, whack_exit_clicked_cb, NULL);
    set_clickable(ui_WhackImageToolExit, whack_exit_clicked_cb, NULL);
    set_clickable(ui_WhackContainerToolMusic, whack_music_clicked_cb, NULL);
    set_clickable(ui_WhackImageToolMusic, whack_music_clicked_cb, NULL);
    set_clickable(ui_WhackContainerToolRestart, whack_restart_clicked_cb, NULL);
    set_clickable(ui_WhackImageToolRestart, whack_restart_clicked_cb, NULL);

    for(uint8_t i = 0; i < WHACK_HOLE_COUNT; i++) {
        set_clickable(s_whack.holes[i], whack_hole_clicked_cb, (void *)(uintptr_t)i);
        lv_obj_remove_flag(s_whack.images[i], LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_image_set_src(s_whack.images[i], &ui_img_hole_png);
    }

    lv_obj_add_event_cb(ui_WhackScreen, whack_screen_loaded_cb, LV_EVENT_SCREEN_LOADED, NULL);

    s_whack.timer = lv_timer_create(whack_timer_cb, WHACK_TIMER_PERIOD_MS, NULL);
}

static void setup_nonogram_screen(void)
{
    nonogram_game_init(&s_nonogram_game);
    nonogram_game_set_draw_callbacks(&s_nonogram_game, nonogram_clear_board, nonogram_draw_cell,
                                     nonogram_draw_hint, nonogram_present_board, NULL);
    nonogram_game_set_info_callback(&s_nonogram_game, nonogram_update_info);
    nonogram_game_set_random_seed_callback(&s_nonogram_game, nonogram_time_seed_cb, NULL);

    create_pause_overlay(ui_NonogramScreen, &s_nonogram_pause_overlay, nonogram_overlay_clicked_cb);
    set_tools_container_clickable(ui_NonogramContainerTools);

    s_nonogram.music_on = true;
    lv_dropdown_set_selected(ui_NonogramDropdownGameDifficulty, 1);
    set_music_style(ui_NonogramContainerToolMusic, s_nonogram.music_on);

    set_clickable(ui_NonogramContainerToolMenu, nonogram_menu_clicked_cb, NULL);
    set_clickable(ui_NonogramImageToolMenu, nonogram_menu_clicked_cb, NULL);
    set_clickable(ui_NonogramContainerToolExit, nonogram_exit_clicked_cb, NULL);
    set_clickable(ui_NonogramImageToolExit, nonogram_exit_clicked_cb, NULL);
    set_clickable(ui_NonogramContainerToolQuestion, consume_click_cb, NULL);
    set_clickable(ui_NonogramImageToolQuestion, consume_click_cb, NULL);
    set_clickable(ui_NonogramContainerToolMusic, nonogram_music_clicked_cb, NULL);
    set_clickable(ui_NonogramImageToolMusic, nonogram_music_clicked_cb, NULL);
    set_clickable(ui_NonogramContainerToolRestart, nonogram_restart_clicked_cb, NULL);
    set_clickable(ui_NonogramImageToolRestart, nonogram_restart_clicked_cb, NULL);

    lv_obj_add_event_cb(ui_NonogramDropdownGameDifficulty, nonogram_difficulty_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_NonogramScreen, nonogram_screen_loaded_cb, LV_EVENT_SCREEN_LOADED, NULL);

    s_nonogram.timer = lv_timer_create(nonogram_timer_cb, NONOGRAM_TIMER_PERIOD_MS, NULL);
}

void ui_app_init(void)
{
    set_clickable(ui_GameContainerSnakeGame, game_snake_clicked_cb, NULL);
    set_clickable(ui_GameContainerReactionGame, game_rapid_tap_clicked_cb, NULL);
    set_clickable(ui_GameContainerWhackGame, game_whack_clicked_cb, NULL);
    set_clickable(ui_GameContainerNonogramGame, game_nonogram_clicked_cb, NULL);
    set_clickable(ui_GameContainerJumpPrinceGame, game_jump_prince_clicked_cb, NULL);
    if(ui_GameLabelJumpPrinceGameName) {
        lv_obj_set_width(ui_GameLabelJumpPrinceGameName, 180);
        lv_obj_set_x(ui_GameLabelJumpPrinceGameName, 0);
        lv_obj_set_style_text_align(ui_GameLabelJumpPrinceGameName, LV_TEXT_ALIGN_CENTER,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    setup_snake_screen();
    setup_rapid_tap_screen();
    setup_whack_screen();
    setup_nonogram_screen();
    setup_jump_prince_screen();

    load_screen(ui_GameScreen);
}
