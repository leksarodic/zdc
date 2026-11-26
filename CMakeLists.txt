cmake_minimum_required(VERSION 3.16)
project(strada7010b C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(STRADA_BUILD_SDL "Build the SDL2 emulator" ON)
option(STRADA_BUILD_FBDEV "Build the Linux framebuffer/evdev target" ON)

set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

if(STRADA_BUILD_SDL)
    find_package(SDL2 REQUIRED)
endif()

include(FetchContent)
FetchContent_Declare(
    lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG        v9.3.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(lvgl)

if(STRADA_BUILD_SDL)
    target_compile_definitions(lvgl PUBLIC STRADA_USE_SDL=1)
    if(TARGET SDL2::SDL2)
        target_link_libraries(lvgl PUBLIC SDL2::SDL2)
    else()
        target_include_directories(lvgl PUBLIC ${SDL2_INCLUDE_DIRS})
        target_link_libraries(lvgl PUBLIC ${SDL2_LIBRARIES})
    endif()
endif()

add_library(strada_core STATIC
    app/bt_sim.c
    app/bt_uart.c
    app/ui.c
    app/panels.c
    app/call_panel.c
    app/theme.c
)
target_include_directories(strada_core PUBLIC app)
target_link_libraries(strada_core PUBLIC lvgl m)

if(STRADA_BUILD_SDL)
    add_executable(strada-ui app/main_sdl.c)
    target_link_libraries(strada-ui PRIVATE strada_core)
    if(TARGET SDL2::SDL2)
        target_link_libraries(strada-ui PRIVATE SDL2::SDL2)
    else()
        target_include_directories(strada-ui PRIVATE ${SDL2_INCLUDE_DIRS})
        target_link_libraries(strada-ui PRIVATE ${SDL2_LIBRARIES})
    endif()
endif()

if(STRADA_BUILD_FBDEV AND UNIX AND NOT APPLE)
    add_executable(strada-ui-fb app/main_fbdev.c)
    target_link_libraries(strada-ui-fb PRIVATE strada_core)
endif()

enable_testing()
add_executable(test_bt tests/test_bt.c app/bt_sim.c app/bt_uart.c)
target_include_directories(test_bt PRIVATE app)
add_test(NAME bt COMMAND test_bt)

if(TARGET strada-ui)
    install(TARGETS strada-ui)
endif()
if(TARGET strada-ui-fb)
    install(TARGETS strada-ui-fb)
endif()
