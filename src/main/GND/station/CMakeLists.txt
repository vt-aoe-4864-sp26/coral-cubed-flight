cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED)
project(gnd)

target_sources(app PRIVATE 
    gnd_main.c
    libs/gnd.c
    libs/uart.c
    libs/radio.c
    libs/tab.c
)
target_include_directories(app PRIVATE libs)