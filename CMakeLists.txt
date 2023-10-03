cmake_minimum_required(VERSION 3.26)
project(opentok_encoder
        VERSION 0.1
        DESCRIPTION "Custom OpenTok Streamer"
        LANGUAGES CXX)

#################################################
# Settings
#################################################

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Let's ensure -std=c++xx instead of -std=g++xx
set(CMAKE_CXX_EXTENSIONS OFF)

# Let's nicely support folders in IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

string(APPEND CMAKE_CXX_FLAGS " -Wall")
string(APPEND CMAKE_CXX_FLAGS " -Wbuiltin-macro-redefined")
string(APPEND CMAKE_CXX_FLAGS " -pedantic")
string(APPEND CMAKE_CXX_FLAGS " -Werror")
string(APPEND CMAKE_CXX_FLAGS " -g")

# clangd completion
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Copy .env file for local config
configure_file(${CMAKE_SOURCE_DIR}/.env ${CMAKE_CURRENT_BINARY_DIR}/.env)

#################################################
# Source Code
#################################################

include(FindPkgConfig)

include(FetchContent)

# Dependencies
##############

# fmt

FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 10.0.0
)
FetchContent_MakeAvailable(fmt)

# dotenv

FetchContent_Declare(
        dotenv
        GIT_REPOSITORY https://github.com/laserpants/dotenv-cpp.git
        GIT_TAG master
)
FetchContent_MakeAvailable(dotenv)

# OpenTok

if (DEFINED ENV{LIBOPENTOK_PATH})
    message(STATUS "Opentok Path $ENV{LIBOPENTOK_PATH}")
    find_path(LIBOPENTOK_HEADER opentok.h PATHS $ENV{LIBOPENTOK_PATH}/include NO_DEFAULT_PATH)
    find_library(LIBOPENTOK_LIBRARIES libopentok NAMES libopentok.so PATHS $ENV{LIBOPENTOK_PATH}/lib NO_DEFAULT_PATH)

    message(STATUS "Opentok header $ENV{LIBOPENTOK_HEADER}")
    message(STATUS "Opentok libs $ENV{LIBOPENTOK_LIBRARIES}")
endif ()
if (NOT LIBOPENTOK_LIBRARIES AND NOT LIBOPENTOK_HEADER)
    pkg_search_module(LIBOPENTOK REQUIRED libopentok)
else ()
    set(LIBOPENTOK_LIBRARY_DIRS $ENV{LIBOPENTOK_PATH}/lib)
    set(LIBOPENTOK_INCLUDE_DIRS $ENV{LIBOPENTOK_PATH}/include)
endif ()

include_directories(${CMAKE_SOURCE_DIR}/src ${LIBOPENTOK_INCLUDE_DIRS})
link_directories(${LIBOPENTOK_LIBRARY_DIRS})

add_executable(opentok_encoder
        src/otk_thread.h
        src/otk_thread.c
        src/main.cpp)

target_link_libraries(opentok_encoder
        PRIVATE
        ${LIBOPENTOK_LIBRARIES}
        fmt::fmt
        dotenv
)
