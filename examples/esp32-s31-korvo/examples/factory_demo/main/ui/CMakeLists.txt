SET(SOURCES screens/ui_SnakeScreen.c
    screens/ui_ReactionScreen.c
    screens/ui_WhackScreen.c
    screens/ui_GameScreen.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    images/ui_img_menu_png.c
    images/ui_img_exit_png.c
    images/ui_img_question_png.c
    images/ui_img_music_png.c
    images/ui_img_restart_png.c
    images/ui_img_bomb_png.c
    images/ui_img_hole_png.c
    images/ui_img_whack_png.c
    images/ui_img_snake_png.c
    images/ui_img_reaction_png.c)

add_library(ui ${SOURCES})
