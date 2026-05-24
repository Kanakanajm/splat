#include "image_buffer.hpp"

#include <fstream>

bool write_ppm(const std::string& path, const ImageBuffer& img) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << "P6\n" << img.width << ' ' << img.height << "\n255\n";
    f.write(reinterpret_cast<const char*>(img.rgb.data()),
            static_cast<std::streamsize>(img.rgb.size()));
    return static_cast<bool>(f);
}
