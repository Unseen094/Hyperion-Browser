#pragma once

#include "hre/dom/node.hpp"
#include <string>
#include <vector>
#include <memory>

namespace hre::dom {

class CanvasRenderingContext2D {
public:
    CanvasRenderingContext2D(int width, int height) : width(width), height(height) {
        pixels.resize(width * height * 4, 0); // RGBA
    }

    void fill_rect(double x, double y, double w, double h) {
        // Simplified: Fill with white for now
        for (int py = (int)y; py < y + h && py < height; ++py) {
            for (int px = (int)x; px < x + w && px < width; ++px) {
                if (py >= 0 && px >= 0) {
                    size_t idx = (py * width + px) * 4;
                    pixels[idx] = 255;     // R
                    pixels[idx + 1] = 255; // G
                    pixels[idx + 2] = 255; // B
                    pixels[idx + 3] = 255; // A
                }
            }
        }
    }

    void fill_text(const std::wstring& text, double x, double y) {
        // Placeholder: Text rendering not implemented in this minimal version
    }

    void clear_rect(double x, double y, double w, double h) {
        for (int py = (int)y; py < y + h && py < height; ++py) {
            for (int px = (int)x; px < x + w && px < width; ++px) {
                if (py >= 0 && px >= 0) {
                    size_t idx = (py * width + px) * 4;
                    pixels[idx] = 0;
                    pixels[idx + 1] = 0;
                    pixels[idx + 2] = 0;
                    pixels[idx + 3] = 0;
                }
            }
        }
    }

    int width;
    int height;
    std::vector<unsigned char> pixels;
};

class canvas_element : public element {
public:
    explicit canvas_element(std::wstring tag_name)
        : element(std::move(tag_name)), m_context(nullptr) {}

    CanvasRenderingContext2D* get_context_2d() {
        if (!m_context) {
            int w = 300; // Default width
            int h = 150; // Default height
            // In a real implementation, parse attributes for width/height
            m_context = std::make_unique<CanvasRenderingContext2D>(w, h);
        }
        return m_context.get();
    }

private:
    std::unique_ptr<CanvasRenderingContext2D> m_context;
};

} // namespace hre::dom
