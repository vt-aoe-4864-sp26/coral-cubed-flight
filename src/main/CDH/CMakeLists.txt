cmake_minimum_required(VERSION 3.20.0)

set(ZEPHYR_BASE ${CMAKE_CURRENT_SOURCE_DIR}/../../zephyr CACHE PATH "Zephyr base dir")

find_package(Zephyr REQUIRED HINTS ${ZEPHYR_BASE})
project(cdh)

target_sources(app PRIVATE 
    cdh_main.c
    libs/cdh.c
    libs/tab.c
)
target_include_directories(app PRIVATE libs)