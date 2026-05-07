#ifndef VSTIR_BVH_GLSL
#define VSTIR_BVH_GLSL

#ifndef TRACE_STACK_SIZE
#define TRACE_STACK_SIZE 128
#endif

const float TRACE_INFINITY = 3.402823466e+38;
const uint TRACE_NO_IGNORE = 0xffffffffu;

vec3 triangle_geometric_normal(uint triangle_ind) {
    Triangle tri = triangleIn[triangle_ind];
    vec3 a = vertexIn[tri.a];
    vec3 b = vertexIn[tri.b];
    vec3 c = vertexIn[tri.c];
    return normalize(cross(b - a, c - a));
}

bool triangle_intersect_tmax(Ray ray, uint triangle_ind, float tMax, inout Hit hit) {
    Triangle tri = triangleIn[triangle_ind];
    vec3 a = vertexIn[tri.a];
    vec3 b = vertexIn[tri.b];
    vec3 c = vertexIn[tri.c];
    vec3 e1 = b - a;
    vec3 e2 = c - a;
    vec3 h = cross(ray.direction, e2);
    float det = dot(e1, h);
    if (abs(det) < EPS) return false;
    float inv_det = 1.0 / det;
    vec3 s = ray.position - a;
    float u = inv_det * dot(s, h);
    if (u < 0.0 || u > 1.0) return false;
    vec3 q = cross(s, e1);
    float v = inv_det * dot(ray.direction, q);
    if (v < 0.0 || u + v > 1.0) return false;
    float distance = inv_det * dot(e2, q);
    if (distance <= EPS || distance >= tMax) return false;
    hit.distance = distance;
    hit.material = tri.material;
    hit.position = ray.position + ray.direction * distance;
    if (tri.an != 0xffffffffu) {
        hit.normal = normalize((1.0 - u - v) * normalIn[tri.an] + u * normalIn[tri.bn] + v * normalIn[tri.cn]);
    } else {
        hit.normal = triangle_geometric_normal(triangle_ind);
    }
    if (dot(hit.normal, -ray.direction) < 0.0) hit.normal = -hit.normal;
    return true;
}

bool triangle_intersect_any(Ray ray, uint triangle_ind, float tMax) {
    Triangle tri = triangleIn[triangle_ind];
    vec3 a = vertexIn[tri.a];
    vec3 b = vertexIn[tri.b];
    vec3 c = vertexIn[tri.c];
    vec3 e1 = b - a;
    vec3 e2 = c - a;
    vec3 h = cross(ray.direction, e2);
    float det = dot(e1, h);
    if (abs(det) < EPS) return false;
    float inv_det = 1.0 / det;
    vec3 s = ray.position - a;
    float u = inv_det * dot(s, h);
    if (u < 0.0 || u > 1.0) return false;
    vec3 q = cross(s, e1);
    float v = inv_det * dot(ray.direction, q);
    if (v < 0.0 || u + v > 1.0) return false;
    float distance = inv_det * dot(e2, q);
    return distance > EPS && distance < tMax;
}

bool aabb_intersect_interval(Ray ray, uint node_ind, float tMax, out float tNear) {
    vec3 inv_dir = 1.0 / ray.direction;
    vec3 t0 = (bvhIn[node_ind].min - ray.position) * inv_dir;
    vec3 t1 = (bvhIn[node_ind].max - ray.position) * inv_dir;
    vec3 tmin3 = min(t0, t1);
    vec3 tmax3 = max(t0, t1);
    float tmin = max(max(tmin3.x, tmin3.y), tmin3.z);
    float tmax = min(min(tmax3.x, tmax3.y), tmax3.z);
    tNear = max(tmin, 0.0);
    return tmax >= tNear && tNear < tMax;
}

Hit traceClosest(Ray ray, float tMax, inout uint hit_id) {
    Hit hit;
    hit.distance = -1.0;
    hit.material = 0;
    hit.normal = vec3(0.0);
    hit.position = vec3(0.0);
    if (ubo.triangles == 0 || tMax <= EPS) return hit;

    float root_near = 0.0;
    if (!aabb_intersect_interval(ray, 0u, tMax, root_near)) return hit;

    uint stack[TRACE_STACK_SIZE];
    int stack_ptr = 0;
    stack[stack_ptr++] = 0u;
    float closest = tMax;

    while (stack_ptr > 0) {
        uint node_ind = stack[--stack_ptr];
        float node_near = 0.0;
        if (!aabb_intersect_interval(ray, node_ind, closest, node_near)) continue;

        NodeBVH node = bvhIn[node_ind];
        if (node.config == 0u) {
            Hit tri_hit;
            if (triangle_intersect_tmax(ray, node.left, closest, tri_hit)) {
                hit = tri_hit;
                hit_id = node.left;
                closest = tri_hit.distance;
            }
        } else if (node.config == 1u) {
            float left_near = 0.0;
            if (aabb_intersect_interval(ray, node.left, closest, left_near) && stack_ptr < TRACE_STACK_SIZE) stack[stack_ptr++] = node.left;
        } else if (node.config == 2u) {
            float right_near = 0.0;
            if (aabb_intersect_interval(ray, node.right, closest, right_near) && stack_ptr < TRACE_STACK_SIZE) stack[stack_ptr++] = node.right;
        } else {
            float left_near = 0.0;
            float right_near = 0.0;
            bool hit_left = aabb_intersect_interval(ray, node.left, closest, left_near);
            bool hit_right = aabb_intersect_interval(ray, node.right, closest, right_near);
            if (hit_left && hit_right) {
                if (left_near < right_near) {
                    if (stack_ptr < TRACE_STACK_SIZE) stack[stack_ptr++] = node.right;
                    if (stack_ptr < TRACE_STACK_SIZE) stack[stack_ptr++] = node.left;
                } else {
                    if (stack_ptr < TRACE_STACK_SIZE) stack[stack_ptr++] = node.left;
                    if (stack_ptr < TRACE_STACK_SIZE) stack[stack_ptr++] = node.right;
                }
            } else if (hit_left) {
                if (stack_ptr < TRACE_STACK_SIZE) stack[stack_ptr++] = node.left;
            } else if (hit_right) {
                if (stack_ptr < TRACE_STACK_SIZE) stack[stack_ptr++] = node.right;
            }
        }
    }
    return hit;
}

Hit trace(Ray ray, inout uint hit_id) {
    return traceClosest(ray, TRACE_INFINITY, hit_id);
}

bool traceAny(Ray ray, float tMax, uint ignored_triangle) {
    if (ubo.triangles == 0 || tMax <= EPS) return false;

    float root_near = 0.0;
    if (!aabb_intersect_interval(ray, 0u, tMax, root_near)) return false;

    uint stack[TRACE_STACK_SIZE];
    int stack_ptr = 0;
    stack[stack_ptr++] = 0u;

    while (stack_ptr > 0) {
        uint node_ind = stack[--stack_ptr];
        NodeBVH node = bvhIn[node_ind];
        if (node.config == 0u) {
            if (node.left == ignored_triangle) continue;
            if (triangle_intersect_any(ray, node.left, tMax)) return true;
        } else if (node.config == 1u) {
            float left_near = 0.0;
            if (aabb_intersect_interval(ray, node.left, tMax, left_near) && stack_ptr < TRACE_STACK_SIZE) stack[stack_ptr++] = node.left;
        } else if (node.config == 2u) {
            float right_near = 0.0;
            if (aabb_intersect_interval(ray, node.right, tMax, right_near) && stack_ptr < TRACE_STACK_SIZE) stack[stack_ptr++] = node.right;
        } else {
            float left_near = 0.0;
            float right_near = 0.0;
            bool hit_left = aabb_intersect_interval(ray, node.left, tMax, left_near);
            bool hit_right = aabb_intersect_interval(ray, node.right, tMax, right_near);
            if (hit_left && hit_right) {
                if (left_near < right_near) {
                    if (stack_ptr < TRACE_STACK_SIZE) stack[stack_ptr++] = node.right;
                    if (stack_ptr < TRACE_STACK_SIZE) stack[stack_ptr++] = node.left;
                } else {
                    if (stack_ptr < TRACE_STACK_SIZE) stack[stack_ptr++] = node.left;
                    if (stack_ptr < TRACE_STACK_SIZE) stack[stack_ptr++] = node.right;
                }
            } else if (hit_left) {
                if (stack_ptr < TRACE_STACK_SIZE) stack[stack_ptr++] = node.left;
            } else if (hit_right) {
                if (stack_ptr < TRACE_STACK_SIZE) stack[stack_ptr++] = node.right;
            }
        }
    }
    return false;
}

bool traceAny(Ray ray, float tMax) {
    return traceAny(ray, tMax, TRACE_NO_IGNORE);
}

#endif
