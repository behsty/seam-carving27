
cmake_minimum_required(VERSION 3.16)

project(SeamCarving LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(seam_carving
    src/main.cpp
)
