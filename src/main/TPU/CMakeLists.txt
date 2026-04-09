cmake_minimum_required(VERSION 3.13)

# Define the path to coralmicro so we don't have to type ../../../ repeatedly
set(CORAL_MICRO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../third-party/coralmicro")

# Toolchain must be set before project() call.
if (NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE ${CORAL_MICRO_PATH}/cmake/toolchain-arm-none-eabi-gcc.cmake)
endif()

project(coralmicro-app)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(${CORAL_MICRO_PATH})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(${CORAL_MICRO_PATH} coralmicro_build)

add_executable_m7(coralmicro-app
        coral_main.cc
        libs/uart.cc
        libs/inference.cc
        libs/tab.c
        DATA
        ${PROJECT_SOURCE_DIR}/models/ssd_mobilenet_v2_face_quant_postprocess_edgetpu.tflite
        ${PROJECT_SOURCE_DIR}/images/denby.rgb
)

target_link_libraries(coralmicro-app
        libs_base-m7_freertos
)
