#pragma once

// Homogeneous participating medium with an isotropic phase function.
// Vacuum is the convention sigma_s = sigma_a = 0 (sigma_t == 0).
struct Medium {
    float sigma_s = 0.0f;  // scattering coefficient
    float sigma_a = 0.0f;  // absorption coefficient

    float sigma_t() const { return sigma_s + sigma_a; }  // extinction coefficient
};
