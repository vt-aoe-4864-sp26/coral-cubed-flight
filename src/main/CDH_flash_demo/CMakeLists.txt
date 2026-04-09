cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(cdh_flash_demo)

target_sources(app PRIVATE src/main.c)
