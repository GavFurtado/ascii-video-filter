// test/test_video_decoder.cpp
#include <iostream>
#include <cassert>
#include <string>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavutil/frame.h>
}

#include "VideoDecoder.hpp"
#include "Utils.hpp"

using namespace AsciiVideoFilter;

void test_decoder_construction() {
    VideoDecoder decoder;
    
    // Test initial state
    assert(decoder.getWidth() == 0);
    assert(decoder.getHeight() == 0);
    assert(decoder.getPixelFormat() == AV_PIX_FMT_NONE);
    
    std::cout << "Decoder construction test passed\n";
}

void test_decoder_invalid_file() {
    VideoDecoder decoder;
    
    int result = decoder.open("this_file_does_not_exist.mp4");
    assert(result < 0);  // Should fail
    
    // State should remain unchanged after failed open
    assert(decoder.getWidth() == 0);
    assert(decoder.getHeight() == 0);
    
    std::cout << "Invalid file test passed\n";
}

void test_decoder_empty_filename() {
    VideoDecoder decoder;
    
    int result = decoder.open("");
    assert(result < 0);  // Should fail
    
    std::cout << "Empty filename test passed\n";
}

void test_decoder_read_frame_before_open() {
    VideoDecoder decoder;
    AVFrame* frame = av_frame_alloc();
    
    bool result = decoder.readFrame(frame);
    assert(result == false);  // Should fail since no file is open
    
    av_frame_free(&frame);
    std::cout << "Read frame before open test passed\n";
}

int main() {
    std::cout << "Running video decoder tests...\n";
    
    try {
        test_decoder_construction();
        test_decoder_invalid_file();
        test_decoder_empty_filename();
        test_decoder_read_frame_before_open();
        
        std::cout << "All video decoder tests passed!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Unknown test failure\n";
        return 1;
    }
}
