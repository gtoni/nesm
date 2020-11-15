cmake_minimum_required(VERSION 3.13)
project(nesm)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl2)

find_package(SDL2 REQUIRED)

set(SOURCE_FILES src/main.c src/emu/nes_system.c)
include_directories(src)
add_executable(nesm ${SOURCE_FILES})
target_link_libraries(nesm SDL2::Main)





































