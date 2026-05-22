#pragma once

#include "random.hpp"
#include "tiny_bvh.h"

enum class BsdfKind { Diffuse, Conductor, Dielectric, MediumShell };

// Surface scattering descriptor. Only Diffuse scattering is implemented so far;
// the remaining kinds are placeholders for later chunks.
struct Bsdf {
    BsdfKind         kind  = BsdfKind::Diffuse;
    float            ior   = 1.0f;              // relative index of refraction, used by Dielectric
    tinybvh::bvhvec3 color = {0.8f, 0.8f, 0.8f}; // display albedo for Diffuse AOV

    // Sample an outgoing (scattered) direction. `incoming` is the normalized
    // direction the photon travels (pointing into the surface) and `normal` is
    // the normalized geometric normal; the returned direction is normalized.
    tinybvh::bvhvec3 sample(Rng& rng,
                            const tinybvh::bvhvec3& incoming,
                            const tinybvh::bvhvec3& normal) const;
};
