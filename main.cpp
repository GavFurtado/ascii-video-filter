#pragma once

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <string>

inline bool validateArgs(int argc, int expected) {
    return argc == expected;
}

bool checkExtension(const std::filesystem::path &path) {
    const std::array<std::string, 7> extensions = {
        ".mp4", ".mkv", ".avi", ".mov", ".flv", ".webm", ".wmv"
    };

    std::string ext = path.extension().string();
    // convert extension to lower
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

    if(!validateArgs(argc, expected)) {
        std::cerr << "Invalid Argument Count. Too many or too few.\nSyntax: AsciiVideoFilter filename";
        return -1;
    }

    std::filesystem::path path = argv[1];

    if(!checkExtension(path))


    return 0;

}
