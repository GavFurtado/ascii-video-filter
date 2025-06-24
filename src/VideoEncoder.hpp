#pragma once

#include <string>
#include "Utils.hpp"

extern "C" {
    #include <libavutil/error.h>
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/frame.h>
    #include <libavutil/opt.h>
    #include <libswscale/swscale.h>
}

namespace AsciiVideoFilter {

/**
 * @class VideoEncoder
 * @brief Encodes RGB frames to MP4 video format using H.264 codec.
 *
 * Takes RGB24 frames (typically from AsciiRenderer) and encodes them into
 * an MP4 container with H.264 compression, preserving original video timing.
 */
class VideoEncoder {
public:
    /**
     * @brief Constructs an uninitialized VideoEncoder.
     */
    VideoEncoder();

    /**
     * @brief Cleans up all FFmpeg resources.
     */
    ~VideoEncoder();

    /**
     * @brief Initializes the encoder with output file and video parameters.
     *
     * @param outputPath Path to output MP4 file.
     * @param metadata Video metadata from the source (fps, duration, etc.).
     * @param width Output video width in pixels.
     * @param height Output video height in pixels.
     * @param bitrate Target bitrate in bits per second (default: 2Mbps).
     * @return 0 on success, or negative FFmpeg/AppErrorCode on failure.
     */
    int init(const std::string& outputPath, const VideoMetadata& metadata,
             int width, int height, int64_t bitrate = 2000000);

    /**
     * @brief Adds an audio stream to the output file by copying parameters from the input stream.
     * This allows audio to be remuxed without decoding or re-encoding.
     * @param inputAudioStream Pointer to the input AVStream containing audio metadata.
     * @return 0 on success, a negative AVERROR code on failure.
     */
    int addAudioStreamFrom(AVStream* inputAudioStream);

    /**
     * @brief Writes a compressed audio packet directly to the output file.
     * Assumes the audio stream has already been added via addAudioStreamFrom().
     * @param pkt Pointer to an AVPacket containing encoded audio data.
     * @return 0 on success, a negative AVERROR code on failure.
     */
    int writeAudioPacket(AVPacket* pkt);

    /**
     * @brief Encodes a single RGB24 frame.
     *
     * @param frame RGB24 AVFrame to encode (typically from AsciiRenderer; it better be).
     * @return 0 on success, or negative error code on failure.
     */
    int encodeFrame(AVFrame* frame);

    /**
     * @brief Finalizes encoding and writes file trailer.
     *
     * Must be called after all frames are encoded to properly close the file.
     * @return 0 on success, or negative error code on failure.
     */
    int finalize();

private:
    char m_errbuf[AV_ERROR_MAX_STRING_SIZE];
    // FFmpeg encoding contexts
    AVFormatContext* m_formatContext;
    AVCodecContext* m_codecContext;
    AVStream* m_videoStream;
    AVPacket* m_packet;

    // Color space conversion (RGB24 -> YUV420P for H.264)
    SwsContext* m_swsContext;
    AVFrame* m_yuvFrame;
    uint8_t* m_yuvBuffer;

    // Encoding parameters
    int m_width;
    int m_height;
    AVRational m_timeBase;
    int64_t m_frameCount;

    AVStream* m_audioStream = nullptr;

    bool m_hasAudio = false;

    AVStream* m_outputVideoStream; // Pointer to the video stream in m_formatContext (output)
    AVStream* m_outputAudioStream; // Pointer to the audio stream in m_formatContext (output)
    int m_outputVideoCodecId;      // Codec ID of the output video stream
    int m_outputAudioCodecId;      // Codec ID of the output audio stream
    int m_outputVideoStreamIndex;  // Index of the video stream in the output format context
    int m_outputAudioStreamIndex;  // Index of the audio stream in the output format context

    /**
     * @brief Writes encoded packet to output file.
     */
    int writePacket(AVPacket* packet);

    /**
     * @brief Cleans up all allocated resources.
     */
    void cleanup();

    // Disable copying
    VideoEncoder(const VideoEncoder&) = delete;
    VideoEncoder& operator=(const VideoEncoder&) = delete;
};

} // namespace AsciiVideoFilter
