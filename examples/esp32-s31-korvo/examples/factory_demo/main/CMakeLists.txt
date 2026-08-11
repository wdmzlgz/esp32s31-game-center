set(UI_DIR "${CMAKE_CURRENT_LIST_DIR}/ui")

file(GLOB UI_SRCS
    "${UI_DIR}/*.c"
    "${UI_DIR}/components/*.c"
    "${UI_DIR}/screens/*.c"
    "${UI_DIR}/fonts/*.c"
    "${UI_DIR}/images/*.c"
)
list(APPEND UI_SRCS
    "${UI_DIR}/jump_prince_game.c"
    "${UI_DIR}/images/ui_img_jumpprince_png.c"
    "${UI_DIR}/images/ui_img_jump_prince_sprites.c"
)
list(REMOVE_DUPLICATES UI_SRCS)

idf_component_register(
    SRCS
        "main.c"
        ${UI_SRCS}
    INCLUDE_DIRS "."
        "${UI_DIR}"
        "${UI_DIR}/components"
        "${UI_DIR}/screens"
    REQUIRES esp32_s31_korvo esp_lvgl_adapter espressif__cjson
)

target_add_binary_data(${COMPONENT_LIB} "ui/nonogram_puzzles.json" TEXT)
