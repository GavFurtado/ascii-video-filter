#pragma once

#include <libavcodec/codec.h>
#include <string>

extern "C" {
    #include <libavformat/avformat.h> // AVFormatContext, avformat_open_input, etc.
    #include <libavcodec/avcodec.h>   // AVCodecContext, avcodec_find_decoder, etc.
    #include <libavutil/frame.h>      // AVFrame
    #include <libavcodec/packet.h>     // AVPacket
}

namespace AsciiVideoFilter {

class VideoDecoder {
public:
    /**
     * @brief Constructs a VideoDecoder with all FFmpeg pointers initialized to nullptr.
     */
    VideoDecoder();

    /**
     * @brief Cleans up all FFmpeg-related resources.
     */
    ~VideoDecoder();

    /**
     *  Opens the input video file and prepares streams for decoding.
     *  @param filename The path to the video file
     *  @return 0 (APP_ERR_SUCCESS) on success, or a negative FFmpeg or AppErrorCode on failure.
     */
    int open(const std::string& filename);

    /**
     * Reads and decodes a single video frame.
     * @param *out_frame will contain the decoded raw video data.
     * @return true if a frame was successfully decoded, false if end of stream or no frame yet.
     */
    bool readFrame(AVFrame* out_frame);

    // Getters for video stream properties. Returns 0 or AV_PIX_FMT_NONE if context is not open.
    int getWidth() const { return m_codecContext ? m_codecContext->width : 0; }
    int getHeight() const { return m_codecContext ? m_codecContext->height : 0; }
    AVPixelFormat getPixelFormat() const { return m_codecContext ? m_codecContext->pix_fmt : AV_PIX_FMT_NONE; }
    AVRational getTimeBase() const { return m_formatContext && m_videoStreamIndex != -1 ? m_formatContext->streams[m_videoStreamIndex]->time_base : av_make_q(0, 1); }


private:
    // FFmpeg contexts and pointers
    AVFormatContext *m_formatContext;
    AVCodecContext *m_codecContext;
    AVPacket *m_packet;
    int m_videoStreamIndex;

    // Private helper to clean up resources (called by destructor and on error in open())
    void cleanup();

    VideoDecoder(const VideoDecoder&) = delete; // Disable copy constructor
    VideoDecoder& operator=(const VideoDecoder&) = delete; // Disable operator= overload
};

} // namespace AsciiVideoFilter
