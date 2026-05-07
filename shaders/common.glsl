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
    uint contributioncap;
    uint candidatecap;
    uint spacerange;
    uint spacecount;
    uint emissivecount;
    uint directlighting;
    uint divider;
    uint filters;
    uint restir;
    uint showdivider;
    uint directlightingright;
    uint filtersright;
    uint restirright;
    uint restirframes;
    uint resetreservoirs;
} ubo;

#endif
