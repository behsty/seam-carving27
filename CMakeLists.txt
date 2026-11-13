cmake_minimum_required(VERSION 3.16)

project(SeamCarving LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

add_executable(seam_carving
    src/main.cpp
)

target_link_libraries(seam_carving PRIVATE
    ${OpenCV_LIBS}
)

# Copy test images to the executable directory
add_custom_command(TARGET seam_carving POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:seam_carving>/assets"
)
