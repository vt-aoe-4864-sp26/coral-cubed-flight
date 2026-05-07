cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED)
project(cdh_blink)

target_sources(app PRIVATE
    blink.c
    ../../libs/cdh.c
    ../../libs/tab.c
)
target_include_directories(app PRIVATE ../../libs)
