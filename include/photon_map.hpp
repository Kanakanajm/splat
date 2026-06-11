#pragma once

#include "tiny_bvh.h"

#include <algorithm>
#include <vector>

// 3-D kd-tree for radius queries. T must have a `tinybvh::bvhvec3 position` member.
// Build once after the emit pass; query per pixel during gather.
template <typename T>
class PhotonKdTree {
public:
    // Takes pts by value — sorts in place during build.
    void build(std::vector<T> pts);

    // Returns pointers into internal storage; valid until next build() call.
    std::vector<const T*> radius_search(const tinybvh::bvhvec3& pos, float r) const;

    bool        empty() const { return nodes_.empty(); }
    std::size_t size()  const { return nodes_.size(); }

private:
    struct Node {
        T   pt;
        int axis  = 0;
        int left  = -1;
        int right = -1;
    };

    std::vector<Node> nodes_;
    int               root_ = -1;

    int  build_impl(std::vector<T>& pts, int lo, int hi);
    void search_impl(int idx, const tinybvh::bvhvec3& pos, float r2,
                     std::vector<const T*>& out) const;

    static float at(const tinybvh::bvhvec3& p, int axis) noexcept {
        return axis == 0 ? p.x : (axis == 1 ? p.y : p.z);
    }
    static float sqdist(const tinybvh::bvhvec3& a, const tinybvh::bvhvec3& b) noexcept {
        const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
        return dx*dx + dy*dy + dz*dz;
    }
};

template <typename T>
void PhotonKdTree<T>::build(std::vector<T> pts) {
    nodes_.clear();
    root_ = -1;
    if (pts.empty()) return;
    nodes_.reserve(pts.size());
    root_ = build_impl(pts, 0, static_cast<int>(pts.size()));
}

template <typename T>
int PhotonKdTree<T>::build_impl(std::vector<T>& pts, int lo, int hi) {
    if (lo >= hi) return -1;

    // Pick the axis with the widest extent.
    float mn[3] = {pts[lo].position.x, pts[lo].position.y, pts[lo].position.z};
    float mx[3] = {mn[0], mn[1], mn[2]};
    for (int i = lo + 1; i < hi; ++i) {
        mn[0] = std::min(mn[0], pts[i].position.x);
        mn[1] = std::min(mn[1], pts[i].position.y);
        mn[2] = std::min(mn[2], pts[i].position.z);
        mx[0] = std::max(mx[0], pts[i].position.x);
        mx[1] = std::max(mx[1], pts[i].position.y);
        mx[2] = std::max(mx[2], pts[i].position.z);
    }
    const int axis = (mx[0]-mn[0] >= mx[1]-mn[1] && mx[0]-mn[0] >= mx[2]-mn[2]) ? 0
                   : (mx[1]-mn[1] >= mx[2]-mn[2]) ? 1 : 2;

    const int mid = lo + (hi - lo) / 2;
    std::nth_element(pts.begin() + lo, pts.begin() + mid, pts.begin() + hi,
        [axis](const T& a, const T& b) { return at(a.position, axis) < at(b.position, axis); });

    // Push root before recursing; access by index is stable across later push_backs.
    const int node_idx = static_cast<int>(nodes_.size());
    nodes_.push_back({pts[mid], axis, -1, -1});

    nodes_[node_idx].left  = build_impl(pts, lo, mid);
    nodes_[node_idx].right = build_impl(pts, mid + 1, hi);
    return node_idx;
}

template <typename T>
void PhotonKdTree<T>::search_impl(int idx, const tinybvh::bvhvec3& pos, float r2,
                                  std::vector<const T*>& out) const {
    if (idx < 0) return;
    const Node& n = nodes_[idx];

    if (sqdist(n.pt.position, pos) <= r2)
        out.push_back(&n.pt);

    // Visit closer half first; only descend into the far half if the split plane is within r.
    const float d     = at(pos, n.axis) - at(n.pt.position, n.axis);
    const int first   = d <= 0.0f ? n.left  : n.right;
    const int second  = d <= 0.0f ? n.right : n.left;
    search_impl(first, pos, r2, out);
    if (d * d <= r2)
        search_impl(second, pos, r2, out);
}

template <typename T>
std::vector<const T*> PhotonKdTree<T>::radius_search(const tinybvh::bvhvec3& pos, float r) const {
    std::vector<const T*> out;
    search_impl(root_, pos, r * r, out);
    return out;
}
