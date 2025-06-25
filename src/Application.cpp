#include "Application.hpp"
#include "VideoDecoder.hpp"
#include "VideoEncoder.hpp"
#include "AsciiConverter.hpp"
#include "AsciiRenderer.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <cstdint>
#include <libavutil/log.h>
#include <string>
#include <unordered_map>
#include <iostream>

extern "C" {
    #include <libavutil/frame.h>
    #include <libavcodec/packet.h>
}

namespace AsciiVideoFilter {

Application::Application() {}
Application::~Application() {}

int Application::run(int argc, const char *argv[]) {
    // TODO: Better argument parsing. Current one very rudimentary
    // TODO: AppErrorCodes aren't setup right in recent parts of the codebase. Fix soon
    // TODO: Actual Multithreaded Pipeline.

    AppConfig config = Utils::parseArguments(argc, argv);

    av_log_set_level(AV_LOG_PANIC);

    if(config.verbose) {
        Utils::printConfig(config);
        av_log_set_level(AV_LOG_VERBOSE);
        #ifdef DEBUG
            av_log_set_level(AV_LOG_DEBUG);
        #endif // DEBUG
    }

    const std::unordered_map<std::string, std::string> charPresets = {
        {"standard", " .:-=+*#%@"},
        {"detailed", " .'`^,:;Il!i><~+_-?][}{1)(|\\/tfjrxnumbroCLJVUNYXOZmwqpdbkhao*#MW&8%B@$"},
        {"binary", " 01 "}
        // {"dots", " .∙⬤⦿☉○●"}, // std::string doesn't do unicode so needs changes to work
        // {"shapes", " ▫▪▩▨▧▦▥▤▣▢□■"}
    };

    // Determine charset to use
    std::string charset = config.customCharset.empty() ?
                        charPresets.at(config.charsetPreset) :
                        config.customCharset;


    VideoDecoder decoder;
    if (decoder.open(config.inputPath) < 0) {
        std::cerr << "Failed to open input video.\n";
        return 1;
    }

    int videoWidth = decoder.getWidth();
    int videoHeight = decoder.getHeight();
    double frameRate = decoder.getMetadata().getFps();

    int64_t totalFrames = config.maxFrames == -1 ? decoder.getMetadata().getTotalFrames() :
                     std::min<int64_t>(config.maxFrames, decoder.getMetadata().getTotalFrames());

    ProgressTracker progress(totalFrames, frameRate, config.progressInterval, config.showProgress);

    AsciiConverter converter;
    converter.setAsciiCharset(charset);
    converter.init(videoWidth, videoHeight, decoder.getPixelFormat(), config.blockWidth, config.blockHeight);

    AVFrame* inFrame = av_frame_alloc();
    if (!inFrame) {
        std::cerr << "Failed to allocate input frame.\n";
        return 1;
    }

    AsciiRenderer renderer;
    // Initialize AsciiRenderer's font
    if (renderer.initFont(config.fontPath, converter.getBlockHeight()) < 0) {
        std::cerr << "Error: Failed to initialize ASCII renderer font. Exiting.\\n";
        return static_cast<int>(AppErrorCode::APP_ERR_FONT_INIT_FAILED); 
    }

    renderer.initFrame(videoWidth, videoHeight, converter.getBlockWidth(), converter.getBlockHeight());

    VideoEncoder encoder;
    if (encoder.init(config.outputPath, decoder.getMetadata(), videoWidth, videoHeight, 400000) < 0) {
        std::cerr << "Failed to initialize video encoder.\n";
        return 1;
    }

    if (config.enableAudio && decoder.hasAudio()) {
        encoder.addAudioStreamFrom(decoder.getAudioStream());
    }

    AsciiGrid grid;
    grid.cols = converter.getGridCols();
    grid.rows = converter.getGridRows();
    grid.chars.assign(grid.rows, std::vector<char>(grid.cols));
    grid.colours.assign(grid.rows, std::vector<RGB>(grid.cols));

    int64_t frameCount = 0;
    while (decoder.readFrame(inFrame) && (config.maxFrames == -1 || frameCount < config.maxFrames)) {
        converter.convert(inFrame, grid);

        AVFrame* renderedFrame = renderer.render(grid, config.enableColour);
        if (!renderedFrame) {
            std::cerr << "Rendering failed.\n";
            break;
        }

        if (encoder.encodeFrame(renderedFrame) < 0) {
            std::cerr << "Encoding frame failed.\n";
            break;
        }
        av_frame_unref(inFrame);

        progress.update(frameCount++);
    }
    av_frame_free(&inFrame);


    encoder.finalize();
    progress.finish();

    if(config.verbose) {
        LOG("Video stream rendered and encoded..\n");
    }

    int64_t count = 0;
    // remux the audio stream if exists
    if(config.verbose) {
        std::cout<< "Remuxxing audio stream.\n";
    }
    if (decoder.hasAudio()) {
        AVPacket* pkt = av_packet_alloc();
        if (!pkt) {
            std::cerr << "Failed to allocate audio packet.\n";
            return static_cast<int>(AppErrorCode::APP_ERR_AUDIO_PKT_ALLOC_FAILED);
        } else {
            // Loop for audio packets
            while (decoder.readNextAudioPacket(pkt)) {
                if(config.verbose) {
                    LOG("Audio Packet Loop Counter: %d, PTS: %lld, DTS: %lld, Duration: %lld, Stream Index: %d\n",
                        count, pkt->pts, pkt->dts, pkt->duration, pkt->stream_index);
                }

                if (encoder.writeAudioPacket(pkt) < 0) {
                    std::cerr << "Error writing audio packet. Stopping audio remux.\n";
                    av_packet_unref(pkt);
                    break;
                }
                av_packet_unref(pkt); // Unreference packet after successful write
                count++;
            }
            av_packet_free(&pkt); // Free the packet when done with the loop
        }
    } else {
        std::cout << "No audio stream to remux.\n";
    }
    if(config.verbose) {
        LOG("Audio Stream remuxxed into output file.\n");
    }
    LOG("End\n");
    return 0;
}
} // namespace AsciiVideoFilter
