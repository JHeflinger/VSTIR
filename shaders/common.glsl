#ifndef VSTIR_COMMON_GLSL
#define VSTIR_COMMON_GLSL

struct Triangle {
    uint a;
    uint b;
    uint c;
    uint an;
    uint bn;
    uint cn;
    uint material;
};

struct NodeBVH {
    vec3 min;
    vec3 max;
    uint config;
    uint left;
    uint right;
};

struct Ray {
    vec3 position;
    vec3 direction;
};

struct Hit {
    float distance;
    uint material;
    vec3 normal;
    vec3 position;
};

struct Material {
    vec3 emission;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    vec3 absorbtion;
    vec3 dispersion;
    float ior;
    float shiny;
    uint model;
};

struct RayGenerator {
    vec3 rawaccumulation;
    vec3 accumulation;
    vec3 initial_x_v;
    vec3 initial_n_v;
    vec3 initial_x_s;
    vec3 initial_n_s;
    vec3 initial_Lo;
    float initial_pdf;
    uint initial_mat;
    uint initial_flags;
    vec3 temporal_x_v;
    vec3 temporal_n_v;
    vec3 temporal_x_s;
    vec3 temporal_n_s;
    vec3 temporal_Lo;
    float temporal_w;
    uint temporal_M;
    float temporal_W;
    uint temporal_flags;
    vec3 spatial_x_v;
    vec3 spatial_n_v;
    vec3 spatial_x_s;
    vec3 spatial_n_s;
    vec3 spatial_Lo;
    float spatial_w;
    uint spatial_M;
    float spatial_W;
    uint spatial_flags;
    vec3 direct;
    vec3 filtered;
    vec3 filtered_tmp;
    vec3 direct_filtered;
    vec3 direct_filtered_tmp;
};

layout(binding = 0) uniform UniformBufferObject {
    vec3 look;
    vec3 position;
    vec3 up;
    vec3 u;
    vec3 v;
    vec3 w;
    uint triangles;
    uint seed;
    uint samples;
    float fov;
    float width;
    float height;
    mat4 previousvpm;
    mat4 currentvpm;
    float depththreshold;
    float normalthreshold;
    uint temporal_m_cap;
    uint spatial_m_cap;
    uint spacerange;
    uint spacecount;
    uint restir_bounces;
    uint emissivecount;
    uint directlighting;
    uint divider;
    float divider_position;
    float divider_angle;
    uint filters;
    uint restir;
    uint showdivider;
    uint directlightingright;
    uint filtersright;
    uint restirright;
    uint restirframes;
    uint resetreservoirs;
} ubo;

const float RESTIR_EPS = 0.000001;
const float RESTIR_PI = 3.14159265358979323846;
const float RESTIR_MAX_RECONNECTION_CORRECTION = 10.0;
const float RESTIR_DIFFUSE_SPECULAR_LIMIT = 0.05;
const uint RESTIR_LAMBERTIAN_MODEL = 2u;
const uint RESTIR_FLAG_EXPORTABLE = 1u;
const uint RESTIR_FLAG_DIFFUSE_REUSE = 2u;

float restir_luminance(vec3 c) {
    return 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b;
}

bool restir_finite_vec3(vec3 v) {
    return !any(isnan(v)) && !any(isinf(v));
}

bool restir_valid_radiance(vec3 v) {
    return restir_finite_vec3(v) && restir_luminance(v) > RESTIR_EPS;
}

vec3 restir_offset_ray_origin(vec3 position, vec3 normal, vec3 direction) {
    return position + normal * (dot(normal, direction) >= 0.0 ? RESTIR_EPS : -RESTIR_EPS);
}

bool restir_diffuse_safe_material(Material material) {
    return material.model == RESTIR_LAMBERTIAN_MODEL &&
        restir_luminance(material.diffuse) > RESTIR_EPS &&
        restir_luminance(material.specular) <= RESTIR_DIFFUSE_SPECULAR_LIMIT;
}

vec3 restir_diffuse_throughput(vec3 normal, vec3 incoming, Material material) {
    return (material.diffuse / RESTIR_PI) * max(0.0, dot(incoming, normal));
}

float restir_target_luminance(vec3 Lo) {
    return restir_valid_radiance(Lo) ? restir_luminance(Lo) : 0.0;
}

float restir_contribution_target_luminance(vec3 x_v, vec3 n_v, vec3 x_s, vec3 Lo, Material material) {
    if (!restir_valid_radiance(Lo) || !restir_diffuse_safe_material(material)) return 0.0;
    vec3 delta = x_s - x_v;
    if (length(delta) <= RESTIR_EPS) return 0.0;
    vec3 wi = normalize(delta);
    vec3 contribution = restir_diffuse_throughput(n_v, wi, material) * Lo;
    if (!restir_finite_vec3(contribution)) return 0.0;
    return max(restir_luminance(contribution), 0.0);
}

float restir_reconnection_jacobian(vec3 source_x_v, vec3 target_x_v, vec3 x_s, vec3 n_s) {
    vec3 source_delta = source_x_v - x_s;
    vec3 target_delta = target_x_v - x_s;
    float source_dist = length(source_delta);
    float target_dist = length(target_delta);
    if (source_dist <= RESTIR_EPS || target_dist <= RESTIR_EPS) return 0.0;
    vec3 source_dir = source_delta / source_dist;
    vec3 target_dir = target_delta / target_dist;
    float source_cos = clamp(abs(dot(n_s, source_dir)), 0.0, 1.0);
    float target_cos = clamp(abs(dot(n_s, target_dir)), 0.0, 1.0);
    if (source_cos <= RESTIR_EPS || target_cos <= RESTIR_EPS) return 0.0;
    float J = (source_dist * source_dist) / (target_dist * target_dist) * (target_cos / source_cos);
    return (J > RESTIR_EPS && !isnan(J) && !isinf(J)) ? J : 0.0;
}

float restir_reconnection_correction(vec3 source_x_v, vec3 target_x_v, vec3 x_s, vec3 n_s) {
    float J = restir_reconnection_jacobian(source_x_v, target_x_v, x_s, n_s);
    if (J <= RESTIR_EPS) return 0.0;
    float correction = 1.0 / J;
    return (!isnan(correction) && !isinf(correction)) ?
        min(correction, RESTIR_MAX_RECONNECTION_CORRECTION) : 0.0;
}

vec2 restir_divider_axis() {
    float angle = radians(clamp(ubo.divider_angle, 0.0, 180.0));
    return vec2(cos(angle), sin(angle));
}

float restir_divider_extent(vec2 axis) {
    return 0.5 * (abs(axis.x) + abs(axis.y));
}

float restir_divider_coord(vec2 pixel, vec2 axis) {
    vec2 denom = max(vec2(ubo.width - 1.0, ubo.height - 1.0), vec2(1.0));
    vec2 uv = pixel / denom;
    return dot(uv - vec2(0.5), axis);
}

float restir_divider_threshold(vec2 axis) {
    float extent = restir_divider_extent(axis);
    return mix(-extent, extent, clamp(ubo.divider_position, 0.0, 1.0));
}

bool restir_divider_right_side_pixel(int x, int y) {
    if (ubo.showdivider == 0) return false;
    vec2 axis = restir_divider_axis();
    return restir_divider_coord(vec2(float(x), float(y)), axis) > restir_divider_threshold(axis);
}

bool restir_divider_line_pixel(int x, int y) {
    if (ubo.showdivider == 0) return false;
    vec2 axis = restir_divider_axis();
    float coord = restir_divider_coord(vec2(float(x), float(y)), axis);
    float threshold = restir_divider_threshold(axis);
    float pixel_width = max(1.0 / max(ubo.width, 1.0), 1.0 / max(ubo.height, 1.0));
    return abs(coord - threshold) <= pixel_width * 1.5;
}

#endif
