#include "scene.hpp"

Scene::Scene(const RayModel& model)
    : model_(model),
      instance_bsdf_(model.instance_count(), 0u),
      instance_medium_in_(model.instance_count(), 0u),
      instance_medium_out_(model.instance_count(), 0u),
      bsdf_table_(1),     // id 0 = default Diffuse
      medium_table_(1) {} // id 0 = vacuum

void Scene::set_instance_bsdf(uint32_t instance_id, uint32_t bsdf_id) {
    instance_bsdf_[instance_id] = bsdf_id;
}

void Scene::set_instance_medium(uint32_t instance_id, uint32_t medium_in_id, uint32_t medium_out_id) {
    instance_medium_in_[instance_id]  = medium_in_id;
    instance_medium_out_[instance_id] = medium_out_id;
}

void Scene::set_bsdf(uint32_t bsdf_id, const Bsdf& bsdf) {
    if (bsdf_id >= bsdf_table_.size()) bsdf_table_.resize(bsdf_id + 1);
    bsdf_table_[bsdf_id] = bsdf;
}

void Scene::set_medium(uint32_t medium_id, const Medium& medium) {
    if (medium_id >= medium_table_.size()) medium_table_.resize(medium_id + 1);
    medium_table_[medium_id] = medium;
}

uint32_t Scene::bsdf_id_at(uint32_t prim)    const { return instance_bsdf_[model_.instance_id(prim)]; }
uint32_t Scene::medium_in_at(uint32_t prim)  const { return instance_medium_in_[model_.instance_id(prim)]; }
uint32_t Scene::medium_out_at(uint32_t prim) const { return instance_medium_out_[model_.instance_id(prim)]; }
