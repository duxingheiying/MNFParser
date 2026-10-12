cmake_minimum_required(VERSION 3.16)
project(MNFUniversalTool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(mnf_tool
  src/main.cc
  src/mnf_parser.cc
)

target_include_directories(mnf_tool PRIVATE include)

if(MSVC)
  target_compile_options(mnf_tool PRIVATE /W4 /permissive-)
else()
  target_compile_options(mnf_tool PRIVATE -Wall -Wextra -Wpedantic)
endif()
