cmake_minimum_required(VERSION 3.16)
project(ReshadeFocusBridge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# RESHADE_INCLUDE 指向 clone 下来的 ReShade 仓库的 include 目录
add_library(ReshadeFocusBridge SHARED FocusBridge.cpp)
target_include_directories(ReshadeFocusBridge PRIVATE "${RESHADE_INCLUDE}")

# 输出成 ReShade 认的扩展名
set_target_properties(ReshadeFocusBridge PROPERTIES
    PREFIX ""
    SUFFIX ".addon64"
)