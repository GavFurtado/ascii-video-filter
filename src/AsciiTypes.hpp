#pragma once

#include <vector>
#include <cstdint>

namespace AsciiVideoFilter {

struct RGB {
    uint8_t r, g, b;
};

struct AsciiGrid {
    std::vector<std::vector<char>> chars;
    std::vector<std::vector<RGB>> colours; // for per-block RGB colouring
    int rows = 0;
    int cols = 0;
};

} // namespace AsciiVideoFilter
