cmake_minimum_required(VERSION 3.10)
project(cube_app_week10)

set(CMAKE_CXX_STANDARD 17)

include_directories(../include)

add_executable(cube_app main.cpp ../src/glad.c)

# Linux (GLX/X11) үшін кітапханаларды байлау
target_link_libraries(cube_app glfw GL X11 pthread Xrandr Xi dl)
