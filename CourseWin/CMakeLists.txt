cmake_minimum_required(VERSION 4.0)
project(CourseWin)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXE_LINKER_FLAGS "-static")

add_definitions(-DUNICODE -D_UNICODE)
add_executable(CourseWin WIN32
        main.cpp
        AudioRecorder.h
        AudioRecorder.cpp
)

target_link_libraries(CourseWin winmm)