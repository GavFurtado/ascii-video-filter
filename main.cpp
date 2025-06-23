#pragma once
#include "src/Application.h"


int main(int argc, const char *argv[]) {
    AsciiVideoFilter::Application app;

    // The run method handles all the application logic.
    return app.run(argc, argv);
}
