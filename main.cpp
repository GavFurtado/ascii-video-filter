#pragma once

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

inline bool validateArgs(int argc, int expected);
bool checkExtension(const std::filesystem::path &path, const std::vector<std::string> &extensions);


int main(int argc, const char *argv[]) {
    // very likely to change depending on scope
    int expected = 1;
    const std::vector<std::string> extensions = {
        ".mp4", ".mkv", ".avi", ".mov", ".flv", ".webm", ".wmv"
    };

    if(!validateArgs(argc, expected)) {
        std::cerr << "Invalid Argument Count. Too many or too few.\nSyntax: AsciiVideoFilter filename";
        return -1;
    }

    std::filesystem::path path = argv[1];

    if(!checkExtension(path, extensions)) {
        std::cerr << "Invalid Media File type. Supported types: "<< std::endl;
        for(const auto& extension : extensions) {
            std::cerr << extension << ", ";
        }
        std::cerr << std::endl;
    }


    // ffmpeg stuff

    return 0;
}

inline bool validateArgs(int argc, int expected) {
    return argc == expected;
}

bool checkExtension(const std::filesystem::path &path, const std::vector<std::string> &extensions) {


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
