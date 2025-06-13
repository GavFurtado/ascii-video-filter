#pragma once

namespace AsciiVideoFilter {

class Application {
public:
    Application();
    ~Application();

    // The main entry point for the application logic.
    // Takes command-line arguments and returns 0 on success, or a negative error code. See "src/Utils.h"
    int run(int argc, const char *argv[]);

private:
    // Helper to print usage information
    void printUsage() const;

};

} // namespace AsciiVideoFilter
