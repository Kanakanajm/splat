#pragma once

#include "random.hpp"
#include "tiny_bvh.h"

// Uniformly sample a direction on the unit sphere (PDF = 1 / (4*pi)).
tinybvh::bvhvec3 sample_unit_sphere(Rng& rng);

// Cosine-weighted sample on the hemisphere about `normal` (PDF = cos(theta) / pi).
// `normal` must be normalized; the returned direction is normalized.
tinybvh::bvhvec3 sample_cosine_hemisphere(Rng& rng, const tinybvh::bvhvec3& normal);
