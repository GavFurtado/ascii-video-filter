cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(AsciiVideoFilter)

# Find FFmpeg packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavformat
    libavcodec
    libavutil
    libswscale
)

# Create a library from your source files (excluding main.cpp)
file(GLOB_RECURSE SRC_FILES src/*.cpp)
add_library(AsciiVideoFilterLib ${SRC_FILES})

# Link FFmpeg to the library
target_link_libraries(AsciiVideoFilterLib
    PkgConfig::FFMPEG
)

# Include directories for the library
target_include_directories(AsciiVideoFilterLib PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${FFMPEG_INCLUDE_DIRS}
)

# Add compile definitions
target_compile_definitions(AsciiVideoFilterLib PRIVATE
    ${FFMPEG_CFLAGS_OTHER}
)

# Main executable
add_executable(AsciiVideoFilter main.cpp)
target_link_libraries(AsciiVideoFilter AsciiVideoFilterLib)

# ================================
# TESTING SETUP
# ================================

# Enable testing
enable_testing()

# Create test directory structure
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tests)

# Test 1: Basic types test (no FFmpeg dependencies)
add_executable(test_basic_types tests/test_basic_types.cpp)
target_link_libraries(test_basic_types AsciiVideoFilterLib)
add_test(NAME BasicTypes COMMAND test_basic_types)

# Test 2: Video decoder test (requires FFmpeg)
add_executable(test_video_decoder tests/test_decoder.cpp)
target_link_libraries(test_video_decoder AsciiVideoFilterLib)
add_test(NAME VideoDecoder COMMAND test_video_decoder)

# # Test 3: ASCII converter test
# add_executable(test_ascii_converter test/test_ascii_converter.cpp)
# target_link_libraries(test_ascii_converter AsciiVideoFilterLib)
# add_test(NAME AsciiConverter COMMAND test_ascii_converter)

# Test 4: Integration test (if you have a test video file)
# add_test(NAME Integration
#          COMMAND AsciiVideoFilter ${CMAKE_SOURCE_DIR}/test/sample.mp4 ${CMAKE_BINARY_DIR}/test/output.mp4
#          WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

# Set test properties (optional)
set_tests_properties(BasicTypes PROPERTIES TIMEOUT 10)
set_tests_properties(VideoDecoder PROPERTIES TIMEOUT 30)
# set_tests_properties(AsciiConverter PROPERTIES TIMEOUT 30)

# Custom target to run all tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS test_basic_types test_video_decoder
    COMMENT "Running all tests"
)

# # Generate compile_commands.json (CMAKE 3.5 and above sadly)
# set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
