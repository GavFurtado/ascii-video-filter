#pragma once

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

inline bool validateArgs(int argc, int expected) {
    return argc == expected;
}

bool validateExtension(const std::filesystem::path &path, const std::vector<std::string> &extensions) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return std::tolower(c);
    });

    for(const auto& validExt: extensions) {
        if(ext == validExt) {
            return true;
        }
    }
    return false;
}

int main(int argc, const char *argv[]) {
    // very likely to change depending on scope
    int expected = 1;
    const std::vector<std::string> extensions = {
        ".mp4", ".mkv", ".avi", ".mov", ".flv", ".webm", ".wmv"
    };


    if(!validateArgs(argc, expected)) {
        std::cerr << "Invalid Argument Count. Too many or too few.\nExpected: AsciiVideoFilter filename\n";
        return -1;
    }

    std::filesystem::path path = argv[1];

    if(!validateExtension(path, extensions)) {
        std::cerr << "File type invalid.\nValid file types: ";
        for(size_t i = 0; i <= extensions.size() - 2; ++i) {
            std::cerr << extensions[i] << ", ";
        }
        std::cerr << extensions.back() << "\n";
        return -1;
    }

    // start processing the video


    return 0;

}
