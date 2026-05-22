#pragma once

#include "bsdf.hpp"
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

    // Bsdf table keyed by bsdf id. Id 0 defaults to a Diffuse bsdf; set_bsdf
    // grows the table as needed, leaving gaps as default-constructed (Diffuse).
    void set_bsdf(uint32_t bsdf_id, const Bsdf& bsdf);
    const Bsdf& bsdf(uint32_t bsdf_id) const { return bsdf_table_[bsdf_id]; }
    const Bsdf& bsdf_at(uint32_t prim) const { return bsdf_table_[bsdf_id_at(prim)]; }

    const RayModel& model() const { return model_; }

private:
    const RayModel&       model_;
    std::vector<uint32_t> instance_bsdf_;
    std::vector<uint32_t> instance_medium_in_;
    std::vector<uint32_t> instance_medium_out_;
    std::vector<Bsdf>     bsdf_table_;
};
