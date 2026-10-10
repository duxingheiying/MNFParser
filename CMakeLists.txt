cmake_minimum_required(VERSION 3.16)
project(MNFParser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(mnf_parser
    src/main.cpp
    src/MnfReader.cpp
    src/BigEndianReader.cpp
)

target_include_directories(mnf_parser PRIVATE include)

if (MSVC)
    target_compile_options(mnf_parser PRIVATE /W4)
else()
    target_compile_options(mnf_parser PRIVATE -Wall -Wextra -Wpedantic)
endif()
