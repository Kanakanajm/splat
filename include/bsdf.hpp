#pragma once

#include "random.hpp"
#include "tiny_bvh.h"

enum class BsdfKind { Diffuse, Conductor, Dielectric, MediumShell };

struct BsdfSample {
    tinybvh::bvhvec3 dir;        // normalized outgoing direction
    tinybvh::bvhvec3 weight;     // BSDF multiplier for the running photon weight
    bool             is_refract; // true for transmissive events (Dielectric refract, MediumShell)
};

// Surface scattering descriptor.
struct Bsdf {
    BsdfKind         kind               = BsdfKind::Diffuse;
    float            ior                = 1.0f;
    tinybvh::bvhvec3 color              = {0.8f, 0.8f, 0.8f}; // reflectance tint (all kinds)
    tinybvh::bvhvec3 transmittance_color = {1.0f, 1.0f, 1.0f}; // Dielectric transmission tint

    // Sample an outgoing direction. `incoming` is the normalized photon travel
    // direction (into the surface); `normal` is the normalized geometric normal.
    BsdfSample sample(Rng& rng,
                      const tinybvh::bvhvec3& incoming,
                      const tinybvh::bvhvec3& normal) const;
};
