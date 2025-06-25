#include "AsciiRenderer.hpp"
#include "Utils.hpp"


#include <fstream>
#include <iostream>
#include <cstring> // for memset
#include <cassert>
#define STB_TRUETYPE_IMPLEMENTATION

extern "C" {
    #include <libavutil/pixdesc.h>
    #include <libavutil/error.h>
    #include "../include/stb_truetype.h"
    #include <libavutil/imgutils.h>
    #include <libavutil/mem.h>
}


namespace AsciiVideoFilter {

AsciiRenderer::AsciiRenderer()
    : m_fontBuffer(nullptr),
      m_bitmap(nullptr),
      m_fontInfo(nullptr),
      m_frame(nullptr),
      m_frameBuffer(nullptr), 
      m_frameWidth(0),
      m_frameHeight(0),
      m_blockWidth(0),
      m_blockHeight(0),
      m_scale(0.0f),
      m_ascent(0)
{}

AsciiRenderer::~AsciiRenderer() {
    cleanup();
}

void AsciiRenderer::cleanup() {
    if (m_fontBuffer) {
        delete[] m_fontBuffer;
        m_fontBuffer = nullptr;
    }
    if (m_bitmap) {
        delete[] m_bitmap;
        m_bitmap = nullptr;
    }
    if (m_fontInfo) {
        delete static_cast<stbtt_fontinfo*>(m_fontInfo);
        m_fontInfo = nullptr;
    }
    if (m_frameBuffer) {
        av_free(m_frameBuffer);
        m_frameBuffer = nullptr;
    }
    if (m_frame) {
        av_frame_free(&m_frame);
        m_frame = nullptr;
    }

    // Free cached glyph bitmaps
    for (auto const& [key, val] : m_glyphCache) {
        stbtt_FreeBitmap(val.bitmap, nullptr);
    }
    m_glyphCache.clear();
}

bool AsciiRenderer::loadFont(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate); // open in binary and seek to EOF
    if (!file) {
        std::cerr << "Error (AsciiRenderer::loadFont): Failed to open font file: " << path << "\n";
        return false;
    }

    std::streamsize size = file.tellg(); // gets full size
    file.seekg(0, std::ios::beg);

    m_fontBuffer = new uint8_t[size];
    if (!file.read(reinterpret_cast<char*>(m_fontBuffer), size)) {
        std::cerr << "Error (AsciiRenderer::loadFont): Failed to read font data.\n";
        delete[] m_fontBuffer;
        m_fontBuffer = nullptr;
        return false;
    }

    m_fontInfo = new stbtt_fontinfo;
    if (!stbtt_InitFont(static_cast<stbtt_fontinfo*>(m_fontInfo), m_fontBuffer, 0)) {
        std::cerr << "Error (AsciiRenderer::loadFont): Failed to initialize stbtt font.\n";
        // If font init fails, cleanup both m_fontInfo and m_fontBuffer
        delete static_cast<stbtt_fontinfo*>(m_fontInfo);
        m_fontInfo = nullptr;
        delete[] m_fontBuffer;
        m_fontBuffer = nullptr;
        return false;
    }

    return true;
}

int AsciiRenderer::initFont(const std::string& fontPath, int fontHeight) {
    // If initFont is called multiple times, clean up existing font resources first
    // This prevents leaks if a new font is loaded.
    if (m_fontBuffer) {
        delete[] m_fontBuffer;
        m_fontBuffer = nullptr;
    }
    if (m_bitmap) {
        delete[] m_bitmap;
        m_bitmap = nullptr;
    }
    if (m_fontInfo) {
        delete static_cast<stbtt_fontinfo*>(m_fontInfo);
        m_fontInfo = nullptr;
    }

    if (!loadFont(fontPath)) {
        return static_cast<int>(AppErrorCode::APP_ERR_FONT_LOAD_FAILED);
    }

    auto* font = static_cast<stbtt_fontinfo*>(m_fontInfo);
    m_scale = stbtt_ScaleForPixelHeight(font, static_cast<float>(fontHeight));
    int descent, lineGap;
    stbtt_GetFontVMetrics(font, &m_ascent, &descent, &lineGap);
    m_ascent = static_cast<int>(m_ascent * m_scale);

    // Allocate space for one glyph at max block size
    // Consider if this allocation should also be checked for failure
    m_bitmap = new unsigned char[fontHeight * fontHeight];
    if (!m_bitmap) {
        std::cerr << "Error (AsciiRenderer::initFont): Failed to allocate glyph bitmap.\n";
        // If m_bitmap allocation fails, clean up font resources that were successfully loaded
        delete static_cast<stbtt_fontinfo*>(m_fontInfo);
        m_fontInfo = nullptr;
        delete[] m_fontBuffer;
        m_fontBuffer = nullptr;
        return AVERROR(ENOMEM);
    }
    return static_cast<int>(AppErrorCode::APP_ERR_SUCCESS);
}

int AsciiRenderer::initFrame(int targetFrameWidth, int targetFrameHeight, int blockWidth, int blockHeight) {
    // cleanup();

    m_blockWidth = blockWidth;
    m_blockHeight = blockHeight;
    m_frameWidth = targetFrameWidth;
    m_frameHeight = targetFrameHeight;

    // Allocate AVFrame structure
    m_frame = av_frame_alloc();
    if (!m_frame) {
        std::cerr << "Error (AsciiRenderer::initFrame) Failed to allocate AVFrame: " << av_make_error_string(m_errbuf, AV_ERROR_MAX_STRING_SIZE, AVERROR(ENOMEM)) << "\n";
        return AVERROR(ENOMEM); // Out of memory
    }

    m_frame->format = AV_PIX_FMT_RGB24;
    m_frame->width = m_frameWidth;
    m_frame->height = m_frameHeight;

    // Get buffer size and allocate it
    int bufferSize = av_image_get_buffer_size(AV_PIX_FMT_RGB24, m_frameWidth, m_frameHeight, 32);
    if (bufferSize < 0) {
        std::cerr << "Invalid image buffer size.\n";
        av_frame_free(&m_frame); // Free m_frame struct on buffer size error
        m_frame = nullptr; // Ensure pointer is nullified
        return bufferSize;
    }

    m_frameBuffer = static_cast<uint8_t*>(av_malloc(bufferSize + 64));
    if (!m_frameBuffer) {
        std::cerr << "Error (AsciiRenderer::initFrame) Failed to allocate AVFrame buffer: " << av_make_error_string(m_errbuf, AV_ERROR_MAX_STRING_SIZE, AVERROR(ENOMEM)) << "\n";
        av_frame_free(&m_frame); 
        m_frame = nullptr; 
        return AVERROR(ENOMEM);
    }

    int ret = av_image_fill_arrays(m_frame->data, m_frame->linesize, m_frameBuffer, AV_PIX_FMT_RGB24,
                                   m_frameWidth, m_frameHeight, 32);
    if (ret < 0) {
        // If av_image_fill_arrays fails, free both m_frameBuffer and m_frame
        av_free(m_frameBuffer); 
        m_frameBuffer = nullptr; 
        av_frame_free(&m_frame); 
        m_frame = nullptr; 
        std::cerr << "Failed to fill AVFrame image arrays.\n";
        return ret;
    }

    std::memset(m_frameBuffer, 0, bufferSize);  // Clear to black
    return static_cast<int>(AppErrorCode::APP_ERR_SUCCESS);
}

AVFrame* AsciiRenderer::render(const AsciiGrid& grid) {
    if (!m_frame || !m_fontInfo) {
        std::cerr << "Renderer not initialized.\n";
        return nullptr;
    }

    // Clear the frame each time before rendering
    int bufferSize = av_image_get_buffer_size(AV_PIX_FMT_RGB24, m_frameWidth, m_frameHeight, 32);
    std::memset(m_frameBuffer, 0, bufferSize);

    for (int row = 0; row < grid.rows; ++row) {
        for (int col = 0; col < grid.cols; ++col) {
            char c = grid.chars[row][col];
            RGB color = grid.colours[row][col];

            int x = col * m_blockWidth;
            int y = row * m_blockHeight + m_ascent;

            drawGlyph(c, x, y, color);
        }
    }

    return m_frame;
}

void AsciiRenderer::drawGlyph(char c, int x, int y, RGB color) {

    assert(c >= 32 && c != 127 && "drawGlyph: character must be printable ASCII (32–126)");
    if (!m_frame || !m_fontInfo)
        return;
    assert(m_frame->format == AV_PIX_FMT_RGB24);

    auto* font = static_cast<stbtt_fontinfo*>(m_fontInfo);

    unsigned char* glyph_bitmap = nullptr;
    int width, height, xoff, yoff;

    auto it = m_glyphCache.find(c);
    if(it != m_glyphCache.end()) {
        // glyph found in cache
        glyph_bitmap = it->second.bitmap;
        width = it->second.width;
        height = it->second.height;
        xoff = it->second.xoff;
        yoff = it->second.yoff;
    } else {
        // glyph not found in cache, generate and cache it 
        // since the char set itself is small enough we can just store all of them without some kinda lru or fifo
        auto *font = static_cast<stbtt_fontinfo*>(m_fontInfo);
        glyph_bitmap = stbtt_GetCodepointBitmap(font, 0, m_scale, c, &width, &height, &xoff, &yoff);
        // assert(glyph_bitmap != nullptr && "Fatal Error: Glyph bitmap could not be generated.");
        if(!glyph_bitmap) return;

        m_glyphCache[c] = {glyph_bitmap, width, height, xoff, yoff}; // cache the glyph 
    }

    for (int gy = 0; gy < height; ++gy) {
        for (int gx = 0; gx < width; ++gx) {
            int dstX = x + gx + xoff;
            int dstY = y + gy + yoff;
            if (dstX < 0 || dstX >= m_frameWidth || dstY < 0 || dstY >= m_frameHeight)
                continue;

            int dstIndex = dstY * m_frame->linesize[0] + dstX * 3;
            float alpha = glyph_bitmap[gy * width + gx] / 255.0f;

            // set color
            m_frame->data[0][dstIndex + 0] = static_cast<uint8_t>(color.r * alpha);
            m_frame->data[0][dstIndex + 1] = static_cast<uint8_t>(color.g * alpha);
            m_frame->data[0][dstIndex + 2] = static_cast<uint8_t>(color.b * alpha);
        }
    }
}
} // namespace AsciiVideoFilter
