#pragma once

#include <string>

extern "C" {
    #include <libavutil/frame.h>    // AVFrame
    #include <libswscale/swscale.h> // SwsContext
    #include <libavutil/imgutils.h> // av_image_get_buffer_size, av_image_fill_arrays
    #include <libavutil/mem.h>      // av_malloc, av_free
    #include <libavutil/avutil.h>   // av_err2str
}

namespace AsciiVideoFilter {

/**
 * @class AsciiConverter
 * @brief Converts decoded video frames to ASCII art.
 */
class AsciiConverter {
public:
    /**
     * @brief Constructs an uninitialized AsciiConverter.
     * 
     * The actual setup must be done by calling init().
     */
    AsciiConverter();

    /**
     * @brief Destroys the converter and frees associated resources.
     */
    ~AsciiConverter();

    /**
     * @brief Initializes the converter with the source frame format and ASCII block size.
     * 
     * @param src_width Source frame width in pixels.
     * @param src_height Source frame height in pixels.
     * @param src_pix_fmt Source pixel format (e.g., AV_PIX_FMT_YUV420P).
     * @param ascii_block_width Width of each ASCII block (in pixels).
     * @param ascii_block_height Height of each ASCII block (in pixels).
     * @return int 0 (success), or a negative FFmpeg/App error code on failure.
     */
    int init(int src_width, int src_height, AVPixelFormat src_pix_fmt,
             int ascii_block_width = 4, int ascii_block_height = 8);

    /**
     * @brief Converts a decoded video frame to an ASCII art string.
     * 
     * Internally converts the frame to RGB24, samples pixel brightness,
     * and maps it to ASCII characters.
     * 
     * @param decoded_frame pointer to an AVFrame returned from the decoder.
     * @return std::string ASCII art representation of the frame,
     *         or an empty string on error or if not initialized.
     */
    [[nodiscard]] std::string convert(AVFrame* decoded_frame);

private:
    SwsContext *m_swsContext;
    AVFrame *m_rgbFrame;
    uint8_t *m_rgbBuffer;

    int m_srcWidth;
    int m_srcHeight;
    int m_blockWidth;
    int m_blockHeight;
    const std::string m_asciiChars = "@%#*+=-:. ";

    void cleanup();

    // Deleted copy constructor and assignment operator
    AsciiConverter(const AsciiConverter&) = delete;
    AsciiConverter& operator=(const AsciiConverter&) = delete;
};

} // namespace AsciiVideoFilter
