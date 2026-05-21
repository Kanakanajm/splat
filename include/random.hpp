#pragma once

#include <cstdint>
#include <random>

// Thin deterministic PRNG wrapper. Same seed -> same sequence.
// Uses mt19937 and a manual 24-bit float mapping to guarantee strictly [0, 1).
class Rng {
public:
    explicit Rng(uint64_t seed) : engine_(seed) {}

    // Uniform float in [0, 1).
    float uniform() {
        return static_cast<float>(engine_() >> 8) * (1.0f / 16777216.0f);
    }

    uint32_t uniform_u32() { return engine_(); }

private:
    std::mt19937 engine_;
};
