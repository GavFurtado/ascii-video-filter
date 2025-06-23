#include <algorithm> // For std::transform
#include <cctype>    // For std::tolower
#include "Utils.hpp"

namespace AsciiVideoFilter {
namespace Utils {

inline bool validateArgs(int argc, int expected) {
    return argc == expected;
}

bool checkExtension(const std::filesystem::path &path, const std::vector<std::string> &extensions) {
    std::string ext = path.extension().string();

    // Convert extension to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    for (const auto& validExt : extensions) {
        if (ext == validExt) {
            return true;
        }
    }
    return false;
}

const char* getAppErrorString(int errnum) {
    switch (static_cast<AppErrorCode>(errnum)) {
        case APP_ERR_SUCCESS: return "Success";
        case APP_ERR_INVALID_ARG_COUNT: return "Invalid argument count";
        case APP_ERR_UNSUPPORTED_FILE_TYPE: return "Unsupported file type";
        case APP_ERR_DECODER_NOT_FOUND: return "FFmpeg decoder not found for stream";
        case APP_ERR_CONVERTER_INIT_FAILED: return "ASCII converter initialization failed";
        case APP_ERR_FRAME_CONVERSION_FAILED: return "Frame to ASCII conversion failed";
        case APP_ERR_FONT_LOAD_FAILED: return "Loading font failed";
        default: return "Unknown application error";
    }
}

} // namespace Utils
} // namespace AsciiVideoFilter
