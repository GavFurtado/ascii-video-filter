#include <iostream>
#include <string>
#include "cxxopts.hpp"
#include "Utils.hpp"

namespace AsciiVideoFilter {

std::string ProgressTracker::formatTime(double seconds) const {
    int hours = static_cast<int>(seconds) / 3600;
    int minutes = (static_cast<int>(seconds) % 3600) / 60;
    int secs = static_cast<int>(seconds) % 60;

    std::ostringstream oss;
    if (hours > 0) {
        oss << hours << "h " << minutes << "m " << secs << "s";
    } else if (minutes > 0) {
        oss << minutes << "m " << secs << "s";
    } else {
        oss << secs << "s";
    }
    return oss.str();
}

std::string ProgressTracker::formatProgress(double percentage) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << percentage << "%";
    return oss.str();
}

ProgressTracker::ProgressTracker(int totalFrames, double fps, double updateInterval, bool enabled) 
    : m_totalFrames(totalFrames), m_processedFrames(0), m_frameRate(fps), m_updateInterval(updateInterval), m_enabled(enabled) {
    m_startTime = std::chrono::steady_clock::now();
    m_lastUpdate = m_startTime;

    if (m_enabled) {
        std::cout << "Processing " << m_totalFrames << " frames @" << std::fixed << std::setprecision(2) <<  m_frameRate << "fps\n";
        std::cout << "Progress updates every " << m_updateInterval << " seconds\n";
        std::cout << std::string(60, '-') << std::endl;
    }
}

void ProgressTracker::update(int frameNumber) {
    if (!m_enabled) return;

    m_processedFrames = frameNumber + 1;
    auto now = std::chrono::steady_clock::now();

    // Check if we should update progress
    auto timeSinceLastUpdate = std::chrono::duration<double>(now - m_lastUpdate).count();
    bool shouldUpdate = (timeSinceLastUpdate >= m_updateInterval) ||
        (m_processedFrames == m_totalFrames);  // Always show final frame

    if (shouldUpdate) {
        auto elapsed = std::chrono::duration<double>(now - m_startTime).count();
        double actualFps = m_processedFrames / elapsed;
        double percentage = (static_cast<double>(m_processedFrames) / m_totalFrames) * 100.0;

        // Calculate ETA
        double remainingFrames = m_totalFrames - m_processedFrames;
        double etaSeconds = remainingFrames / actualFps;

        // Progress bar
        int barWidth = 30;
        int filledWidth = static_cast<int>((percentage / 100.0) * barWidth);
        std::string progressBar = "[" + std::string(filledWidth, '=') + 
            std::string(barWidth - filledWidth, ' ') + "]";

        std::cout << "\r" << progressBar << " " 
            << formatProgress(percentage) << " "
            << "(" << m_processedFrames << "/" << m_totalFrames << ") "
            << "FPS: " << std::fixed << std::setprecision(1) << actualFps << " "
            << "Elapsed: " << formatTime(elapsed);

        if (m_processedFrames < m_totalFrames) {
            std::cout << " ETA: " << formatTime(etaSeconds);
        }

        std::cout << std::flush;

        if (m_processedFrames == m_totalFrames) {
            std::cout << "\nProcessing completed in " << formatTime(elapsed) << std::endl;
        }

        m_lastUpdate = now;
    }
}

void ProgressTracker::finish() {
    if (!m_enabled) return;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - m_startTime).count();
    double actualFps = m_processedFrames / elapsed;

    std::cout << "\n" << std::string(60, '-') << std::endl;
    std::cout << "Final Statistics:\n";
    std::cout << "  Frames processed: " << m_processedFrames << "/" << m_totalFrames << "\n";
    std::cout << "  Total time: " << formatTime(elapsed) << "\n";
    std::cout << "  Average FPS: " << std::fixed << std::setprecision(2) << actualFps << "\n";
    std::cout << std::string(60, '-') << std::endl;
}


namespace Utils {

AppConfig parseArguments(int argc, const char* argv[]) {
    AppConfig config;

    cxxopts::Options options("ascii-video-filter", "Convert videos to ASCII art");

    options.add_options()
        ("i,input", "Input video file", cxxopts::value<std::string>())
        ("o,output", "Output video file", cxxopts::value<std::string>())
        ("f,font", "Path to TTF font file", cxxopts::value<std::string>()->default_value(config.fontPath))
        ("p,preset", "Character preset (standard, detailed, binary)", cxxopts::value<std::string>()->default_value(config.charsetPreset))
        ("c,charset", "Custom character set (overrides preset)", cxxopts::value<std::string>())
        ("max-frames", "Maximum frames to process (-1 for all)", 
            cxxopts::value<int>()->default_value(std::to_string(-1)))
        ("block-width", "Character block width in pixels", 
            cxxopts::value<int>()->default_value(std::to_string(config.blockWidth)))
        ("block-height", "Character block height in pixels",
            cxxopts::value<int>()->default_value(std::to_string(config.blockHeight)))
        ("no-audio", "Disable audio processing")
        ("no-colour", "Disable colour video")
        ("v,verbose", "Enable verbose output")
        ("no-progress", "Disable progress output")
        ("h,help", "Print usage information");

    try {
        auto result = options.parse(argc, argv);

        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            std::exit(0);
        }

        // Required arguments
        if (!result.count("input")) {
            std::cerr << "Error: Input file is required\n";
            std::cerr << options.help() << std::endl;
            std::exit(1);
        }

        if (!result.count("output")) {
            std::cerr << "Error: Output file is required\n";
            std::cerr << options.help() << std::endl;
            std::exit(1);
        }

        config.inputPath = result["input"].as<std::string>();
        config.outputPath = result["output"].as<std::string>();
        config.fontPath = result["font"].as<std::string>();
        config.charsetPreset = result["preset"].as<std::string>();
        config.maxFrames = result["max-frames"].as<int>();
        config.blockWidth = result["block-width"].as<int>();
        config.blockHeight = result["block-height"].as<int>();
        config.enableAudio = !result.count("no-audio");
        config.enableColour = !result.count("no-colour");
        config.verbose = result.count("verbose");
        config.showProgress = !result.count("no-progress");

        if (result.count("charset")) {
            config.customCharset = result["charset"].as<std::string>();
        }

        // Validation
        if (!std::filesystem::exists(config.inputPath)) {
            std::cerr << "Error: Input file does not exist: " << config.inputPath << std::endl;
            std::exit(1);
        }

        if (!std::filesystem::exists(config.fontPath)) {
            std::cerr << "Error: Font file does not exist: " << config.fontPath << std::endl;
            std::exit(1);
        }

        if (config.blockWidth <= 0 || config.blockHeight <= 0) {
            std::cerr << "Error: Block dimensions must be positive\n";
            std::exit(1);
        }

        // Validate charset preset
        const std::unordered_map<std::string, std::string> validPresets = {
            {"standard", " .:-=+*#%@"},
            {"detailed", " .'`^,:;Il!i><~+_-?][}{1)(|\\/tfjrxnumbroCLJVUNYXOZmwqpdbkhao*#MW&8%B@$"},
            {"binary", " 01 "}
        };

        if (config.customCharset.empty() && validPresets.find(config.charsetPreset) == validPresets.end()) {
            std::cerr << "Error: Invalid preset '" << config.charsetPreset << "'. Valid presets: ";
            for (const auto& [key, val] : validPresets) {
                std::cerr << key << " ";
            }
            std::cerr << std::endl;
            std::exit(1);
        }

    } catch (const cxxopts::exceptions::parsing& e) {
        std::cerr << "Error parsing options: " << e.what() << std::endl;
        std::cerr << options.help() << std::endl;
        std::exit(1);
    }
    return config;
}

void printConfig(const AppConfig& config) {
    std::cout << "Configuration:\n";
    std::cout << "  Input: " << config.inputPath << "\n";
    std::cout << "  Output: " << config.outputPath << "\n";
    std::cout << "  Font: " << config.fontPath << "\n";
    std::cout << "  Charset: " << (config.customCharset.empty() ? config.charsetPreset : "custom") << "\n";
    std::cout << "  Block size: " << config.blockWidth << "x" << config.blockHeight << "\n";
    std::cout << "  Max frames: " << (config.maxFrames == -1 ? "all" : std::to_string(config.maxFrames)) << "\n";
    std::cout << "  Audio: " << (config.enableAudio ? "enabled" : "disabled") << "\n";
    std::cout << std::endl;
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

