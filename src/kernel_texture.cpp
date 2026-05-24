#include "kernel_texture.hpp"

#include <cmath>

std::vector<float> build_kernel_texture(int size) {
    constexpr float kPi   = 3.14159265358979f;
    constexpr float kPeak = 2.0f / kPi;  // Epanechnikov 2D: k(r) = (2/π)(1−r²)

    std::vector<float> data(static_cast<size_t>(size * size), 0.0f);
    const float inv = 1.0f / static_cast<float>(size);
    for (int row = 0; row < size; ++row) {
        for (int col = 0; col < size; ++col) {
            const float u = (col + 0.5f) * inv;
            const float v = (row + 0.5f) * inv;
            const float du = u - 0.5f;
            const float dv = v - 0.5f;
            const float r  = 2.0f * std::sqrt(du * du + dv * dv);
            if (r < 1.0f)
                data[static_cast<size_t>(row * size + col)] = kPeak * (1.0f - r * r);
        }
    }
    return data;
}
