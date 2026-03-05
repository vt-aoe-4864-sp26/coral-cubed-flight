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

add_subdirectory(${CORAL_MICRO_PATH} coralmicro_build)

add_executable_m7(coralmicro-app
        demo.cc
        DATA
        models/ssd_mobilenet_v2_face_quant_postprocess_edgetpu.tflite
)

target_link_libraries(coralmicro-app
        libs_base-m7_freertos
)