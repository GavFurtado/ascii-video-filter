cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(AsciiVideoFilter)

# Add source files
file(GLOB_RECURSE SRC_FILES src/*.cpp)
add_executable(AsciiVideoFilter main.cpp ${SRC_FILES})

# Find FFmpeg packages
find_package(PkgConfig REQUIRED)

# Tell pkg-config what to look for
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavformat
    libavcodec
    libavutil
    libswscale
)

# Link the libraries
target_link_libraries(AsciiVideoFilter
    PkgConfig::FFMPEG
)

# Include FFmpeg headers
target_include_directories(AsciiVideoFilter PRIVATE
    ${FFMPEG_INCLUDE_DIRS}
    src
)

# Add compile definitions if needed (some FFmpeg functions require them)
target_compile_definitions(AsciiVideoFilter PRIVATE
    ${FFMPEG_CFLAGS_OTHER}
)
