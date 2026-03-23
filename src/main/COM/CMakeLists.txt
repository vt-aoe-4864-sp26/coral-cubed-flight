cmake_minimum_required(VERSION 3.20.0)

set(ZEPHYR_BASE ${CMAKE_CURRENT_SOURCE_DIR}/../zephyr CACHE PATH "Zephyr base dir")

find_package(Zephyr REQUIRED HINTS ${ZEPHYR_BASE})
project(com)

target_sources(app PRIVATE 
    com_main.c
    libs/com.c
    libs/tab.c
)
target_include_directories(app PRIVATE libs)