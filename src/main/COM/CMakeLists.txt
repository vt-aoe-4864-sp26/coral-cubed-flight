cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED)
project(com)

target_sources(app PRIVATE 
    com_main.c
    libs/com.c
    libs/uart.c
    libs/radio.c
    libs/tab.c
    libs/radio_test.c
)
target_include_directories(app PRIVATE libs)