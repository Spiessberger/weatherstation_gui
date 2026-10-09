cmake_minimum_required(VERSION 3.28)
project(weatherstation_gui VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Quick)
qt_standard_project_setup(REQUIRES 6.12)

# ponytail: GCC/Clang flags only; add an MSVC branch if Windows builds are ever needed
add_compile_options(-Wall -Wextra -Wpedantic)

add_subdirectory(src/app)
