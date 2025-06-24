#pragma once

#include "Utils.hpp"

#include <libavcodec/codec.h>
#include <libavutil/error.h>
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

    /**
     * @brief Reads the next packet from the input audio stream.
     * @param outPacket Packet to be filled with raw (compressed) audio data.
     * @return true if a packet was read successfully, false if end of stream or failure.
     */
    bool readNextAudioPacket(AVPacket* outPacket);

    // Getters for video stream properties. Returns 0 or AV_PIX_FMT_NONE if context is not open.
    int getWidth() const { return m_codecContext ? m_codecContext->width : 0; }
    int getHeight() const { return m_codecContext ? m_codecContext->height : 0; }
    AVPixelFormat getPixelFormat() const { return m_codecContext ? m_codecContext->pix_fmt : AV_PIX_FMT_NONE; }
    AVRational getTimeBase() const { return m_formatContext && m_videoStreamIndex != -1 ? m_formatContext->streams[m_videoStreamIndex]->time_base : av_make_q(0, 1); }
    int getAudioStreamIndex() const { return m_audioStreamIndex; }
    AVStream* getAudioStream() const { return m_audioStream; }

    bool hasAudio() const { return m_audioStreamIndex != -1; }


    /**
     * @brief Gets comprehensive video metadata for encoding.
     * @return VideoMetadata struct with all timing and format info.
     */
    VideoMetadata getMetadata() const { return m_metadata; }

private:
    char m_errbuf[AV_ERROR_MAX_STRING_SIZE];
    // FFmpeg contexts and pointers
    AVFormatContext *m_formatContext;
    AVCodecContext *m_codecContext;
    AVPacket *m_packet;
    int m_videoStreamIndex;

    VideoMetadata m_metadata; ///< Cached metadata populated during open()

    // audio stream for remuxing into the output
    AVStream *m_audioStream = nullptr;
    int m_audioStreamIndex = -1;

    // Private helpers
    // cleans up resources (called by destructor and on error in open())
    void cleanup();
    // populates m_metadata (called by open())
    void populateMetadata();

    VideoDecoder(const VideoDecoder&) = delete; // Disable copy constructor
    VideoDecoder& operator=(const VideoDecoder&) = delete; // Disable operator= overload
};

} // namespace AsciiVideoFilter
