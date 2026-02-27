cmake_minimum_required(VERSION 3.5)

# ESP-IDF component support
if(DEFINED ENV{IDF_PATH})
    idf_component_register(
        SRCS "src/ads131m0x.c"
        INCLUDE_DIRS "include"
        REQUIRES ""
    )
    return()
endif()

# Standard CMake build (works for Zephyr too)
project(ads131m0x VERSION 1.0.0 LANGUAGES C)

# Create static library
add_library(ads131m0x STATIC
    src/ads131m0x.c
)

# Set include directories
target_include_directories(ads131m0x PUBLIC include)
