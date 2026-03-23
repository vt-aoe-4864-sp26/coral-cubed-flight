cmake_minimum_required(VERSION 3.20.0)


find_package(Zephyr REQUIRED)
project(com)

target_sources(app PRIVATE 
    com_main.c
    libs/com.c
    libs/tab.c
)
target_include_directories(app PRIVATE libs)