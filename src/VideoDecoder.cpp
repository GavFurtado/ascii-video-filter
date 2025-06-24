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
        std::cerr << "Error (VideoDecoder::open): Could not open input file '" << filename << "': " << av_make_error_string(m_errbuf, AV_ERROR_MAX_STRING_SIZE, ret) << "\n";
        cleanup();
        return ret; // FFmpeg error code
    }

    // 2. Find stream information
    ret = avformat_find_stream_info(m_formatContext, nullptr);
    if (ret < 0) {
        std::cerr << "Error (VideoDecoder::open): Could not find stream information: " << av_make_error_string(m_errbuf, AV_ERROR_MAX_STRING_SIZE, ret) << "\n";
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
        std::cerr << "Error (VideoDecoder::open): Could not allocate codec context: " << av_make_error_string(m_errbuf, AV_ERROR_MAX_STRING_SIZE, AVERROR(ENOMEM)) << "\n";
        cleanup();
        return AVERROR(ENOMEM); // FFmpeg memory error
    }

    ret = avcodec_parameters_to_context(m_codecContext, codec_params);
    if (ret < 0) {
        std::cerr << "Error (VideoDecoder::open): Could not copy codec parameters to context: " << av_make_error_string(m_errbuf, AV_ERROR_MAX_STRING_SIZE, ret) << "\n";
        cleanup();
        return ret;
    }

    ret = avcodec_open2(m_codecContext, codec, nullptr);
    if (ret < 0) {
        std::cerr << "Error (VideoDecoder::open): Could not open codec: " << av_make_error_string(m_errbuf, AV_ERROR_MAX_STRING_SIZE, ret) << "\n";
        cleanup();
        return ret;
    }

    // 5. Allocate AVPacket for reading data
    m_packet = av_packet_alloc();
    if (!m_packet) {
        std::cerr << "Error (VideoDecoder::open): Could not allocate AVPacket: " << av_make_error_string(m_errbuf, AV_ERROR_MAX_STRING_SIZE, AVERROR(ENOMEM)) << "\n";
        cleanup();
        return AVERROR(ENOMEM); // FFmpeg memory error
    }

    const char* pix_fmt_name = av_get_pix_fmt_name(m_codecContext->pix_fmt);
    std::cout << "VideoDecoder opened: " << filename
              << ", Resolution: " << m_codecContext->width << "x" << m_codecContext->height
              << ", Pixel Format: " << (pix_fmt_name ? pix_fmt_name : "unknown") << "\n";

    // populate m_metadata
    populateMetadata();

    // 6. Manually search for audio stream (remuxing audio stream in output)
    for (unsigned i = 0; i < m_formatContext->nb_streams; ++i) {
        AVStream* stream = m_formatContext->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            m_audioStreamIndex = i;
            m_audioStream = stream;
            break;
        }
    }

    return static_cast<int>(AppErrorCode::APP_ERR_SUCCESS); // Indicate success using our enum
}

void VideoDecoder::populateMetadata() {
    if (!m_formatContext || m_videoStreamIndex < 0) {
        return;
    }

    AVStream* stream = m_formatContext->streams[m_videoStreamIndex];

    m_metadata.width = m_codecContext->width;
    m_metadata.height = m_codecContext->height;
    m_metadata.timeBase = stream->time_base;
    m_metadata.frameRate = stream->avg_frame_rate;
    m_metadata.duration = stream->duration;
    m_metadata.bitRate = m_codecContext->bit_rate;

    LOG("DEBUG: Populated metadata.frameRate: %d/%d\n", m_metadata.frameRate.num, m_metadata.frameRate.den);

    // Calculate duration in seconds
    if (m_metadata.duration > 0) {
        m_metadata.durationSeconds = m_metadata.duration * av_q2d(m_metadata.timeBase);
    } else if (m_formatContext->duration > 0) {
        // Fallback to format context duration
        m_metadata.durationSeconds = m_formatContext->duration / static_cast<double>(AV_TIME_BASE);
    }

    std::cout << "Video metadata: " << m_metadata.width << "x" << m_metadata.height
              << ", " << m_metadata.getFps() << "fps"
              << ", " << m_metadata.durationSeconds << "s"
              << ", " << m_metadata.getTotalFrames() << " frames\n";
}

bool VideoDecoder::readNextAudioPacket(AVPacket* outPacket) {
    if (!m_formatContext || m_audioStreamIndex == -1 || !outPacket) {
        return false;
    }

    while (av_read_frame(m_formatContext, outPacket) >= 0) {
        if (outPacket->stream_index == m_audioStreamIndex) {
            return true; // got an audio packet
        }
        av_packet_unref(outPacket); // not audio, discard
    }

    return false; // end of file
}

bool VideoDecoder::readFrame(AVFrame* out_frame) {
    if (!m_formatContext || !m_codecContext || !m_packet || !out_frame) {
        std::cerr << "Error (VideoDecoder::readFrame): Decoder not properly initialized.\n";
        return false; // Indicate failure
    }

    int ret;
    // Flag to indicate if we have reached the end of the input file
    bool reachedEOF = false;

    while (true) {
        // Try to receive a frame from the decoder
        ret = avcodec_receive_frame(m_codecContext, out_frame);
        if (ret == 0) {
            // Frame successfully received
            return true;
        } else if (ret == AVERROR(EAGAIN)) {
            // Decoder needs more packets. Proceed to read more if not at EOF.
            if (reachedEOF) {
                // If we've already sent the flush packet and received EAGAIN,
                // it means there are no more frames to output.
                return false;
            }
        } else if (ret == AVERROR_EOF) {
            // Decoder has flushed all frames. We are done with this stream.
            return false;
        } else {
            // Other error occurred while receiving frame
            std::cerr << "Error (VideoDecoder::readFrame): Error receiving frame from decoder: " << av_make_error_string(m_errbuf, AV_ERROR_MAX_STRING_SIZE, ret) << "\n";
            return false; // Critical error
        }

        // If we reached here, it means avcodec_receive_frame returned EAGAIN
        // (or it was the first call). We need to read more packets.
        // If we've already hit EOF on reading packets, we should flush the decoder.

        if (!reachedEOF) {
            ret = av_read_frame(m_formatContext, m_packet);
            if (ret < 0) {
                if (ret != AVERROR_EOF) {
                    std::cerr << "Error (VideoDecoder::readFrame): Error reading packet: " << av_make_error_string(m_errbuf, AV_ERROR_MAX_STRING_SIZE, ret) << "\n";
                    // If a non-EOF error occurs while reading, we might as well stop.
                    return false;
                }
                // Reached end of file for input packets.
                reachedEOF = true;
                // Send a flush packet (nullptr) to the decoder to get any remaining frames.
                // This is crucial to get out any frames that are delayed within the decoder.
                ret = avcodec_send_packet(m_codecContext, nullptr);
                if (ret < 0) {
                    std::cerr << "Error (VideoDecoder::readFrame): Error sending flush packet to decoder: " << av_make_error_string(m_errbuf, AV_ERROR_MAX_STRING_SIZE, ret) << "\n";
                    return false;
                }
                // After sending flush, go back to try receiving frames.
                continue;
            }

            // If the packet is from the video stream, send it for decoding
            if (m_packet->stream_index == m_videoStreamIndex) {
                ret = avcodec_send_packet(m_codecContext, m_packet);
                if (ret < 0) {
                    std::cerr << "Error (VideoDecoder::readFrame): Error sending packet to decoder: " << av_make_error_string(m_errbuf, AV_ERROR_MAX_STRING_SIZE, ret) << "\n";
                    av_packet_unref(m_packet); // Ensure packet is unreferenced even on error
                    return false; // Error, cannot proceed
                }
            }
            av_packet_unref(m_packet); // Packet data is consumed, unreference it
        } else {
            // If we are already at the end of the file and still in this loop,
            // it means avcodec_receive_frame previously returned EAGAIN
            // after we sent the flush packet. This indicates no more frames will be produced.
            return false;
        }
    }
}

} // namespace AsciiVideoFilter
