#pragma once

#include "AsciiConverter.hpp"
#include "AsciiRenderer.hpp"
#include "AsciiTypes.hpp"
#include "ThreadSafeQueue.hpp"
#include "VideoDecoder.hpp"
#include "VideoEncoder.hpp"

extern "C" {
    #include <libavcodec/packet.h>
    #include <libavutil/frame.h>
}
namespace AsciiVideoFilter {

void decoding_thread_func(VideoDecoder *decoder, ThreadSafeQueue<AVFrame*> *video_queue,
                          ThreadSafeQueue<AVPacket*> *audio_queue, int64_t max_frames, bool verbose);

void conversion_thread_func(AsciiConverter *converter, ThreadSafeQueue<AVFrame*> *in_video_queue,
                            ThreadSafeQueue<std::unique_ptr<AsciiGrid>> *out_grid_queue, bool verbose);

void rendering_thread_func(AsciiRenderer *renderer, ThreadSafeQueue<std::unique_ptr<AsciiGrid>> *in_grid_queue,
                           ThreadSafeQueue<AVFrame*> *out_video_queue, bool enable_color, bool verbose);

void encoding_thread_func(VideoEncoder* encoder,
                          ThreadSafeQueue<AVFrame*> *video_in_queue,
                          ThreadSafeQueue<AVPacket*> *audio_in_queue,
                          bool has_audio, bool verbose, ProgressTracker* progress);

class Application {
public:
    Application();
    ~Application();

    // Takes command-line arguments and returns 0 on success, or a negative error code. See "src/Utils.h"
    int run(int argc, const char *argv[]);

private:
    // Helper to print usage information
    void printUsage() const;
};

} // namespace AsciiVideoFilter
