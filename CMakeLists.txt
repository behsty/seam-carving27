cmake_minimum_required(VERSION 3.16)

project(SeamCarving LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

# Discover source files, including files in subfolders, on each build.
# Keep only this program's sources under src (one main function).
file(GLOB_RECURSE SEAM_CARVING_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
# main.cpp goes into the program only; everything else into a library that
# both the program and the tests link against.
list(FILTER SEAM_CARVING_SOURCES EXCLUDE REGEX ".*/src/main\\.cpp$")

add_library(seam_core STATIC ${SEAM_CARVING_SOURCES})

target_link_libraries(seam_core PUBLIC
    ${OpenCV_LIBS}
)

target_include_directories(seam_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_executable(seam_carving "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

target_link_libraries(seam_carving PRIVATE seam_core)

# Copy test images to the executable directory
add_custom_command(TARGET seam_carving POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:seam_carving>/assets"
)

# Correctness tests (DP vs brute force, graph cut, greedy, edge cases).
# Run with: ctest --test-dir out/build/windows-debug --output-on-failure
enable_testing()

add_executable(seam_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_seams.cpp")

target_link_libraries(seam_tests PRIVATE seam_core)

add_test(NAME seam_tests COMMAND seam_tests)
