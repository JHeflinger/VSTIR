#include "bvh.h"
#include "util/log.h"

#include <algorithm>
#include <array>
#include <limits>

#define BVH_LIMIT 0.0001f
#define BVH_BIN_COUNT 16
#define BVH_MAX_SAH_DEPTH 48

namespace VSTIR {

    struct SAHSplit {
        bool found = false;
        int axis = 0;
        int splitBin = 0;
        float cost = std::numeric_limits<float>::max();
    };

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

    static float SurfaceArea(const glm::vec3& min, const glm::vec3& max) {
        const glm::vec3 extent = glm::max(max - min, glm::vec3(0.0f));
        return 2.0f * (extent.x * extent.y + extent.x * extent.z + extent.y * extent.z);
    }

    static int BinIndex(float value, float min, float extent) {
        const float normalized = (value - min) / extent;
        return std::clamp((int)(normalized * BVH_BIN_COUNT), 0, BVH_BIN_COUNT - 1);
    }

    static size_t MedianSplit(
        std::vector<size_t>& indices,
        const std::vector<AABB>& aabbs,
        size_t begin,
        size_t end,
        const glm::vec3& centroidMin,
        const glm::vec3& centroidMax) {
        const glm::vec3 centroidExtents = centroidMax - centroidMin;
        const int axis = LongestAxis(centroidExtents);
        const size_t mid = begin + (end - begin) / 2;

        if (centroidExtents[axis] > BVH_LIMIT) {
            std::nth_element(
                indices.begin() + begin,
                indices.begin() + mid,
                indices.begin() + end,
                [&](size_t a, size_t b) {
                    return aabbs[a].centroid[axis] < aabbs[b].centroid[axis];
                });
        }
        return mid;
    }

    static SAHSplit FindBinnedSAHSplit(
        const std::vector<size_t>& indices,
        const std::vector<AABB>& aabbs,
        size_t begin,
        size_t end,
        const glm::vec3& nodeMin,
        const glm::vec3& nodeMax,
        const glm::vec3& centroidMin,
        const glm::vec3& centroidMax) {
        SAHSplit best;
        const float parentArea = SurfaceArea(nodeMin, nodeMax);
        if (parentArea <= BVH_LIMIT) return best;

        for (int axis = 0; axis < 3; axis++) {
            const float extent = centroidMax[axis] - centroidMin[axis];
            if (extent <= BVH_LIMIT) continue;

            std::array<glm::vec3, BVH_BIN_COUNT> binMin;
            std::array<glm::vec3, BVH_BIN_COUNT> binMax;
            std::array<size_t, BVH_BIN_COUNT> binCount{};
            for (int i = 0; i < BVH_BIN_COUNT; i++) {
                binMin[i] = glm::vec3(std::numeric_limits<float>::max());
                binMax[i] = glm::vec3(-std::numeric_limits<float>::max());
            }

            for (size_t i = begin; i < end; i++) {
                const AABB& box = aabbs[indices[i]];
                const int bin = BinIndex(box.centroid[axis], centroidMin[axis], extent);
                binCount[bin]++;
                binMin[bin] = glm::min(binMin[bin], box.min);
                binMax[bin] = glm::max(binMax[bin], box.max);
            }

            for (int split = 0; split < BVH_BIN_COUNT - 1; split++) {
                glm::vec3 leftMin(std::numeric_limits<float>::max());
                glm::vec3 leftMax(-std::numeric_limits<float>::max());
                glm::vec3 rightMin(std::numeric_limits<float>::max());
                glm::vec3 rightMax(-std::numeric_limits<float>::max());
                size_t leftCount = 0;
                size_t rightCount = 0;

                for (int i = 0; i <= split; i++) {
                    if (binCount[i] == 0) continue;
                    leftCount += binCount[i];
                    leftMin = glm::min(leftMin, binMin[i]);
                    leftMax = glm::max(leftMax, binMax[i]);
                }
                for (int i = split + 1; i < BVH_BIN_COUNT; i++) {
                    if (binCount[i] == 0) continue;
                    rightCount += binCount[i];
                    rightMin = glm::min(rightMin, binMin[i]);
                    rightMax = glm::max(rightMax, binMax[i]);
                }

                if (leftCount == 0 || rightCount == 0) continue;
                const float cost = 1.0f +
                    (SurfaceArea(leftMin, leftMax) * (float)leftCount +
                     SurfaceArea(rightMin, rightMax) * (float)rightCount) / parentArea;
                if (cost < best.cost) {
                    best.found = true;
                    best.axis = axis;
                    best.splitBin = split;
                    best.cost = cost;
                }
            }
        }

        return best;
    }

    static size_t SAHSplitRange(
        std::vector<size_t>& indices,
        const std::vector<AABB>& aabbs,
        size_t begin,
        size_t end,
        const glm::vec3& nodeMin,
        const glm::vec3& nodeMax,
        const glm::vec3& centroidMin,
        const glm::vec3& centroidMax,
        size_t depth) {
        if (depth < BVH_MAX_SAH_DEPTH) {
            const SAHSplit split = FindBinnedSAHSplit(indices, aabbs, begin, end, nodeMin, nodeMax, centroidMin, centroidMax);
            if (split.found) {
                const float extent = centroidMax[split.axis] - centroidMin[split.axis];
                auto midIt = std::partition(
                    indices.begin() + begin,
                    indices.begin() + end,
                    [&](size_t index) {
                        const int bin = BinIndex(aabbs[index].centroid[split.axis], centroidMin[split.axis], extent);
                        return bin <= split.splitBin;
                    });
                const size_t mid = (size_t)std::distance(indices.begin(), midIt);
                if (mid > begin && mid < end) return mid;
            }
        }

        return MedianSplit(indices, aabbs, begin, end, centroidMin, centroidMax);
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

        auto build = [&](auto&& self, size_t begin, size_t end, size_t depth) -> uint32_t {
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

            const size_t mid = SAHSplitRange(
                indices, aabbs,
                begin, end,
                nodeMin, nodeMax,
                centroidMin, centroidMax,
                depth);

            // Force progress even in degenerate centroid distributions.
            if (mid == begin || mid == end) {
                const size_t fallbackMid = begin + count / 2;
                bvh[nodeIndex].config = BVH_BOTH;
                bvh[nodeIndex].left = self(self, begin, fallbackMid, depth + 1);
                bvh[nodeIndex].right = self(self, fallbackMid, end, depth + 1);
                return nodeIndex;
            }

            bvh[nodeIndex].config = BVH_BOTH;
            bvh[nodeIndex].left = self(self, begin, mid, depth + 1);
            bvh[nodeIndex].right = self(self, mid, end, depth + 1);
            return nodeIndex;
        };

        build(build, 0, indices.size(), 0);
        return bvh;
    }

}
