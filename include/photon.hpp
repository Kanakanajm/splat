#pragma once

#include "tiny_bvh.h"

#include <cstdint>

struct PhotonPoint {
    tinybvh::bvhvec3 position;
    uint32_t         bsdf_id;
};

struct PhotonBeam {
    tinybvh::bvhvec3 start;
    tinybvh::bvhvec3 end;
    uint32_t         medium_id;
};
