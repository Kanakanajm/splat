#pragma once

#include <vector>

// Build a size×size Epanechnikov kernel texture (row-major, one float per texel).
//
// Kernel: k(r) = (2/π)(1 − r²) for r ≤ 1, 0 outside.
// r = 2 · |UV − 0.5|, so the kernel support maps exactly to the inscribed
// unit circle within the [0,1]² UV square. Integrates to 1 in 2D r-space.
std::vector<float> build_kernel_texture(int size = 64);
