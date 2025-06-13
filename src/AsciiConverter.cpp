#include "AsciiConverter.h"
#include "Utils.h" // AppErrorCode
#include <iostream>
#include <libswscale/swscale.h>
#include <sstream>
#include <cmath>

extern "C" {
    #include <libavutil/avutil.h>
    #include <libavutil/error.h>
}

namespace AsciiVideoFilter {

AsciiConverter::AsciiConverter()
    : m_swsContext(nullptr),
      m_rgbFrame(nullptr),
      m_rgbBuffer(nullptr),
      m_srcWidth(0),
      m_srcHeight(0),
      m_blockWidth(0),
      m_blockHeight(0),
      m_asciiChars(" .'`^,:;Il!i><~+_-?][}{1)(|\\/tfjrxnumbroCLJVUNYXOZmwqpdbkhao*#MW&8%B@$")
{
    // Constructor initializes members; actual FFmpeg setup is in 'init'.
}

AsciiConverter::~AsciiConverter() {
    cleanup();
}

void AsciiConverter::cleanup() {
    if (m_rgbBuffer) {
        av_free(m_rgbBuffer);
        m_rgbBuffer = nullptr;
    }
    if (m_rgbFrame) {
        av_frame_free(&m_rgbFrame);
        m_rgbFrame = nullptr;
    }
    if (m_swsContext) {
        sws_freeContext(m_swsContext);
        m_swsContext = nullptr;
    }
}

int AsciiConverter::init(int src_width, int src_height, AVPixelFormat src_pix_fmt,
                         int ascii_block_width, int ascii_block_height) {
    // Ensure state is clean before initialization
    cleanup();

    m_srcWidth = src_width;
    m_srcHeight = src_height;
    m_blockWidth = ascii_block_width;
    m_blockHeight = ascii_block_height;

    // Initialize SwsContext for converting to RGB24
    m_swsContext = sws_getContext(m_srcWidth, m_srcHeight, src_pix_fmt,
                                  m_srcWidth, m_srcHeight, AV_PIX_FMT_RGB24,
                                  SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!m_swsContext) {
        std::cerr << "Error (AsciiConverter::init): Could not initialize SwsContext for ASCII conversion.\n";
        cleanup();
        // Use custom error code
        return static_cast<int>(AppErrorCode::APP_ERR_CONVERTER_INIT_FAILED);
    }

    // Allocate RGB frame and its buffer
    m_rgbFrame = av_frame_alloc();
    if (!m_rgbFrame) {
        std::cerr << "Error (AsciiConverter::init): Could not allocate RGB AVFrame: " << av_err2str(AVERROR(ENOMEM)) << "\n";
        cleanup();
        return AVERROR(ENOMEM); // FFmpeg memory error
    }

    int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, m_srcWidth, m_srcHeight, 1);
    m_rgbBuffer = (uint8_t *)av_malloc(num_bytes);
    if (!m_rgbBuffer) {
        std::cerr << "Error (AsciiConverter::init): Could not allocate image buffer for RGB frame: " << av_err2str(AVERROR(ENOMEM)) << "\n";
        cleanup();
        return AVERROR(ENOMEM); // FFmpeg memory error
    }

    av_image_fill_arrays(m_rgbFrame->data, m_rgbFrame->linesize, m_rgbBuffer, AV_PIX_FMT_RGB24,
                         m_srcWidth, m_srcHeight, 1);
    m_rgbFrame->width = m_srcWidth;
    m_rgbFrame->height = m_srcHeight;
    m_rgbFrame->format = AV_PIX_FMT_RGB24;

    std::cout << "AsciiConverter initialized. Source: " << m_srcWidth << "x" << m_srcHeight
              << ", ASCII Block: " << m_blockWidth << "x" << m_blockHeight << "\n";

    return static_cast<int>(AppErrorCode::APP_ERR_SUCCESS); // Indicate success
}

std::string AsciiConverter::convert(AVFrame* decoded_frame) {
    if (!m_swsContext || !m_rgbFrame || !decoded_frame) {
        std::cerr << "Error (AsciiConverter::convert): Converter not properly initialized or invalid input frame.\n";
        return ""; // Return empty string on error
    }

    // Convert the decoded frame to RGB24
    sws_scale(m_swsContext, decoded_frame->data, decoded_frame->linesize, 0, decoded_frame->height,
              m_rgbFrame->data, m_rgbFrame->linesize);

    std::stringstream ss;

    // Iterate over the RGB24 frame in blocks to generate ASCII
    for (int y = 0; y < m_srcHeight; y += m_blockHeight) {
        for (int x = 0; x < m_srcWidth; x += m_blockWidth) {
            long sum_brightness = 0;
            int count = 0;

            // Calculate average brightness for the current block
            for (int dy = 0; dy < m_blockHeight && (y + dy) < m_srcHeight; ++dy) {
                for (int dx = 0; dx < m_blockWidth && (x + dx) < m_srcWidth; ++dx) {
                    // Calculate pixel offset: y * stride + x * bytes_per_pixel
                    // For RGB24, each pixel is 3 bytes (R, G, B)
                    uint8_t *pixel = m_rgbFrame->data[0] + (y + dy) * m_rgbFrame->linesize[0] + (x + dx) * 3;
                    uint8_t r = pixel[0];
                    uint8_t g = pixel[1];
                    uint8_t b = pixel[2];

                    // Simple luminance calculation: Y = 0.299R + 0.587G + 0.114B (standard formula)
                    int brightness = (r * 299 + g * 587 + b * 114) / 1000;
                    sum_brightness += brightness;
                    count++;
                }
            }

            if (count > 0) {
                int avg_brightness = static_cast<int>(std::round(static_cast<double>(sum_brightness) / count));
                // Map average brightness (0-255) to an index in the ascii_chars string
                // The brightest character is at the end of the string.
                int char_index = (avg_brightness * (m_asciiChars.length() - 1)) / 255;
                ss << m_asciiChars[char_index];
            } else {
                ss << " "; // Fallback for empty blocks (shouldn't happen with proper loop)
            }
        }
        ss << "\n"; // Newline after each row of ASCII characters
    }

    return ss.str();
}

} // namespace AsciiVideoFilter
