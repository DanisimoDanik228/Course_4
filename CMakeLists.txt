cmake_minimum_required(VERSION 4.0)
project(Course)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXE_LINKER_FLAGS "-static")

add_executable(Course main.cpp
        VolumeScaner.hpp
        VolumeScaner.cpp)
target_link_libraries(Course winmm)