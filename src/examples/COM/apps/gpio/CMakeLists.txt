cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED)
project(com_gpio)

target_sources(app PRIVATE
    gpio.c
    ../../libs/com.c
    ../../libs/tab.c
    ../../libs/uart.c
    ../../libs/radio.c
)
target_include_directories(app PRIVATE ../../libs)
