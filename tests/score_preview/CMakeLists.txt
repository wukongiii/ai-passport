cmake_minimum_required(VERSION 3.16)
project(score_preview C)
if(NOT SCORE_LVGL_SOURCE)
    message(FATAL_ERROR "Set SCORE_LVGL_SOURCE to LVGL 9.5.0 sources")
endif()
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE PATH "" FORCE)
add_subdirectory("${SCORE_LVGL_SOURCE}" lvgl)
add_executable(preview ../../main/app/app_preferences.c ../../main/app/app_settings_model.c ../../main/app/app_settings.c ../../main/ui/app_range.c ../../main/app/app_carousel.c ../../main/app/app_carousel_layout.c ../../assets/images/launcher/launcher_art.c ../../main/games/eight_module.c ../../main/app/app_registry.c ../../main/app/app_container.c ../../main/app/app_ui.c ../../main/ui/app_actions.c ../../main/ui/app_layout.c ../../main/ui/app_shell.c preview.c ../../main/score/score_ui.c ../../main/score/score.c ../../main/score/score_config.c
    ../../main/score/chinese_eight.c ../../main/score/score_controller.c
    ../../assets/fonts/score_font_16.c ../../assets/fonts/score_font_inventory.c
    ../../components/bsp/src/bsp_display_rounding.c)
target_include_directories(preview PRIVATE ../../main ../bsp_stubs ../../components/bsp/src)
target_link_libraries(preview PRIVATE lvgl)
