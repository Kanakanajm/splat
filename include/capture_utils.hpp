#pragma once

#include <cmath>
#include <string>
#include <string_view>
#include <vector>

// Generate n evenly-spaced checkpoint SPP values from 1 to max_spp (inclusive).
// The first value is always 1 and the last is always max_spp.
// Duplicate values that arise from integer rounding are removed, so the
// returned vector may have fewer than n entries when max_spp < n.
inline std::vector<int> generate_checkpoints(int max_spp, int n) {
    if (n <= 0 || max_spp <= 0) return {};
    if (n == 1)                  return {max_spp};

    std::vector<int> result;
    result.reserve(n);
    for (int i = 0; i < n; ++i) {
        const int spp = 1 + static_cast<int>(
            std::round(static_cast<double>(max_spp - 1) * i / (n - 1)));
        if (result.empty() || spp != result.back())
            result.push_back(spp);
    }
    result.back() = max_spp;  // guarantee exact last value
    return result;
}

// Parse a comma-separated list of positive integers, e.g. "1,4,16,64,256".
// Non-positive values and unparseable tokens are silently skipped.
inline std::vector<int> parse_checkpoint_spps(std::string_view s) {
    std::vector<int> result;
    size_t start = 0;
    while (start <= s.size()) {
        const size_t end = s.find(',', start);
        const auto   tok = s.substr(start, (end == std::string_view::npos ? s.size() : end) - start);
        if (!tok.empty()) {
            try {
                const int val = std::stoi(std::string(tok));
                if (val > 0) result.push_back(val);
            } catch (...) {}
        }
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return result;
}
