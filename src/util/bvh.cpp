#include "bvh.h"
#include "util/log.h"

#include <algorithm>
#include <limits>

#define BVH_LIMIT 0.0001f

namespace VSTIR {

    static std::vector<AABB> generateAABBs(const std::vector<Triangle>& triangles, const std::vector<glm::vec4>& vertices) {
        std::vector<AABB> aabbs;
        aabbs.reserve(triangles.size());
        for (size_t i = 0; i < triangles.size(); i++) {
            const glm::vec3 a = glm::vec3(vertices[triangles[i].a]);
            const glm::vec3 b = glm::vec3(vertices[triangles[i].b]);
            const glm::vec3 c = glm::vec3(vertices[triangles[i].c]);
            const glm::vec3 min = glm::min(a, glm::min(b, c));
            const glm::vec3 max = glm::max(a, glm::max(b, c));
            const glm::vec3 centroid = (min + max) * 0.5f;
            aabbs.push_back((AABB){ min, max, centroid });
        }
        return aabbs;
    }

    static int LongestAxis(const glm::vec3& extents) {
        return extents.x > extents.y ? (extents.x > extents.z ? 0 : 2) : (extents.y > extents.z ? 1 : 2);
    }

    static void ComputeRangeBounds(
        const std::vector<size_t>& indices,
        const std::vector<AABB>& aabbs,
        size_t begin,
        size_t end,
        glm::vec3& outMin,
        glm::vec3& outMax,
        glm::vec3& centroidMin,
        glm::vec3& centroidMax) {
        outMin = glm::vec3(std::numeric_limits<float>::max());
        outMax = glm::vec3(-std::numeric_limits<float>::max());
        centroidMin = glm::vec3(std::numeric_limits<float>::max());
        centroidMax = glm::vec3(-std::numeric_limits<float>::max());
        for (size_t i = begin; i < end; i++) {
            const AABB& box = aabbs[indices[i]];
            outMin = glm::min(outMin, box.min);
            outMax = glm::max(outMax, box.max);
            centroidMin = glm::min(centroidMin, box.centroid);
            centroidMax = glm::max(centroidMax, box.centroid);
        }
    }

    std::vector<NodeBVH> BVH::Create(const std::vector<Triangle>& triangles, const std::vector<glm::vec4>& vertices) {
        if (triangles.empty()) {
            return {};
        }

        std::vector<NodeBVH> bvh;
        bvh.reserve(triangles.size() * 2);

        const std::vector<AABB> aabbs = generateAABBs(triangles, vertices);
        std::vector<size_t> indices;
        indices.reserve(triangles.size());
        for (size_t i = 0; i < triangles.size(); i++) {
            indices.push_back(i);
        }

        auto build = [&](auto&& self, size_t begin, size_t end) -> uint32_t {
            glm::vec3 nodeMin;
            glm::vec3 nodeMax;
            glm::vec3 centroidMin;
            glm::vec3 centroidMax;
            ComputeRangeBounds(indices, aabbs, begin, end, nodeMin, nodeMax, centroidMin, centroidMax);

            const uint32_t nodeIndex = (uint32_t)bvh.size();
            bvh.push_back((NodeBVH){ nodeMin, nodeMax, BVH_LEAF, 0, 0 });

            const size_t count = end - begin;
            if (count == 1) {
                bvh[nodeIndex].config = BVH_LEAF;
                bvh[nodeIndex].left = (uint32_t)indices[begin];
                bvh[nodeIndex].right = 0;
                return nodeIndex;
            }

            const glm::vec3 centroidExtents = centroidMax - centroidMin;
            const int axis = LongestAxis(centroidExtents);
            const size_t mid = begin + count / 2;

            if (centroidExtents[axis] > BVH_LIMIT) {
                std::nth_element(
                    indices.begin() + begin,
                    indices.begin() + mid,
                    indices.begin() + end,
                    [&](size_t a, size_t b) {
                        return aabbs[a].centroid[axis] < aabbs[b].centroid[axis];
                    });
            }

            // Force progress even in degenerate centroid distributions.
            if (mid == begin || mid == end) {
                const size_t fallbackMid = begin + count / 2;
                bvh[nodeIndex].config = BVH_BOTH;
                bvh[nodeIndex].left = self(self, begin, fallbackMid);
                bvh[nodeIndex].right = self(self, fallbackMid, end);
                return nodeIndex;
            }

            bvh[nodeIndex].config = BVH_BOTH;
            bvh[nodeIndex].left = self(self, begin, mid);
            bvh[nodeIndex].right = self(self, mid, end);
            return nodeIndex;
        };

        build(build, 0, indices.size());
        return bvh;
    }

}
