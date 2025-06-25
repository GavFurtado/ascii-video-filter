#include "Application.hpp"
#include "VideoDecoder.hpp"
#include "VideoEncoder.hpp"
#include "AsciiConverter.hpp"
#include "AsciiRenderer.hpp"
#include "Utils.hpp"

#include <string>
#include <unordered_map>

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
    // TODO: fix memory leak (IMPIMPIMP)


    // Define a maximum frame count for testing purposes
    // Set to -1 to process all frames
    // const int MAX_FRAMES = 3;
    const int MAX_FRAMES = -1;

    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_file> <output_file>\n";
        return 1;
    }

    const std::string inputPath = argv[1];
    const std::string outputPath = argv[2];

    const std::string ttfFontPath = "./assets/RubikMonoOne-Regular.ttf";
    const std::unordered_map<std::string, std::string> charPresets = {
        {"standard", " .:-=+*#%@"},
        {"detailed", " .'`^,:;Il!i><~+_-?][}{1)(|\\/tfjrxnumbroCLJVUNYXOZmwqpdbkhao*#MW&8%B@$"},
        {"binary", " 01 "}
        // {"dots", " .∙⬤⦿☉○●"}, // std::string doesn't do unicode so needs changes to work
        // {"shapes", " ▫▪▩▨▧▦▥▤▣▢□■"}
    };


    VideoDecoder decoder;
    if (decoder.open(inputPath) < 0) {
        std::cerr << "Failed to open input video.\n";
        return 1;
    }

    int videoWidth = decoder.getWidth();
    int videoHeight = decoder.getHeight();

    AsciiConverter converter;
    converter.setAsciiCharset(charPresets.at("detailed"));
    converter.init(videoWidth, videoHeight, decoder.getPixelFormat(), 12, 36);

    AVFrame* inFrame = av_frame_alloc();
    if (!inFrame) {
        std::cerr << "Failed to allocate input frame.\n";
        return 1;
    }

    AsciiRenderer renderer;
    // Initialize AsciiRenderer's font
    if (renderer.initFont(ttfFontPath, converter.getBlockHeight()) < 0) {
        std::cerr << "Error: Failed to initialize ASCII renderer font. Exiting.\\n";
        return static_cast<int>(AppErrorCode::APP_ERR_FONT_INIT_FAILED); 
    }

    // VideoEncoder encoder;
    // if (encoder.init(outputPath, decoder.getMetadata(), videoWidth, videoHeight, 400000) < 0) {
    //     std::cerr << "Failed to initialize video encoder.\n";
    //     return 1;
    // }

    // if (decoder.hasAudio()) {
    //     encoder.addAudioStreamFrom(decoder.getAudioStream());
    // }

    AsciiGrid grid;
    grid.cols = converter.getGridCols();
    grid.rows = converter.getGridRows();
    grid.chars.assign(grid.rows, std::vector<char>(grid.cols));
    grid.colours.assign(grid.rows, std::vector<RGB>(grid.cols));

    int frameCount = 0;
    while (decoder.readFrame(inFrame) && (MAX_FRAMES == -1 || frameCount < MAX_FRAMES)) {
        converter.convert(inFrame, grid);
        renderer.initFrame(videoWidth, videoHeight, converter.getBlockWidth(), converter.getBlockHeight());

        auto start = std::chrono::steady_clock::now();
        AVFrame* renderedFrame = renderer.render(grid);
        auto end = std::chrono::steady_clock::now();

        std::cout << "Frame: " << frameCount << " Render time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms\n";
        #ifdef DEBUG
        #endif // DEBUG

        if (!renderedFrame) {
            std::cerr << "Rendering failed.\n";
            break;
        }

        // if (encoder.encodeFrame(renderedFrame) < 0) {
        //     std::cerr << "Encoding frame failed.\n";
        //     break;
        // }

        av_frame_unref(inFrame);
        frameCount++;
    }
    av_frame_free(&inFrame);

    LOG("Reached frame rendering loop exit.\n") ;

    // encoder.finalize();

    LOG("encoder.finalize() executed.\n");

    
    long count = 0;
    std::cout<< "Remuxxing audio stream.\n";
    // remux the audio stream if exists
    // if (decoder.hasAudio()) {
    //     AVPacket* pkt = av_packet_alloc();
    //     if (!pkt) {
    //         std::cerr << "Failed to allocate audio packet.\n";
    //         // Handle error, maybe return 1;
    //     } else {
    //         // Loop for audio packets
    //         while (decoder.readNextAudioPacket(pkt)) {
    //             LOG("Audio Packet Loop Counter: %d, PTS: %lld, DTS: %lld, Duration: %lld, Stream Index: %d\n",
    //                 count, pkt->pts, pkt->dts, pkt->duration, pkt->stream_index);
    //
    //             if (encoder.writeAudioPacket(pkt) < 0) {
    //                 std::cerr << "Error writing audio packet. Stopping audio remux.\n";
    //                 av_packet_unref(pkt);
    //                 break;
    //             }
    //             av_packet_unref(pkt); // Unreference packet after successful write
    //             count++;
    //         }
    //         av_packet_free(&pkt); // Free the packet when done with the loop
    //     }
    // } else {
    //     std::cout << "No audio stream to remux.\n";
    // }

    LOG("Audio Stream remuxxed into output file.\n");

    LOG("End\n");
    return 0;
}

} // namespace AsciiVideoFilter
