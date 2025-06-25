#include "Application.hpp"
#include "ThreadSafeQueue.hpp"
#include "VideoDecoder.hpp"
#include "VideoEncoder.hpp"
#include "AsciiConverter.hpp"
#include "AsciiRenderer.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <cstdint>
#include <libavutil/log.h>
#include <string>
#include <thread>
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

    // AVFrame* inFrame = av_frame_alloc();
    // if (!inFrame) {
    //     std::cerr << "Failed to allocate input frame.\n";
    //     return 1;
    // }

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

    // Setup thread-safe queues for the 4-stage pipeline
    ThreadSafeQueue<AVFrame*> decodedVideoQueue;
    ThreadSafeQueue<std::unique_ptr<AsciiGrid>> asciiGridQueue; // converter queue
    ThreadSafeQueue<AVFrame*> renderedAsciiFrameQueue;
    ThreadSafeQueue<AVPacket*> decodedAudioPacketQueue;

    // Spin up the threads
    std::thread decoder_thread(decoding_thread_func, &decoder, &decodedVideoQueue,
                               &decodedAudioPacketQueue, config.maxFrames, config.verbose);

    // New conversion thread
    std::thread conversion_thread(conversion_thread_func, &converter, &decodedVideoQueue,
                                  &asciiGridQueue, config.verbose);

    // Modified rendering thread now takes AsciiGrid
    std::thread renderer_thread(rendering_thread_func, &renderer, &asciiGridQueue,
                                &renderedAsciiFrameQueue, config.enableColour, config.verbose);

    std::thread encoder_thread(encoding_thread_func, &encoder,
                               &renderedAsciiFrameQueue, &decodedAudioPacketQueue,
                               config.enableAudio && decoder.hasAudio(), config.verbose, &progress);


    // Wait for each thread to join main thread
    decoder_thread.join();
    conversion_thread.join();
    renderer_thread.join();
    encoder_thread.join();

    if(config.verbose) {
        LOG("Audio Stream remuxxed into output file.\n");
    }
    LOG("End\n");
    return 0;
}

void decoding_thread_func(VideoDecoder *decoder, ThreadSafeQueue<AVFrame*> *video_queue,
                          ThreadSafeQueue<AVPacket*> *audio_queue, int64_t max_frames, bool verbose) {

    AVFrame* inFrame = nullptr;
    AVPacket* audioPkt = nullptr;
    int64_t frameCount = 0;

    while (true) {
        inFrame = av_frame_alloc();
        if (!inFrame) {
            std::cerr << "Decoder thread: Failed to allocate input video frame.\n";
            break;
        }
        bool frame_read = decoder->readFrame(inFrame);

        if (decoder->hasAudio()) {
            audioPkt = av_packet_alloc();
            if (!audioPkt) {
                std::cerr << "Decoder thread: Failed to allocate audio packet.\n";
                av_frame_free(&inFrame);
                break;
            }
            bool audio_packet_read = decoder->readNextAudioPacket(audioPkt);
            if(audio_packet_read) {
                audio_queue->push(audioPkt);
                if (verbose) {
                    LOG("Decoder thread: Pushed audio packet. PTS: %lld, DTS: %lld\n", audioPkt->pts, audioPkt->dts);
                }
            } else {
                av_packet_free(&audioPkt);
            }
        }

        if (frame_read) {
            if (max_frames != -1 && frameCount >= max_frames) {
                av_frame_free(&inFrame);
                break;
            }
            video_queue->push(inFrame);
            if (verbose) {
                LOG("Decoder thread: Pushed video frame %lld. PTS: %lld\n", frameCount, inFrame->pts);
            }
            frameCount++;
        } else {
            av_frame_free(&inFrame);
            break;
        }
    }
    video_queue->stop();
    if (decoder->hasAudio()) {
        audio_queue->stop();
    }
    if (verbose) {
        LOG("Decoder thread: Finished.\n");
    }
}


void conversion_thread_func(AsciiConverter *converter, ThreadSafeQueue<AVFrame*> *in_video_queue,
                            ThreadSafeQueue<std::unique_ptr<AsciiGrid>> *out_grid_queue, bool verbose) {

    AVFrame* inFrame;
    int64_t convertedCount = 0;
    while (true) {
        inFrame = in_video_queue->pop(); // Pop ownership of decoded frame
        if (!inFrame) { // nullptr = queue stop
            break;
        }

        auto grid = std::make_unique<AsciiGrid>(); // Use unique_ptr for ownership management
        grid->cols = converter->getGridCols();
        grid->rows = converter->getGridRows();
        grid->chars.assign(grid->rows, std::vector<char>(grid->cols));
        grid->colours.assign(grid->rows, std::vector<RGB>(grid->cols));

        converter->convert(inFrame, *grid); // Pass grid by reference to fill it
        av_frame_free(&inFrame); // Free the decoded frame after conversion

        out_grid_queue->push(std::move(grid)); // Push ownership of the unique_ptr
        if (verbose) {
            LOG("Conversion thread: Converted and pushed grid %lld.\n", convertedCount);
        }
        convertedCount++;
    }
    out_grid_queue->stop(); // Signal that no more grids will be pushed
    if (verbose) {
        LOG("Conversion thread: Finished.\n");
    }
}


void rendering_thread_func(AsciiRenderer *renderer, ThreadSafeQueue<std::unique_ptr<AsciiGrid>> *in_grid_queue,
                           ThreadSafeQueue<AVFrame*> *out_video_queue, bool enable_color, bool verbose) {

    std::unique_ptr<AsciiGrid> grid;
    int64_t renderedCount = 0;
    while (true) {
        grid = in_grid_queue->pop(); // Pop ownership of AsciiGrid unique_ptr
        if (!grid) { // Check for nullptr indicating queue stop
            break;
        }

        AVFrame* renderedFrame = renderer->render(*grid, enable_color); // Pass grid by reference
        // unique_ptr will free grid automatically when it goes out of scope

        if (!renderedFrame) {
            std::cerr << "Renderer thread: Rendering failed for a frame.\n";
            continue;
        }
        out_video_queue->push(renderedFrame); // Push ownership of rendered frame
        if (verbose) {
            LOG("Renderer thread: Rendered and pushed frame %lld.\n", renderedCount);
        }
        renderedCount++;
    }
    out_video_queue->stop(); // Signal that no more rendered frames will be pushed
    if (verbose) {
        LOG("Renderer thread: Finished.\n");
    }
}


void encoding_thread_func(VideoEncoder* encoder,
                          ThreadSafeQueue<AVFrame*> *video_in_queue,
                          ThreadSafeQueue<AVPacket*> *audio_in_queue,
                          bool has_audio, bool verbose, ProgressTracker* progress) {

    AVFrame* renderedFrame;
    int64_t encodedFrameCount = 0;

    // Process video frames
    while (true) {
        renderedFrame = video_in_queue->pop(); // Pop ownership of rendered frame
        if (!renderedFrame) { // Check for nullptr indicating queue stop
            break;
        }

        if (encoder->encodeFrame(renderedFrame) < 0) {
            std::cerr << "Encoder thread: Encoding frame failed.\n";
        }
        av_frame_free(&renderedFrame); // Free the rendered frame after encoding
        if (verbose) {
            LOG("Encoder thread: Encoded video frame %lld.\n", encodedFrameCount);
        }
        progress->update(encodedFrameCount++); // Update progress
    }

    // Process audio packets
    if (has_audio) {
        AVPacket* audioPkt;
        int64_t audioPacketCount = 0;
        while (true) {
            audioPkt = audio_in_queue->pop(); // Pop ownership of audio packet
            if (!audioPkt) { // Check for nullptr indicating queue stop
                break;
            }
            if (encoder->writeAudioPacket(audioPkt) < 0) {
                std::cerr << "Encoder thread: Error writing audio packet.\n";
                av_packet_unref(audioPkt);
                break;
            }
            av_packet_unref(audioPkt); // Unreference packet after successful write
            if (verbose) {
                LOG("Encoder thread: Wrote audio packet %lld.\n", audioPacketCount);
            }
            audioPacketCount++;
        }
    }
    
    encoder->finalize(); // Finalize the output file
    progress->finish();
    if (verbose) {
        LOG("Encoder thread: Finished.\n");
    }
}
} // namespace AsciiVideoFilter
