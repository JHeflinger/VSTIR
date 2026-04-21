#include "bvh.h"
#include "util/log.h"

#define BVH_LIMIT 0.0001f

namespace VSTIR {

    void ResizeBVH(std::vector<NodeBVH>& bvh, size_t index) {
        if (bvh[index].config == BVH_BOTH) {
            ResizeBVH(bvh, bvh[index].left);
            ResizeBVH(bvh, bvh[index].right);
            bvh[index].min = glm::min(bvh[bvh[index].left].min, bvh[index].min);
            bvh[index].max = glm::max(bvh[bvh[index].left].max, bvh[index].max);
            bvh[index].min = glm::min(bvh[bvh[index].right].min, bvh[index].min);
            bvh[index].max = glm::max(bvh[bvh[index].right].max, bvh[index].max);
        } else if (bvh[index].config == BVH_LEFT_ONLY) {
            ResizeBVH(bvh, bvh[index].left);
            bvh[index].min = glm::min(bvh[bvh[index].left].min, bvh[index].min);
            bvh[index].max = glm::max(bvh[bvh[index].left].max, bvh[index].max);
        } else if (bvh[index].config == BVH_RIGHT_ONLY) {
            ResizeBVH(bvh, bvh[index].right);
            bvh[index].min = glm::min(bvh[bvh[index].right].min, bvh[index].min);
            bvh[index].max = glm::max(bvh[bvh[index].right].max, bvh[index].max);
        } else {
            return;
        }
    }

    void SplitBVH(std::vector<NodeBVH>& bvh, size_t index, const std::vector<Triangle>& triangles, std::vector<size_t>& children, const std::vector<AABB> aabbs) {
        #define CBVH bvh[index]
        #define BVHMIN bvh[index].min
        #define BVHMAX bvh[index].max

        // split boxes
        float xyz_dist[3] = {
            BVHMAX.x - BVHMIN.x,
            BVHMAX.y - BVHMIN.y,
            BVHMAX.z - BVHMIN.z
        };
        int dist_ind = xyz_dist[0] > xyz_dist[1] ?
            (xyz_dist[0] > xyz_dist[2] ? 0 : 2) :
            (xyz_dist[1] > xyz_dist[2] ? 1 : 2);
        if (xyz_dist[dist_ind] < BVH_LIMIT) {
            size_t stream_index = index;
            for (size_t i = 0; i < children.size(); i++) {
                NodeBVH child = {
                    aabbs[children[i]].min,
                    aabbs[children[i]].max,
                    BVH_LEAF, (uint32_t)children[i], 0
                };
                bvh.push_back(child);
                bvh[stream_index].left = bvh.size() - 1;

                if (i + 1 >= children.size()) {
                    bvh[stream_index].config = BVH_LEFT_ONLY;
                } else {
                    bvh[stream_index].config = BVH_BOTH;
                    NodeBVH next = { BVHMIN, BVHMAX, BVH_LEAF, 0, 0 };
                    bvh.push_back(next);
                    bvh[stream_index].right = bvh.size() - 1;
                    stream_index = bvh.size() - 1;
                }
            }
            ResizeBVH(bvh, index);
            return;
        }
        float mid_dist = xyz_dist[dist_ind] / 2.0f;
        NodeBVH left = { BVHMIN, BVHMAX, BVH_LEAF, 0, 0 };
        NodeBVH right = { BVHMIN, BVHMAX, BVH_LEAF, 0, 0 };
        left.max[dist_ind] -= mid_dist;
        right.min[dist_ind] += mid_dist;

        // subchildren
        std::vector<size_t> left_children;
        std::vector<size_t> right_children;
        for (size_t i = 0; i < children.size(); i++) {
            if (aabbs[children[i]].centroid[dist_ind] < left.max[dist_ind])
                left_children.push_back(children[i]);
            else
                right_children.push_back(children[i]);
        }
        if (left_children.size() > 0 && right_children.size() > 0) {
            CBVH.config = BVH_BOTH;
        } else if (left_children.size() > 0) {
            CBVH.config = BVH_LEFT_ONLY;
        } else if (right_children.size() > 0) {
            CBVH.config = BVH_RIGHT_ONLY;
        } else {
            FATAL("This bvh logic should never happen");
            CBVH.config = BVH_LEAF;
        }
        if (left_children.size() > 1) {
            bvh.push_back(left);
            CBVH.left = bvh.size() - 1;
            SplitBVH(bvh, CBVH.left, triangles, left_children, aabbs);
        } else if (left_children.size() == 1) {
            left.config = BVH_LEAF;
            left.left = left_children[0];
            left.min = aabbs[left.left].min;
            left.max = aabbs[left.left].max;
            bvh.push_back(left);
            CBVH.left = bvh.size() - 1;
        }
        if (right_children.size() > 1) {
            bvh.push_back(right);
            CBVH.right = bvh.size() - 1;
            SplitBVH(bvh, CBVH.right, triangles, right_children, aabbs);
        } else if (right_children.size() == 1) {
            right.config = BVH_LEAF;
            right.left = right_children[0];
            right.min = aabbs[right.left].min;
            right.max = aabbs[right.left].max;
            bvh.push_back(right);
            CBVH.right = bvh.size() - 1;
        }

        // resize
        if (bvh[index].config == BVH_BOTH || bvh[index].config == BVH_LEFT_ONLY) {
            bvh[index].min = glm::min(bvh[bvh[index].left].min, bvh[index].min);
            bvh[index].max = glm::max(bvh[bvh[index].left].max, bvh[index].max);
        }
        if (bvh[index].config == BVH_BOTH || bvh[index].config == BVH_RIGHT_ONLY) {
            bvh[index].min = glm::min(bvh[bvh[index].right].min, bvh[index].min);
            bvh[index].max = glm::max(bvh[bvh[index].right].max, bvh[index].max);
        }

        #undef CBVH
        #undef BVHMIN
        #undef BVHMAX
    }

    std::vector<AABB> generateAABBs(const std::vector<Triangle>& triangles, const std::vector<glm::vec4>& vertices) {
        std::vector<AABB> aabbs;
        for (int i = 0; i < triangles.size(); i++) {
            glm::vec3 a = glm::vec3(vertices[triangles[i].a]);
            glm::vec3 b = glm::vec3(vertices[triangles[i].b]);
            glm::vec3 c = glm::vec3(vertices[triangles[i].c]);
            glm::vec3 min = glm::min(a, glm::min(b, c));
            glm::vec3 max = glm::max(a, glm::max(b, c));
            glm::vec3 centroid = (min + max) / 2.0f;
            aabbs.push_back((AABB){ min, max, centroid });
        }
        return aabbs;
    }

    std::vector<NodeBVH> BVH::Create(const std::vector<Triangle>& triangles, const std::vector<glm::vec4>& vertices) {
        std::vector<NodeBVH> bvh;
        std::vector<AABB> aabbs = generateAABBs(triangles, vertices);
        NodeBVH root = (NodeBVH){
            glm::vec3(std::numeric_limits<float>::max()),
            glm::vec3(-std::numeric_limits<float>::max()),
            BVH_LEAF, 0, 0
        };
        std::vector<size_t> indices;
        for (size_t i = 0; i < triangles.size(); i++) {
            root.min = glm::min(aabbs[i].min, root.min);
            root.max = glm::max(aabbs[i].max, root.max);
            indices.push_back(i);
        }
        bvh.push_back(root);
        SplitBVH(bvh, 0, triangles, indices, aabbs);
        return bvh;
    }

}
