#include "Application.hpp"

#include "VideoDecoder.hpp"
#include "VideoEncoder.hpp"
#include "AsciiConverter.hpp"
#include "AsciiRenderer.hpp"

extern "C" {
    #include <libavutil/frame.h>
    #include <libavcodec/packet.h>
}

#include <iostream>

namespace AsciiVideoFilter {

Application::Application() {}
Application::~Application() {}

int Application::run(int argc, const char *argv[]) {
    // TODO: Better argument parsing. Current one very rudimentary
    // TODO: AppErrorCodes aren't setup right in recent parts of the codebase. Fix soon
    // TODO: Actual Multithreaded Pipeline.

    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_file> <output_file>\n";
        return 1;
    }

    const std::string inputPath = argv[1];
    const std::string outputPath = argv[2];

    VideoDecoder decoder;
    if (decoder.open(inputPath) < 0) {
        std::cerr << "Failed to open input video.\n";
        return 1;
    }

    VideoEncoder encoder;
    if (encoder.init(outputPath, decoder.getMetadata(), decoder.getWidth(), decoder.getHeight(), 400000) < 0) {
        std::cerr << "Failed to initialize video encoder.\n";
        return 1;
    }

    if (decoder.hasAudio()) {
        encoder.addAudioStreamFrom(decoder.getAudioStream());
    }

    AsciiConverter converter;
    converter.init(decoder.getWidth(), decoder.getHeight(), decoder.getPixelFormat());
    AsciiRenderer renderer;
    
    AVFrame* inFrame = av_frame_alloc();
    if (!inFrame) {
        std::cerr << "Failed to allocate input frame.\n";
        return 1;
    }

    // renderer isn't setup right

    while (decoder.readFrame(inFrame)) {
        AsciiGrid grid = converter.convert(inFrame);
        AVFrame* renderedFrame = renderer.render(grid);

        if (!renderedFrame) {
            std::cerr << "Rendering failed.\n";
            break;
        }

        if (encoder.encodeFrame(renderedFrame) < 0) {
            std::cerr << "Encoding frame failed.\n";
            break;
        }

        av_frame_unref(inFrame);
    }

    encoder.finalize();

    if (decoder.hasAudio()) {
        AVPacket* pkt = av_packet_alloc();
        if (!pkt) {
            std::cerr << "Failed to allocate audio packet.\n";
        } else {
            while (decoder.readNextAudioPacket(pkt)) {
                encoder.writeAudioPacket(pkt);
                av_packet_unref(pkt);
            }
            av_packet_free(&pkt);
        }
    }

    av_frame_free(&inFrame);
    return 0;
}

} // namespace AsciiVideoFilter
