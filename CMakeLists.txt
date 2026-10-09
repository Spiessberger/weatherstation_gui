cmake_minimum_required(VERSION 3.28)
project(weatherstation_gui VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
include(GNUInstallDirs)

# Third-party code comes first, so it gets neither AUTOMOC nor our warning flags.
if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.18.0
        GIT_SHALLOW TRUE
        SYSTEM
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(googletest)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Network Qml Quick)
qt_standard_project_setup(REQUIRES 6.12)

# ponytail: GCC/Clang flags only; add an MSVC branch if Windows builds are ever needed
add_compile_options(-Wall -Wextra -Wpedantic)

# The QML modules before the app, whose DEPENDENCIES TARGET needs their targets.
add_subdirectory(src/panomax)
add_subdirectory(src/ui)
add_subdirectory(src/app)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
