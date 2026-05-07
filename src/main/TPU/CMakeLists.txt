cmake_minimum_required(VERSION 3.13)

# ---- Target Selection ----
# CORAL_TARGET must be set to "dev" or "pico" via -DCORAL_TARGET=<target>
# "dev"  -> third-party/coralmicro   (Coral Dev Board Micro)
# "pico" -> third-party/coralpico    (Coral Pico Board)
if (NOT DEFINED CORAL_TARGET)
    message(FATAL_ERROR "CORAL_TARGET is not set. Pass -DCORAL_TARGET=dev or -DCORAL_TARGET=pico")
endif()

if (CORAL_TARGET STREQUAL "dev")
    set(CORAL_SDK_DIR "coralmicro")
elseif (CORAL_TARGET STREQUAL "pico")
    set(CORAL_SDK_DIR "coralpico")
else()
    message(FATAL_ERROR "Invalid CORAL_TARGET '${CORAL_TARGET}'. Must be 'dev' or 'pico'.")
endif()

message(STATUS "Building for CORAL_TARGET=${CORAL_TARGET} using ${CORAL_SDK_DIR}")

set(CORAL_MICRO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../third-party/${CORAL_SDK_DIR}")

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
        ${PROJECT_SOURCE_DIR}/images/blk.rgb
)

target_link_libraries(coralmicro-app
        libs_base-m7_freertos
)
