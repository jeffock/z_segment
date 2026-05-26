cmake_minimum_required(VERSION 3.20)
project(z_segment)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# find packages installed via vcpkg
# find_package(OpenCV REQUIRED)

add_executable(z_segment
    src/main.cpp
)

find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})
target_link_libraries(z_segment ${OpenCV_LIBS})

target_include_directories(z_segment PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)