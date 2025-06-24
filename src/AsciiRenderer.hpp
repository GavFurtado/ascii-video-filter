#pragma once

#include <string>
#include "AsciiTypes.hpp"

extern "C" {
    #include <libavutil/error.h>
    #include <libavutil/frame.h>
    #include <libavutil/pixfmt.h>
}

namespace AsciiVideoFilter {

class AsciiRenderer {
public:
    /**
     * @brief Constructs an uninitialized renderer.
     */
    AsciiRenderer();

    /**
     * @brief Frees all resources.
     */
    ~AsciiRenderer();

    /**
     * @brief Loads font from file and prepares stb_truetype.
     *
     * @param fontPath Path to a .ttf file.
     * @param fontHeight Height in pixels for rendering each glyph.
     * @return 0 on success, or negative FFmpeg/AppErrorCode on failure.
     */
    int initFont(const std::string& fontPath, int fontHeight);

    /**
     * @brief Initializes the output AVFrame dimensions and buffer.
     *
     * @param targetFrameWidth The desired pixel width of the output AVFrame.
     * @param targetFrameHeight The desired pixel height of the output AVFrame.
     * @param blockWidth Pixel width of each character cell.
     * @param blockHeight Pixel height of each character cell.
     * @return 0 on success, or negative FFmpeg/AppErrorCode on failure.
     */
    int initFrame(int targetFrameWidth, int targetFrameHeight, int blockWidth, int blockHeight);

    /**
     * @brief Renders the ASCII grid with color to an AVFrame.
     *
     * @param grid AsciiGrid containing characters and RGB values.
     * @return AVFrame* pointing to the internal RGB frame.
     */
    AVFrame* render(const AsciiGrid& grid);

    /**
     * @brief Cleans up allocated frame, font, and buffers.
     */
    void cleanup();


    
private:
    char m_errbuf[AV_ERROR_MAX_STRING_SIZE];
    // Font and glyph
    uint8_t* m_fontBuffer;     ///< Raw font file buffer
    unsigned char* m_bitmap;   ///< Temporary buffer for glyph bitmaps
    void* m_fontInfo;          ///< Opaque pointer to font info (stbtt_fontinfo*)

    float m_scale;             ///< Font scale computed from pixel height
    int m_ascent;              ///< Font ascent in pixels

    // Frame output
    AVFrame* m_frame;          ///< Output RGB frame
    uint8_t* m_frameBuffer;    ///< Buffer backing AVFrame
    int m_frameWidth;          ///< Full frame width (cols * blockWidth)
    int m_frameHeight;         ///< Full frame height (rows * blockHeight)

    int m_blockWidth;          ///< Width of a single glyph block
    int m_blockHeight;         ///< Height of a single glyph block

    bool loadFont(const std::string& path);
    void drawGlyph(char c, int x, int y, RGB color);
};

} // namespace AsciiVideoFilter
