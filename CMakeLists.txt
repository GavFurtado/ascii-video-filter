cmake_minimum_required(VERSION 3.16)

project(AsciiVideoFilter LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")


# Find FFmpeg packages via pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavformat
    libavcodec
    libavutil
    libswscale
    libswresample
)

# Source files (excluding main.cpp)
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    src/*.cpp
)


# Library target from your source files
add_library(AsciiVideoFilterLib STATIC ${SRC_FILES})
target_include_directories(AsciiVideoFilterLib PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(AsciiVideoFilterLib
    PkgConfig::FFMPEG
)
target_compile_options(AsciiVideoFilterLib PRIVATE ${FFMPEG_CFLAGS_OTHER})

# Executable target
add_executable(AsciiVideoFilter main.cpp)
target_link_libraries(AsciiVideoFilter
    AsciiVideoFilterLib
)

# Add debug logging macro globally
target_compile_definitions(AsciiVideoFilterLib PRIVATE DEBUG)
target_compile_definitions(AsciiVideoFilter PRIVATE DEBUG)
