cmake_minimum_required(VERSION 3.22.1)
project("readingmusic")

set(OBOE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/oboe)

if(NOT EXISTS ${OBOE_DIR}/CMakeLists.txt)
    include(FetchContent)
    FetchContent_Declare(
        oboe
        GIT_REPOSITORY https://github.com/google/oboe.git
        GIT_TAG 1.9.0
    )
    FetchContent_MakeAvailable(oboe)
else()
    add_subdirectory(oboe)
endif()

add_library(readingmusic SHARED
    native_bridge.cpp
    AudioEngine.cpp
    StylePresets.cpp
)

target_include_directories(readingmusic PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/synth
)

target_link_libraries(readingmusic
    oboe
    android
    log
)
