#include <iostream>
#include <cassert>
#include <vector>

#include "AsciiTypes.hpp"  

using namespace AsciiVideoFilter;

void test_rgb_struct() {
    RGB color{255, 128, 64};
    assert(color.r == 255);
    assert(color.g == 128);
    assert(color.b == 64);
    std::cout << "RGB struct test passed\n";
}

void test_ascii_grid_empty() {
    AsciiGrid grid;
    assert(grid.rows == 0);
    assert(grid.cols == 0);
    assert(grid.chars.empty());
    assert(grid.colours.empty());
    std::cout << "Empty AsciiGrid test passed\n";
}

void test_ascii_grid_resize() {
    AsciiGrid grid;
    grid.rows = 2;
    grid.cols = 3;
    grid.chars.resize(grid.rows, std::vector<char>(grid.cols, 'X'));
    grid.colours.resize(grid.rows, std::vector<RGB>(grid.cols, RGB{100, 150, 200}));
    
    assert(grid.chars.size() == 2);
    assert(grid.chars[0].size() == 3);
    assert(grid.chars[1][2] == 'X');
    assert(grid.colours[0][1].g == 150);
    std::cout << "AsciiGrid resize test passed\n";
}

int main() {
    std::cout << "Running basic types tests...\n";
    
    try {
        test_rgb_struct();
        test_ascii_grid_empty();
        test_ascii_grid_resize();
        
        std::cout << "All basic types tests passed!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Unknown test failure\n";
        return 1;
    }
}
