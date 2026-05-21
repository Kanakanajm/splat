#pragma once

#include "ray_model.hpp"

#include <cstdint>
#include <vector>

// Scene-side attribution layered on top of RayModel's geometry. Holds per-instance
// bsdf and medium-inside/-outside ids; defaults all to 0. The tracer looks up
// per-triangle ids via prim → instance → assigned id.
class Scene {
public:
    explicit Scene(const RayModel& model);

    void set_instance_bsdf(uint32_t instance_id, uint32_t bsdf_id);
    void set_instance_medium(uint32_t instance_id, uint32_t medium_in_id, uint32_t medium_out_id);

    uint32_t bsdf_id_at(uint32_t prim)    const;
    uint32_t medium_in_at(uint32_t prim)  const;
    uint32_t medium_out_at(uint32_t prim) const;

    const RayModel& model() const { return model_; }

private:
    const RayModel&       model_;
    std::vector<uint32_t> instance_bsdf_;
    std::vector<uint32_t> instance_medium_in_;
    std::vector<uint32_t> instance_medium_out_;
};
