cmake_minimum_required(VERSION 3.20)

include(FetchContent)

FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.0
)
FetchContent_MakeAvailable(unity)

add_executable(test_ads131m0x
    test_ads131m0x.c
)

target_link_libraries(test_ads131m0x PRIVATE
    ads131m0x
    unity
)

add_test(NAME ads131m0x COMMAND test_ads131m0x)