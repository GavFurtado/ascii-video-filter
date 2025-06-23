#include "VideoDecoder.hpp"
#include "Utils.hpp" // AppErrorCode
#include <iostream>

extern "C" {
    #include <libavutil/avutil.h>     // av_err2str, av_log_set_level
    #include <libavutil/error.h>      // AVERROR macro
    #include <libavutil/pixdesc.h>      // av_get_pix_fmt_name()
}

namespace AsciiVideoFilter {

VideoDecoder::VideoDecoder()
    : m_formatContext(nullptr),
      m_codecContext(nullptr),
      m_packet(nullptr),
      m_videoStreamIndex(-1)
{
    // Constructor initializes pointers only
    // Actual FFmpeg setup is in 'open', which needs to be explicitly called
}

VideoDecoder::~VideoDecoder() {
    cleanup();
}

void VideoDecoder::cleanup() {
    if (m_packet) {
        av_packet_free(&m_packet);
        m_packet = nullptr;
    }
    if (m_codecContext) {
        avcodec_free_context(&m_codecContext);
        m_codecContext = nullptr;
    }
    if (m_formatContext) {
        avformat_close_input(&m_formatContext); // close file and free m_formatContext
        m_formatContext = nullptr;
    }
}

int VideoDecoder::open(const std::string& filename) {
    int ret;

    // Ensure state is clean before attempting to open a new file
    cleanup();

    // 1. Open input file
    ret = avformat_open_input(&m_formatContext, filename.c_str(), nullptr, nullptr);
    if (ret < 0) {
        std::cerr << "Error (VideoDecoder::open): Could not open input file '" << filename << "': " << av_err2str(ret) << "\n";
        cleanup();
        return ret; // FFmpeg error code
    }

    // 2. Find stream information
    ret = avformat_find_stream_info(m_formatContext, nullptr);
    if (ret < 0) {
        std::cerr << "Error (VideoDecoder::open): Could not find stream information: " << av_err2str(ret) << "\n";
        cleanup();
        return ret;
    }

    // 3. Find the Best Video Stream
    m_videoStreamIndex = av_find_best_stream(m_formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (m_videoStreamIndex < 0) {
        std::cerr << "Error (VideoDecoder::open): No video stream found in the input file.\n";
        cleanup();
        // Use custom error code when FFmpeg doesn't provide a specific 'int' error, or generic search fails.
        return static_cast<int>(AppErrorCode::APP_ERR_DECODER_NOT_FOUND);
    }

    // 4. Find and Open Decoder
    AVCodecParameters *codec_params = m_formatContext->streams[m_videoStreamIndex]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codec_params->codec_id);
    if (!codec) {
        std::cerr << "Error (VideoDecoder::open): Unsupported codec or decoder not found.\n";
        cleanup();
        // Use custom error code
        return static_cast<int>(AppErrorCode::APP_ERR_DECODER_NOT_FOUND);
    }

    m_codecContext = avcodec_alloc_context3(codec);
    if (!m_codecContext) {
        std::cerr << "Error (VideoDecoder::open): Could not allocate codec context: " << av_err2str(AVERROR(ENOMEM)) << "\n";
        cleanup();
        return AVERROR(ENOMEM); // FFmpeg memory error
    }

    ret = avcodec_parameters_to_context(m_codecContext, codec_params);
    if (ret < 0) {
        std::cerr << "Error (VideoDecoder::open): Could not copy codec parameters to context: " << av_err2str(ret) << "\n";
        cleanup();
        return ret;
    }

    ret = avcodec_open2(m_codecContext, codec, nullptr);
    if (ret < 0) {
        std::cerr << "Error (VideoDecoder::open): Could not open codec: " << av_err2str(ret) << "\n";
        cleanup();
        return ret;
    }

    // 5. Allocate AVPacket for reading data
    m_packet = av_packet_alloc();
    if (!m_packet) {
        std::cerr << "Error (VideoDecoder::open): Could not allocate AVPacket: " << av_err2str(AVERROR(ENOMEM)) << "\n";
        cleanup();
        return AVERROR(ENOMEM); // FFmpeg memory error
    }

    const char* pix_fmt_name = av_get_pix_fmt_name(m_codecContext->pix_fmt);
    std::cout << "VideoDecoder opened: " << filename
              << ", Resolution: " << m_codecContext->width << "x" << m_codecContext->height
              << ", Pixel Format: " << (pix_fmt_name ? pix_fmt_name : "unknown") << "\n";

    return static_cast<int>(AppErrorCode::APP_ERR_SUCCESS); // Indicate success using our enum
}

bool VideoDecoder::readFrame(AVFrame* out_frame) {
    // Decoder should've already set all these fields if it worked correctly
    if (!m_formatContext || !m_codecContext || !m_packet || !out_frame) {
        std::cerr << "Error (VideoDecoder::readFrame): Decoder not properly initialized.\n";
        return false; // Indicate failure
    }

    int ret;
    // Loop to ensure we get a decoded frame, even if we read multiple packets
    while (true) {
        // NOTE: for the first frame this is always EAGAIN because no packets have been sent to the decoder yet
        ret = avcodec_receive_frame(m_codecContext, out_frame); 
        if (ret == 0) {
            // Frame successfully received
            return true;
        } else if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            // Decoder needs more packets (EAGAIN) or has flushed all frames (EOF)

            // Read a new packet from the input file (av_read_frame() is a misleading function name)
            ret = av_read_frame(m_formatContext, m_packet);
            if (ret < 0) {
                if (ret != AVERROR_EOF) {
                    std::cerr << "Error (VideoDecoder::readFrame): Error reading packet: " << av_err2str(ret) << "\n";
                }
                // Send a flush packet to the decoder to get any remaining frames
                avcodec_send_packet(m_codecContext, nullptr); // Send null packet to flush
                av_packet_unref(m_packet); // Clear the packet data
                continue; // Try to receive flushed frames
            }

            // If the packet is from the video stream, send it for decoding
            if (m_packet->stream_index == m_videoStreamIndex) {
                ret = avcodec_send_packet(m_codecContext, m_packet);
                if (ret < 0) {
                    std::cerr << "Error (VideoDecoder::readFrame): Error sending packet to decoder: " << av_err2str(ret) << "\n";
                    av_packet_unref(m_packet);
                    return false; // Error, cannot proceed
                }
            }
            av_packet_unref(m_packet); // Packet data is consumed, unreference it
        } else {
            std::cerr << "Error (VideoDecoder::readFrame): Error receiving frame from decoder: " << av_err2str(ret) << "\n";
            return false; // Critical error
        }
    }
}

} // namespace AsciiVideoFilter
