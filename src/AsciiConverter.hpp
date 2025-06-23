#pragma once

#include <string>

#include "AsciiTypes.hpp"  // Defines RGB and AsciiGrid structures
extern "C" {
    #include <libavutil/frame.h>     ///< AVFrame for decoded frames
    #include <libswscale/swscale.h>  ///< SwsContext for color space conversion
    #include <libavutil/imgutils.h>  ///< av_image_get_buffer_size, av_image_fill_arrays
    #include <libavutil/mem.h>       ///< av_malloc, av_free
    #include <libavutil/avutil.h>    ///< av_err2str
}


namespace AsciiVideoFilter {

/**
 * @class AsciiConverter
 * @brief Converts decoded video frames into a structured ASCII grid representation.
 *
 * Converts AVFrames to RGB24, then samples pixel blocks (group of pixels that makes up a character) and
 * maps their brightness and average color to ASCII characters and RGB triplets.
 */
class AsciiConverter {
public:
    /**
     * @brief Constructs an uninitialized AsciiConverter.
     *
     * You must call init() before using convert().
     */
    AsciiConverter();

    /**
     * @brief Frees any internal resources (frames, buffers, SwsContext).
     */
    ~AsciiConverter();

    /**
     * @brief Initializes the converter with source format and ASCII block size.
     *
     * @param src_width Width of the decoded frame.
     * @param src_height Height of the decoded frame.
     * @param src_pix_fmt Pixel format of the decoded frame (e.g. AV_PIX_FMT_YUV420P).
     * @param ascii_block_width Width of each ASCII block (in pixels).
     * @param ascii_block_height Height of each ASCII block (in pixels).
     * @return 0 on success, or negative FFmpeg/AppErrorCode on failure.
     */
    int init(int srcWidth, int srcHeight, AVPixelFormat src_pix_fmt,
             int asciiBlockWidth = 4, int asciiBlockHeight = 8);

    /**
     * @brief Converts a decoded video frame to an ASCII grid.
     *
     * The frame is first converted to RGB24. Each block of pixels is averaged
     * for brightness and color and mapped to a character.
     *
     * @param decoded_frame A pointer to an AVFrame from the decoder.
     * @return AsciiGrid structure representing the ASCII characters and colors for the frame.
     */
    [[nodiscard]] AsciiGrid convert(AVFrame* decodedFrame);

    /**
     * @brief Sets the ASCII character set used for brightness mapping.
     *
     * The characters should be ordered from darkest (index 0) to brightest (last index).
     *
     * @param charset A non-empty string of characters used to represent brightness levels.
     */
    void setAsciiCharset(const std::string& charset);

private:
    SwsContext *m_swsContext;   ///< Used to convert from input format to RGB24
    AVFrame *m_rgbFrame;        ///< Internal RGB24 frame buffer
    uint8_t *m_rgbBuffer;       ///< Buffer for RGB image data

    int m_srcWidth;             ///< Width of source frame
    int m_srcHeight;            ///< Height of source frame
    int m_blockWidth;           ///< Width of one ASCII character block in pixels
    int m_blockHeight;          ///< Height of one ASCII character block in pixels

    std::string m_asciiChars;   ///< Characters used for brightness-to-ASCII mapping

    /**
     * @brief Frees all allocations and resets members.
     */
    void cleanup();

    // no copy constructor and assignment operator
    AsciiConverter(const AsciiConverter&) = delete;
    AsciiConverter& operator=(const AsciiConverter&) = delete;
};

} // namespace AsciiVideoFilter
