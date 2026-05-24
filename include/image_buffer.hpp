#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct ImageBuffer {
    uint32_t             width  = 0;
    uint32_t             height = 0;
    std::vector<uint8_t> rgb;   // row-major, 3 bytes per pixel

    ImageBuffer() = default;
    ImageBuffer(uint32_t w, uint32_t h) : width(w), height(h), rgb(3u * w * h, 0u) {}

    void set(uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b) {
        const std::size_t i = 3u * (static_cast<std::size_t>(y) * width + x);
        rgb[i + 0] = r;
        rgb[i + 1] = g;
        rgb[i + 2] = b;
    }
};

// Write a P6 (binary RGB) PPM. Returns false on I/O failure.
bool write_ppm(const std::string& path, const ImageBuffer& img);
