#include "AsciiTypes.h"

extern "C" {
    #include <libavutil/frame.h>
}

namespace AsciiVideoFilter {

class AsciiRenderer {
public:
    AsciiRenderer();
    ~AsciiRenderer();

    /**
     * @brief Initializes the renderer output resolution and font cell size.
     * @param width Output frame width (pixels)
     * @param height Output frame height (pixels)
     * @param cellWidth Pixel width of each character block
     * @param cellHeight Pixel height of each character block
     */
    int init(int width, int height, int cellWidth, int cellHeight);

    /**
     * @brief Converts an AsciiGrid to an RGB24 AVFrame ready for encoding.
     * @param grid The ASCII grid (characters + colors)
     * @return AVFrame* (RGB24). You own the frame and must free it.
     */
    AVFrame* render(const AsciiGrid& grid);

private:
    int m_width, m_height;
    int m_cellWidth, m_cellHeight;
};
}
