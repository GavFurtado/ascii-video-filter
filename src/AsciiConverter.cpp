#include "AsciiConverter.hpp"
#include "AsciiTypes.hpp"
#include "Utils.hpp" // AppErrorCode

#include <cerrno>
#include <iostream>
#include <cmath>

extern "C" {
    #include <libswscale/swscale.h>
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
      m_asciiChars(" .'`^,:;Il!i><~+_-?][}{1)(|\\/tfjrxnumbroCLJVUNYXOZmwqpdbkhao*#MW&8%B@$") // detailed character set
{}

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
                         int asciiBlockWidth, int asciiBlockHeight) {
    cleanup();

    m_srcWidth = src_width;
    m_srcHeight = src_height;
    m_blockWidth = asciiBlockWidth;
    m_blockHeight = asciiBlockHeight;

    m_gridCols = src_width / m_blockWidth;
    m_gridRows = src_height / m_blockHeight;

    // Initialize SwsContext for converting to RGB24
    m_swsContext = sws_getContext(m_srcWidth, m_srcHeight, src_pix_fmt,
                                  m_srcWidth, m_srcHeight, AV_PIX_FMT_RGB24,
                                  SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!m_swsContext) {
        std::cerr << "Error (AsciiConverter::init): Could not initialize SwsContext for ASCII conversion.\n";
        cleanup();
        return static_cast<int>(AppErrorCode::APP_ERR_CONVERTER_INIT_FAILED);
    }

    // Allocate RGB frame and its buffer
    m_rgbFrame = av_frame_alloc();
    if (!m_rgbFrame) {
        std::cerr << "Error (AsciiConverter::init): Could not allocate RGB AVFrame: " <<  av_make_error_string(m_errbuf, AV_ERROR_MAX_STRING_SIZE, AVERROR(ENOMEM))<< "\n";
        cleanup();
        return AVERROR(ENOMEM);
    }

    int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, m_srcWidth, m_srcHeight, 1);
    m_rgbBuffer = (uint8_t *)av_malloc(num_bytes);
    if (!m_rgbBuffer) {
        std::cerr << "Error (AsciiConverter::init): Could not allocate image buffer for RGB frame: " << av_make_error_string(m_errbuf, AV_ERROR_MAX_STRING_SIZE, AVERROR(ENOMEM)) << "\n";
        cleanup();
        return AVERROR(ENOMEM);
    }

    // RGB24 is a packed format. only data[0] (start of the buffer) and linesize[0] (i.e., srcWidth * 3) are used
    av_image_fill_arrays(m_rgbFrame->data, m_rgbFrame->linesize, m_rgbBuffer, AV_PIX_FMT_RGB24,
                         m_srcWidth, m_srcHeight, 1);
    m_rgbFrame->width = m_srcWidth;
    m_rgbFrame->height = m_srcHeight;
    m_rgbFrame->format = AV_PIX_FMT_RGB24;

    std::cout << "AsciiConverter initialized. Source: " << m_srcWidth << "x" << m_srcHeight
              << ", ASCII Block: " << m_blockWidth << "x" << m_blockHeight << "\n";

    return static_cast<int>(AppErrorCode::APP_ERR_SUCCESS); 
}

void AsciiConverter::convert(AVFrame* decodedFrame, AsciiGrid &outGrid, bool enableColor) {

    if (!m_swsContext || !m_rgbFrame || !decodedFrame) {
        std::cerr << "Error (AsciiConverter::convert): Not properly initialized.\n";
        return;
    }

    // Convert input frame to RGB24 format
    sws_scale(m_swsContext, decodedFrame->data, decodedFrame->linesize, 0, decodedFrame->height,
              m_rgbFrame->data, m_rgbFrame->linesize);

    // Ensure outGrid has correct dimensions (extra assignment)
    outGrid.cols = m_gridCols; 
    outGrid.rows = m_gridRows;

    // Loop through each ASCII block (row by row, column by column)
    for (int blockY = 0; blockY < outGrid.rows; ++blockY) {
        for (int blockX = 0; blockX < outGrid.cols; ++blockX) {
            long rSum = 0, gSum = 0, bSum = 0, brightnessSum = 0;
            int count = 0;

            // Loop through each pixel in the current ASCII block
            for (int dy = 0; dy < m_blockHeight; ++dy) {
                for (int dx = 0; dx < m_blockWidth; ++dx) {
                    int px = blockX * m_blockWidth + dx;
                    int py = blockY * m_blockHeight + dy;

                    // Boundary check (needed for edge cases on non-divisible resolutions)
                    if (px >= m_srcWidth || py >= m_srcHeight)
                        continue;

                    // Compute pointer to pixel in RGB frame
                    uint8_t* pixel = m_rgbFrame->data[0] + py * m_rgbFrame->linesize[0] + px * 3;
                    uint8_t r = pixel[0];
                    uint8_t g = pixel[1];
                    uint8_t b = pixel[2];

                    // Approximate luminance = 0.299R + 0.587G + 0.114B
                    int brightness = (r * 299 + g * 587 + b * 114) / 1000;

                    // Accumulate for averaging
                    rSum += r;
                    gSum += g;
                    bSum += b;
                    brightnessSum += brightness;
                    count++;
                }
            }

            // Compute average brightness and colour, and assign to grid
            if (count > 0) {
                int avgBrightness = static_cast<int>(std::round(static_cast<double>(brightnessSum) / count));
                int index = (avgBrightness * (m_asciiChars.size() - 1)) / 255;

                outGrid.chars[blockY][blockX] = m_asciiChars[index];
                if(enableColor) {
                    outGrid.colours[blockY][blockX] = RGB{
                        static_cast<uint8_t>(rSum / count),
                        static_cast<uint8_t>(gSum / count),
                        static_cast<uint8_t>(bSum / count)
                    }; // set average red, green and blue colours for the block
                } else {
                    outGrid.colours[blockY][blockX] = RGB{255, 255, 255};
                }
            } else {
                // safety fallback: empty cell
                outGrid.chars[blockY][blockX] = ' ';
                outGrid.colours[blockY][blockX] = RGB{0, 0, 0};
            }
        }
    }
}

} // namespace AsciiVideoFilter
