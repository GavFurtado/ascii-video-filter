#include "VideoEncoder.hpp"
#include <iostream>
#include <cstring>

extern "C" {
    #include <libavutil/avutil.h>
    #include <libavutil/error.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/mem.h>
}

namespace AsciiVideoFilter {

VideoEncoder::VideoEncoder()
    : m_formatContext(nullptr),
      m_codecContext(nullptr),
      m_videoStream(nullptr),
      m_packet(nullptr),
      m_swsContext(nullptr),
      m_yuvFrame(nullptr),
      m_yuvBuffer(nullptr),
      m_width(0),
      m_height(0),
      m_timeBase({0, 1}),
      m_frameCount(0)
{}

VideoEncoder::~VideoEncoder() {
    cleanup();
}

void VideoEncoder::cleanup() {
    // Free YUV conversion resources
    if (m_yuvBuffer) {
        av_free(m_yuvBuffer);
        m_yuvBuffer = nullptr;
    }
    if (m_yuvFrame) {
        av_frame_free(&m_yuvFrame);
        m_yuvFrame = nullptr;
    }
    if (m_swsContext) {
        sws_freeContext(m_swsContext);
        m_swsContext = nullptr;
    }
    
    // Free encoding resources
    if (m_packet) {
        av_packet_free(&m_packet);
        m_packet = nullptr;
    }
    if (m_codecContext) {
        avcodec_free_context(&m_codecContext);
        m_codecContext = nullptr;
    }
    if (m_formatContext) {
        if (m_formatContext->pb) {
            avio_closep(&m_formatContext->pb);
        }
        avformat_free_context(m_formatContext);
        m_formatContext = nullptr;
    }

    m_videoStream = nullptr; // Freed with format context
    m_frameCount = 0;

    m_audioStream = nullptr;
}

int VideoEncoder::init(const std::string& outputPath, const VideoMetadata& metadata,
                       int width, int height, int64_t bitrate) {
    cleanup();

    m_width = width;
    m_height = height;
    m_timeBase = metadata.timeBase;

    int ret;

    // 1. Allocate output format context
    ret = avformat_alloc_output_context2(&m_formatContext, nullptr, "mp4", outputPath.c_str());
    if (ret < 0 || !m_formatContext) {
        std::cerr << "Error (VideoEncoder::init): Could not create output context: " << av_make_error_string(m_errbuf, AV_ERROR_MAX_STRING_SIZE, ret) << "\n";
        cleanup();
        return ret; // ffmpeg err code
    }

    // 2. Find H.264 encoder
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        std::cerr << "Error (VideoEncoder::init): H.264 encoder not found.\n";
        cleanup();
        return static_cast<int>(AppErrorCode::APP_ERR_DECODER_NOT_FOUND);
    }

    // 3. Create video stream
    m_videoStream = avformat_new_stream(m_formatContext, nullptr);
    if (!m_videoStream) {
        std::cerr << "Error (VideoEncoder::init): Could not create video stream.\n";
        cleanup();
        return AVERROR(ENOMEM);
    }
    m_videoStream->id = m_formatContext->nb_streams - 1;
    
    // 4. Allocate codec context
    m_codecContext = avcodec_alloc_context3(codec);
    if (!m_codecContext) {
        std::cerr << "Error (VideoEncoder::init): Could not allocate codec context.\n";
        cleanup();
        return AVERROR(ENOMEM);
    }

    // 5. Configure codec parameters
    m_codecContext->codec_id = AV_CODEC_ID_H264;
    m_codecContext->bit_rate = bitrate;
    m_codecContext->width = m_width;
    m_codecContext->height = m_height;
    m_codecContext->time_base = m_timeBase;
    m_codecContext->framerate = av_inv_q(m_timeBase); // fps = 1/timebase
    m_codecContext->pix_fmt = AV_PIX_FMT_YUV420P; // H.264 standard format
    m_codecContext->gop_size = 12; // Keyframe interval
    m_codecContext->max_b_frames = 1;

    // Set H.264 preset for good compression/speed balance
    av_opt_set(m_codecContext->priv_data, "preset", "medium", 0);
    av_opt_set(m_codecContext->priv_data, "crf", "23", 0); // Constant Rate Factor
    
    // Some formats want stream headers to be separate
    if (m_formatContext->oformat->flags & AVFMT_GLOBALHEADER) {
        m_codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    
    // 6. Open codec
    ret = avcodec_open2(m_codecContext, codec, nullptr);
    if (ret < 0) {
        std::cerr << "Error (VideoEncoder::init): Could not open codec: " << av_make_error_string(m_errbuf, AV_ERROR_MAX_STRING_SIZE, ret) << "\n";
        cleanup();
        return ret;
    }
    
    // 7. Copy codec parameters to stream
    ret = avcodec_parameters_from_context(m_videoStream->codecpar, m_codecContext);
    if (ret < 0) {
        std::cerr << "Error (VideoEncoder::init): Could not copy codec parameters: " << av_make_error_string(m_errbuf, AV_ERROR_MAX_STRING_SIZE, ret) << "\n";
        cleanup();
        return ret;
    }
    
    // 8. Open output file
    ret = avio_open(&m_formatContext->pb, outputPath.c_str(), AVIO_FLAG_WRITE);
    if (ret < 0) {
        std::cerr << "Error (VideoEncoder::init): Could not open output file '" << outputPath << "': " << av_make_error_string(m_errbuf, AV_ERROR_MAX_STRING_SIZE, ret) << "\n";
        cleanup();
        return ret;
    }
    
    // 9. Write file header
    ret = avformat_write_header(m_formatContext, nullptr);
    if (ret < 0) {
        std::cerr << "Error (VideoEncoder::init): Error writing header: " << av_make_error_string(m_errbuf, AV_ERROR_MAX_STRING_SIZE, ret) << "\n";
        cleanup();
        return ret;
    }
    
    // 10. Set up color space conversion (RGB24 -> YUV420P)
    m_swsContext = sws_getContext(m_width, m_height, AV_PIX_FMT_RGB24,
                                  m_width, m_height, AV_PIX_FMT_YUV420P,
                                  SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!m_swsContext) {
        std::cerr << "Error (VideoEncoder::init): Could not initialize color conversion context.\n";
        cleanup();
        return static_cast<int>(AppErrorCode::APP_ERR_CONVERTER_INIT_FAILED);
    }
    
    // 11. Allocate YUV frame and buffer
    m_yuvFrame = av_frame_alloc();
    if (!m_yuvFrame) {
        std::cerr << "Error (VideoEncoder::init): Could not allocate YUV frame.\n";
        cleanup();
        return AVERROR(ENOMEM);
    }
    
    m_yuvFrame->format = AV_PIX_FMT_YUV420P;
    m_yuvFrame->width = m_width;
    m_yuvFrame->height = m_height;
    
    int yuvBufferSize = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, m_width, m_height, 32);
    m_yuvBuffer = static_cast<uint8_t*>(av_malloc(yuvBufferSize));
    if (!m_yuvBuffer) {
        std::cerr << "Error (VideoEncoder::init): Could not allocate YUV buffer.\n";
        cleanup();
        return AVERROR(ENOMEM);
    }
    
    ret = av_image_fill_arrays(m_yuvFrame->data, m_yuvFrame->linesize, m_yuvBuffer,
                               AV_PIX_FMT_YUV420P, m_width, m_height, 32);
    if (ret < 0) {
        std::cerr << "Error (VideoEncoder::init): Could not setup YUV frame arrays.\n";
        cleanup();
        return ret;
    }
    
    // 12. Allocate packet
    m_packet = av_packet_alloc();
    if (!m_packet) {
        std::cerr << "Error (VideoEncoder::init): Could not allocate packet.\n";
        cleanup();
        return AVERROR(ENOMEM);
    }
    
    std::cout << "VideoEncoder initialized: " << outputPath 
              << ", " << m_width << "x" << m_height 
              << ", " << bitrate/1000 << "kbps"
              << ", " << av_q2d(av_inv_q(m_timeBase)) << "fps\n";
    
    return static_cast<int>(AppErrorCode::APP_ERR_SUCCESS);
}

int VideoEncoder::addAudioStreamFrom(AVStream* inputAudioStream) {
    if (!m_formatContext || !inputAudioStream) {
        return AVERROR(EINVAL);
    }

    m_audioStream = avformat_new_stream(m_formatContext, nullptr);
    if (!m_audioStream) {
        std::cerr << "Error: Could not create audio stream in output.\n";
        return AVERROR(ENOMEM);
    }

    int ret = avcodec_parameters_copy(m_audioStream->codecpar, inputAudioStream->codecpar);
    if (ret < 0) {
        std::cerr << "Error: Failed to copy audio stream parameters.\n";
        return ret;
    }

    m_audioStream->codecpar->codec_tag = 0;
    m_audioStream->time_base = inputAudioStream->time_base;
    m_audioStream->id = m_formatContext->nb_streams - 1;

    return 0;
}

int VideoEncoder::writeAudioPacket(AVPacket* pkt) {
    if (!m_audioStream || !pkt) return AVERROR(EINVAL);

    // Rescale timestamps to match the output stream's time_base
    av_packet_rescale_ts(pkt,
                         m_formatContext->streams[pkt->stream_index]->time_base,
                         m_audioStream->time_base);

    pkt->stream_index = m_audioStream->index;

    int ret = av_interleaved_write_frame(m_formatContext, pkt);
    if (ret < 0) {
        std::cerr << "Error: Failed to write audio packet: " << av_make_error_string(m_errbuf, AV_ERROR_MAX_STRING_SIZE, ret) << "\n";
    }
    return ret;
}

int VideoEncoder::encodeFrame(AVFrame* frame) {
    if (!m_codecContext || !m_yuvFrame || !frame) {
        std::cerr << "Error (VideoEncoder::encodeFrame): Encoder not initialized.\n";
        return static_cast<int>(AppErrorCode::APP_ERR_CONVERTER_INIT_FAILED);
    }
    
    // Convert RGB24 to YUV420P
    sws_scale(m_swsContext, frame->data, frame->linesize, 0, frame->height,
              m_yuvFrame->data, m_yuvFrame->linesize);
    
    // Set frame timing
    m_yuvFrame->pts = m_frameCount++;
    
    // Send frame to encoder
    int ret = avcodec_send_frame(m_codecContext, m_yuvFrame);
    if (ret < 0) {
        std::cerr << "Error (VideoEncoder::encodeFrame): Error sending frame to encoder: " << av_make_error_string(m_errbuf, AV_ERROR_MAX_STRING_SIZE, ret) << "\n";
        return ret;
    }
    
    // Receive encoded packets
    while (ret >= 0) {
        ret = avcodec_receive_packet(m_codecContext, m_packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break; // Need more frames or done
        } else if (ret < 0) {
            std::cerr << "Error (VideoEncoder::encodeFrame): Error receiving packet: " << av_make_error_string(m_errbuf, AV_ERROR_MAX_STRING_SIZE, ret) << "\n";
            return ret;
        }
        
        ret = writePacket(m_packet);
        if (ret < 0) {
            return ret;
        }
        
        av_packet_unref(m_packet);
    }
    
    return static_cast<int>(AppErrorCode::APP_ERR_SUCCESS);
}

int VideoEncoder::finalize() {
    if (!m_codecContext || !m_formatContext) {
        return static_cast<int>(AppErrorCode::APP_ERR_CONVERTER_INIT_FAILED);
    }
    
    // Flush encoder
    int ret = avcodec_send_frame(m_codecContext, nullptr);
    if (ret < 0) {
        std::cerr << "Error (VideoEncoder::finalize): Error flushing encoder: " << av_make_error_string(m_errbuf, AV_ERROR_MAX_STRING_SIZE, ret) << "\n";
        return ret;
    }
    
    // Receive remaining packets
    while (ret >= 0) {
        ret = avcodec_receive_packet(m_codecContext, m_packet);
        if (ret == AVERROR_EOF) {
            break; // Done
        } else if (ret < 0) {
            std::cerr << "Error (VideoEncoder::finalize): Error receiving final packets: " << av_make_error_string(m_errbuf, AV_ERROR_MAX_STRING_SIZE, ret) << "\n";
            return ret;
        }
        
        ret = writePacket(m_packet);
        if (ret < 0) {
            return ret;
        }
        
        av_packet_unref(m_packet);
    }
    
    // Write file trailer
    ret = av_write_trailer(m_formatContext);
    if (ret < 0) {
        std::cerr << "Error (VideoEncoder::finalize): Error writing trailer: " << av_make_error_string(m_errbuf, AV_ERROR_MAX_STRING_SIZE, ret) << "\n";
        return ret;
    }
    
    std::cout << "Encoding completed. Total frames: " << m_frameCount << "\n";
    return static_cast<int>(AppErrorCode::APP_ERR_SUCCESS);
}

int VideoEncoder::writePacket(AVPacket* packet) {
    // Rescale packet timestamps to stream timebase
    av_packet_rescale_ts(packet, m_codecContext->time_base, m_videoStream->time_base);
    packet->stream_index = m_videoStream->index;
    
    // Write packet to output file
    int ret = av_interleaved_write_frame(m_formatContext, packet);
    if (ret < 0) {
        std::cerr << "Error (VideoEncoder::writePacket): Error writing packet: " << av_make_error_string(m_errbuf, AV_ERROR_MAX_STRING_SIZE, ret) << "\n";
        return ret;
    }
    
    return 0;
}

} // namespace AsciiVideoFilter
