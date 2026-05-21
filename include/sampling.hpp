#pragma once

#include "random.hpp"
#include "tiny_bvh.h"

// Uniformly sample a direction on the unit sphere (PDF = 1 / (4*pi)).
tinybvh::bvhvec3 sample_unit_sphere(Rng& rng);
