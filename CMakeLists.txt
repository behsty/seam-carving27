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

add_executable(seam_carving ${SEAM_CARVING_SOURCES})

target_link_libraries(seam_carving PRIVATE
    ${OpenCV_LIBS}
)

target_include_directories(seam_carving PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# Copy test images to the executable directory
add_custom_command(TARGET seam_carving POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:seam_carving>/assets"
)
