 #pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>

extern "C" {
    #include <libavutil/rational.h>
}

namespace AsciiVideoFilter {
// Video metadata structure to pass between decoder and encoder
struct VideoMetadata {
    int width = 0;              ///< Video width in pixels
    int height = 0;             ///< Video height in pixels
    AVRational timeBase = {0, 1}; ///< Time base (fps = 1/timeBase)
    AVRational frameRate = {0, 1}; ///< Frame rate
    int64_t duration = 0;       ///< Duration in timebase units
    int64_t bitRate = 0;        ///< Original bitrate (for reference)
    double durationSeconds = 0.0; ///< Duration in seconds

    // Computed properties
    double getFps() const {
        return frameRate.den > 0 ? static_cast<double>(frameRate.num) / frameRate.den : 0.0;
    }

    int64_t getTotalFrames() const {
        return static_cast<int64_t>(durationSeconds * getFps());
    }
};

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
