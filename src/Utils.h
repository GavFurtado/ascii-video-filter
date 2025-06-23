 #pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace AsciiVideoFilter {

// Custom application-specific error codes
enum AppErrorCode {
    APP_ERR_SUCCESS = 0,                     // Success (no error)
     // Starting custom errors at -100 to avoid collision with FFmpeg.
    APP_ERR_INVALID_ARG_COUNT = -100,        // Incorrect number of command-line arguments
    APP_ERR_UNSUPPORTED_FILE_TYPE = -101,    // Input file extension not supported
    APP_ERR_DECODER_NOT_FOUND = -102,        // FFmpeg could not find a suitable decoder
    APP_ERR_CONVERTER_INIT_FAILED = -103,    // AsciiConverter initialization failed (e.g., sws_getContext)
    APP_ERR_FRAME_CONVERSION_FAILED = -104,   // Error during frame-to-ASCII conversion
    APP_ERR_FONT_LOAD_FAILED = -105,   // Error loading font
};

namespace Utils {

inline bool validateArgs(int argc, int expected);
bool checkExtension(const std::filesystem::path &path, const std::vector<std::string> &extensions);

// Helper to get string description for AppErrorCode
const char* getAppErrorString(int errnum);

} // namespace Utils
} // namespace AsciiVideoFilter
