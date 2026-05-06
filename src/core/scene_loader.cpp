#include "scene_loader.h"

#include "core/editor.h"
#include "util/bvh.h"
#include "util/log.h"

#include <yaml-cpp/yaml.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

#include <cctype>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace VSTIR::SceneLoader {

namespace {

struct Transform {
    glm::vec3 translation = glm::vec3(0.0f);
    glm::vec3 rotationDegreesXYZ = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
};

static Material ConvertMaterial(const tinyobj::material_t& src);
static Material ConvertMaterial(const tinygltf::Material& src);

static std::string ToLower(std::string value) {
    for (char& c : value) {
        c = (char)std::tolower((unsigned char)c);
    }
    return value;
}

static std::string ResolvePath(const std::string& baseFile, const std::string& candidate) {
    namespace fs = std::filesystem;
    fs::path p(candidate);
    if (p.is_absolute()) {
        return p.lexically_normal().string();
    }
    fs::path base(baseFile);
    if (base.has_parent_path()) {
        return (base.parent_path() / p).lexically_normal().string();
    }
    return p.lexically_normal().string();
}

static bool ParseVec3Node(const YAML::Node& n, glm::vec3& out) {
    if (!n || !n.IsSequence() || n.size() != 3) {
        return false;
    }
    out.x = n[0].as<float>();
    out.y = n[1].as<float>();
    out.z = n[2].as<float>();
    return true;
}

static bool ParseMaterialNode(const YAML::Node& node, Material& out) {
    if (!node || !node.IsMap()) {
        return false;
    }

    // Match tinyobj/.mtl defaults and then apply explicit overrides.
    out = ConvertMaterial(tinyobj::material_t{});

    // Direct .mtl names.
    ParseVec3Node(node["Ke"], out.emission);
    ParseVec3Node(node["Ka"], out.ambient);
    ParseVec3Node(node["Kd"], out.diffuse);
    ParseVec3Node(node["Ks"], out.specular);
    ParseVec3Node(node["Tf"], out.absorbtion);
    if (node["Ni"]) {
        out.ior = node["Ni"].as<float>();
    }
    if (node["Ns"]) {
        out.shiny = node["Ns"].as<float>();
    }
    if (node["illum"]) {
        out.model = node["illum"].as<uint32_t>();
    }

    // Friendly aliases (mapped to same fields as above).
    ParseVec3Node(node["emission"], out.emission);
    ParseVec3Node(node["ambient"], out.ambient);
    ParseVec3Node(node["diffuse"], out.diffuse);
    ParseVec3Node(node["specular"], out.specular);
    ParseVec3Node(node["transmittance"], out.absorbtion);
    ParseVec3Node(node["absorption"], out.absorbtion);
    ParseVec3Node(node["absorbtion"], out.absorbtion);
    ParseVec3Node(node["dispersion"], out.dispersion);

    if (node["ior"]) {
        out.ior = node["ior"].as<float>();
    }
    if (node["shiny"]) {
        out.shiny = node["shiny"].as<float>();
    }
    if (node["model"]) {
        out.model = node["model"].as<uint32_t>();
    }

    return true;
}

static Transform ParseTransform(const YAML::Node& node) {
    Transform t;
    if (!node || !node.IsMap()) {
        return t;
    }
    if (node["translation"]) {
        ParseVec3Node(node["translation"], t.translation);
    }
    if (node["rotation_degrees_xyz"]) {
        ParseVec3Node(node["rotation_degrees_xyz"], t.rotationDegreesXYZ);
    }
    if (node["scale"]) {
        ParseVec3Node(node["scale"], t.scale);
    }
    return t;
}

static glm::mat4 ComposeTransform(const Transform& t) {
    glm::mat4 m(1.0f);
    m = glm::translate(m, t.translation);
    m = glm::rotate(m, glm::radians(t.rotationDegreesXYZ.x), glm::vec3(1.0f, 0.0f, 0.0f));
    m = glm::rotate(m, glm::radians(t.rotationDegreesXYZ.y), glm::vec3(0.0f, 1.0f, 0.0f));
    m = glm::rotate(m, glm::radians(t.rotationDegreesXYZ.z), glm::vec3(0.0f, 0.0f, 1.0f));
    m = glm::scale(m, t.scale);
    return m;
}

static void ApplyCamera(const YAML::Node& cameraNode) {
    if (!cameraNode || !cameraNode.IsMap()) {
        return;
    }
    camera& cam = Editor::Get()->GetRenderer().GetCamera();
    glm::vec3 v;
    if (ParseVec3Node(cameraNode["position"], v)) {
        cam.Position() = v;
    }
    if (ParseVec3Node(cameraNode["look"], v)) {
        cam.setLook(v);
    }
    if (ParseVec3Node(cameraNode["up"], v)) {
        cam.setUp(v);
    }
    if (cameraNode["fov_degrees"]) {
        cam.Fov() = cameraNode["fov_degrees"].as<float>();
    }
    if (cameraNode["orbiting"]) {
        cam.IsOrbiting() = cameraNode["orbiting"].as<bool>();
    }
    if (ParseVec3Node(cameraNode["orbit_target"], v)) {
        cam.OrbitTarget() = v;
    }
}

static bool AppendEmissiveQuad(const YAML::Node& lightNode, Geometry& geometry) {
    if (!lightNode || !lightNode.IsMap()) {
        return false;
    }
    std::string type = lightNode["type"] ? lightNode["type"].as<std::string>() : "";
    if (type != "emissive_quad") {
        WARN("Unsupported light type \"%s\" in scene file. Skipping", type.c_str());
        return true;
    }

    float width = lightNode["width"] ? lightNode["width"].as<float>() : 1.0f;
    float height = lightNode["height"] ? lightNode["height"].as<float>() : 1.0f;
    glm::vec3 emission(1.0f);
    if (!ParseVec3Node(lightNode["emission"], emission)) {
        WARN("Light is missing valid emission [r,g,b]. Falling back to [1,1,1]");
    }
    Transform transform = ParseTransform(lightNode["transform"]);
    glm::mat4 model = ComposeTransform(transform);
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));

    const size_t vStart = geometry.vertices.size();
    const size_t nStart = geometry.normals.size();
    const uint32_t matIndex = (uint32_t)geometry.materials.size();

    Material m{};
    m.emission = emission;
    m.ambient = glm::vec3(0.0f);
    m.diffuse = glm::vec3(0.0f);
    m.specular = glm::vec3(0.0f);
    m.absorbtion = glm::vec3(0.0f);
    m.dispersion = glm::vec3(0.0f);
    m.ior = 1.0f;
    m.shiny = 0.0f;
    m.model = 0;
    geometry.materials.push_back(m);

    const float hx = width * 0.5f;
    const float hy = height * 0.5f;
    const glm::vec3 localVerts[4] = {
        glm::vec3(-hx, -hy, 0.0f),
        glm::vec3(hx, -hy, 0.0f),
        glm::vec3(hx, hy, 0.0f),
        glm::vec3(-hx, hy, 0.0f)
    };

    for (int i = 0; i < 4; i++) {
        geometry.vertices.push_back(glm::vec4(glm::vec3(model * glm::vec4(localVerts[i], 1.0f)), 0.0f));
    }

    glm::vec3 worldNormal = glm::normalize(normalMatrix * glm::vec3(0.0f, 0.0f, 1.0f));
    for (int i = 0; i < 4; i++) {
        geometry.normals.push_back(glm::vec4(worldNormal, 0.0f));
    }

    geometry.triangles.push_back(Triangle{
        (uint32_t)vStart + 0, (uint32_t)vStart + 1, (uint32_t)vStart + 2,
        (uint32_t)nStart + 0, (uint32_t)nStart + 1, (uint32_t)nStart + 2,
        matIndex
    });
    geometry.triangles.push_back(Triangle{
        (uint32_t)vStart + 0, (uint32_t)vStart + 2, (uint32_t)vStart + 3,
        (uint32_t)nStart + 0, (uint32_t)nStart + 2, (uint32_t)nStart + 3,
        matIndex
    });
    return true;
}

static Material ConvertMaterial(const tinyobj::material_t& src) {
    Material out{};
    out.emission = glm::vec3(src.emission[0], src.emission[1], src.emission[2]);
    out.ambient = glm::vec3(src.ambient[0], src.ambient[1], src.ambient[2]);
    out.diffuse = glm::vec3(src.diffuse[0], src.diffuse[1], src.diffuse[2]);
    out.specular = glm::vec3(src.specular[0], src.specular[1], src.specular[2]);
    out.absorbtion = glm::vec3(src.transmittance[0], src.transmittance[1], src.transmittance[2]);
    out.dispersion = glm::vec3(0.0f);
    out.ior = src.ior > 0.0f ? src.ior : 1.0f;
    out.shiny = src.shininess;
    out.model = src.illum >= 0 ? (uint32_t)src.illum : 0u;
    return out;
}

static Material ConvertMaterial(const tinygltf::Material& src) {
    Material out{};
    out.ior = 1.0f;

    const auto& pbr = src.pbrMetallicRoughness;
    if (pbr.baseColorFactor.size() >= 3) {
        out.diffuse = glm::vec3(
            (float)pbr.baseColorFactor[0],
            (float)pbr.baseColorFactor[1],
            (float)pbr.baseColorFactor[2]);
    }
    if (src.emissiveFactor.size() >= 3) {
        out.emission = glm::vec3(
            (float)src.emissiveFactor[0],
            (float)src.emissiveFactor[1],
            (float)src.emissiveFactor[2]);
    }

    // Approximate MTL-like shininess from glTF roughness.
    const double roughness = pbr.roughnessFactor;
    const double clamped = roughness < 0.0 ? 0.0 : (roughness > 1.0 ? 1.0 : roughness);
    out.shiny = (float)((1.0 - clamped) * (1.0 - clamped) * 1000.0);

    auto iorExtIt = src.extensions.find("KHR_materials_ior");
    if (iorExtIt != src.extensions.end()) {
        const tinygltf::Value::Object& obj = iorExtIt->second.Get<tinygltf::Value::Object>();
        auto iorIt = obj.find("ior");
        if (iorIt != obj.end() && iorIt->second.IsNumber()) {
            out.ior = (float)iorIt->second.Get<double>();
        }
    }

    return out;
}

static glm::mat4 BuildNodeLocalMatrix(const tinygltf::Node& node) {
    if (node.matrix.size() == 16) {
        glm::mat4 m(1.0f);
        for (int c = 0; c < 4; c++) {
            for (int r = 0; r < 4; r++) {
                m[c][r] = (float)node.matrix[c * 4 + r];
            }
        }
        return m;
    }

    glm::vec3 t(0.0f);
    if (node.translation.size() == 3) {
        t = glm::vec3((float)node.translation[0], (float)node.translation[1], (float)node.translation[2]);
    }
    glm::quat q(1.0f, 0.0f, 0.0f, 0.0f);
    if (node.rotation.size() == 4) {
        q = glm::quat((float)node.rotation[3], (float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2]);
    }
    glm::vec3 s(1.0f);
    if (node.scale.size() == 3) {
        s = glm::vec3((float)node.scale[0], (float)node.scale[1], (float)node.scale[2]);
    }

    return glm::translate(glm::mat4(1.0f), t) * glm::mat4_cast(q) * glm::scale(glm::mat4(1.0f), s);
}

static void GatherNodeMeshInstances(
    const tinygltf::Model& model,
    int nodeIndex,
    const glm::mat4& parent,
    std::vector<std::pair<int, glm::mat4>>& out) {
    if (nodeIndex < 0 || (size_t)nodeIndex >= model.nodes.size()) {
        return;
    }
    const tinygltf::Node& node = model.nodes[(size_t)nodeIndex];
    const glm::mat4 world = parent * BuildNodeLocalMatrix(node);
    if (node.mesh >= 0) {
        out.emplace_back(node.mesh, world);
    }
    for (int child : node.children) {
        GatherNodeMeshInstances(model, child, world, out);
    }
}

static bool ReadVec3Accessor(
    const tinygltf::Model& model,
    int accessorIndex,
    std::vector<glm::vec3>& out) {
    if (accessorIndex < 0 || (size_t)accessorIndex >= model.accessors.size()) {
        return false;
    }
    const tinygltf::Accessor& accessor = model.accessors[(size_t)accessorIndex];
    if (accessor.type != TINYGLTF_TYPE_VEC3 || accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
        return false;
    }
    if ((size_t)accessor.bufferView >= model.bufferViews.size()) {
        return false;
    }
    const tinygltf::BufferView& view = model.bufferViews[(size_t)accessor.bufferView];
    if ((size_t)view.buffer >= model.buffers.size()) {
        return false;
    }
    const tinygltf::Buffer& buffer = model.buffers[(size_t)view.buffer];

    const size_t stride = accessor.ByteStride(view) ? accessor.ByteStride(view) : sizeof(float) * 3;
    const size_t base = (size_t)view.byteOffset + (size_t)accessor.byteOffset;

    out.resize((size_t)accessor.count);
    for (size_t i = 0; i < out.size(); i++) {
        const size_t at = base + i * stride;
        if (at + sizeof(float) * 3 > buffer.data.size()) {
            return false;
        }
        float v[3];
        memcpy(v, buffer.data.data() + at, sizeof(float) * 3);
        out[i] = glm::vec3(v[0], v[1], v[2]);
    }
    return true;
}

static bool ReadIndexAccessor(
    const tinygltf::Model& model,
    int accessorIndex,
    std::vector<uint32_t>& out) {
    if (accessorIndex < 0 || (size_t)accessorIndex >= model.accessors.size()) {
        return false;
    }
    const tinygltf::Accessor& accessor = model.accessors[(size_t)accessorIndex];
    if ((size_t)accessor.bufferView >= model.bufferViews.size()) {
        return false;
    }
    const tinygltf::BufferView& view = model.bufferViews[(size_t)accessor.bufferView];
    if ((size_t)view.buffer >= model.buffers.size()) {
        return false;
    }
    const tinygltf::Buffer& buffer = model.buffers[(size_t)view.buffer];

    const size_t compSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    const size_t stride = accessor.ByteStride(view) ? accessor.ByteStride(view) : compSize;
    const size_t base = (size_t)view.byteOffset + (size_t)accessor.byteOffset;

    out.resize((size_t)accessor.count);
    for (size_t i = 0; i < out.size(); i++) {
        const size_t at = base + i * stride;
        if (at + compSize > buffer.data.size()) {
            return false;
        }
        switch (accessor.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                out[i] = (uint32_t)buffer.data[at];
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                uint16_t v;
                memcpy(&v, buffer.data.data() + at, sizeof(uint16_t));
                out[i] = (uint32_t)v;
                break;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
                uint32_t v;
                memcpy(&v, buffer.data.data() + at, sizeof(uint32_t));
                out[i] = v;
                break;
            }
            default:
                return false;
        }
    }
    return true;
}

static bool LoadGLTFFile(
    const std::string& filepath,
    const Transform& transform,
    Geometry& geometry,
    const Material* overrideMaterial) {
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err;
    std::string warn;

    const std::string ext = ToLower(std::filesystem::path(filepath).extension().string());
    bool ok = false;
    if (ext == ".glb") {
        ok = loader.LoadBinaryFromFile(&model, &err, &warn, filepath);
    } else {
        ok = loader.LoadASCIIFromFile(&model, &err, &warn, filepath);
    }
    if (!warn.empty()) {
        WARN("tinygltf warning for '%s': %s", filepath.c_str(), warn.c_str());
    }
    if (!ok) {
        ERROR("tinygltf failed to load '%s': %s", filepath.c_str(), err.c_str());
        return false;
    }

    const glm::mat4 yamlModel = ComposeTransform(transform);

    const size_t materialOffset = geometry.materials.size();
    if (overrideMaterial) {
        geometry.materials.push_back(*overrideMaterial);
    } else if (model.materials.empty()) {
        Material fallback{};
        fallback.ior = 1.0f;
        geometry.materials.push_back(fallback);
    } else {
        for (const tinygltf::Material& mat : model.materials) {
            geometry.materials.push_back(ConvertMaterial(mat));
        }
    }

    std::vector<std::pair<int, glm::mat4>> meshInstances;
    if (model.defaultScene >= 0 && (size_t)model.defaultScene < model.scenes.size()) {
        const tinygltf::Scene& scene = model.scenes[(size_t)model.defaultScene];
        for (int nodeIndex : scene.nodes) {
            GatherNodeMeshInstances(model, nodeIndex, glm::mat4(1.0f), meshInstances);
        }
    } else {
        for (size_t i = 0; i < model.nodes.size(); i++) {
            if (model.nodes[i].mesh >= 0) {
                GatherNodeMeshInstances(model, (int)i, glm::mat4(1.0f), meshInstances);
            }
        }
    }
    if (meshInstances.empty()) {
        for (size_t i = 0; i < model.meshes.size(); i++) {
            meshInstances.emplace_back((int)i, glm::mat4(1.0f));
        }
    }

    for (const auto& instance : meshInstances) {
        const int meshIndex = instance.first;
        if (meshIndex < 0 || (size_t)meshIndex >= model.meshes.size()) {
            continue;
        }
        const tinygltf::Mesh& mesh = model.meshes[(size_t)meshIndex];
        const glm::mat4 worldModel = yamlModel * instance.second;
        const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(worldModel)));

        for (const tinygltf::Primitive& prim : mesh.primitives) {
            if (prim.mode != TINYGLTF_MODE_TRIANGLES) {
                continue;
            }

            auto posIt = prim.attributes.find("POSITION");
            if (posIt == prim.attributes.end()) {
                WARN("Skipping glTF primitive in '%s' with no POSITION", filepath.c_str());
                continue;
            }

            std::vector<glm::vec3> positions;
            if (!ReadVec3Accessor(model, posIt->second, positions) || positions.empty()) {
                WARN("Skipping glTF primitive in '%s' due to invalid POSITION accessor", filepath.c_str());
                continue;
            }

            std::vector<glm::vec3> normals;
            bool hasNormals = false;
            auto nIt = prim.attributes.find("NORMAL");
            if (nIt != prim.attributes.end()) {
                hasNormals = ReadVec3Accessor(model, nIt->second, normals) && (normals.size() == positions.size());
            }

            std::vector<uint32_t> indices;
            if (prim.indices >= 0) {
                if (!ReadIndexAccessor(model, prim.indices, indices)) {
                    WARN("Skipping glTF primitive in '%s' due to invalid index accessor", filepath.c_str());
                    continue;
                }
            } else {
                indices.resize(positions.size());
                for (size_t i = 0; i < indices.size(); i++) {
                    indices[i] = (uint32_t)i;
                }
            }
            if (indices.size() < 3) {
                continue;
            }

            const size_t vOffset = geometry.vertices.size();
            const size_t nOffset = geometry.normals.size();

            geometry.vertices.reserve(geometry.vertices.size() + positions.size());
            for (const glm::vec3& p : positions) {
                geometry.vertices.push_back(glm::vec4(glm::vec3(worldModel * glm::vec4(p, 1.0f)), 0.0f));
            }

            if (hasNormals) {
                geometry.normals.reserve(geometry.normals.size() + normals.size());
                for (glm::vec3 n : normals) {
                    n = normalMatrix * n;
                    if (glm::length(n) > 0.0f) {
                        n = glm::normalize(n);
                    }
                    geometry.normals.push_back(glm::vec4(n, 0.0f));
                }
            }

            uint32_t materialIndex = (uint32_t)materialOffset;
            if (!overrideMaterial && !model.materials.empty() && prim.material >= 0 && (size_t)prim.material < model.materials.size()) {
                materialIndex = (uint32_t)(materialOffset + (size_t)prim.material);
            }

            for (size_t i = 0; i + 2 < indices.size(); i += 3) {
                const uint32_t ia = indices[i + 0];
                const uint32_t ib = indices[i + 1];
                const uint32_t ic = indices[i + 2];
                if (ia >= positions.size() || ib >= positions.size() || ic >= positions.size()) {
                    continue;
                }

                Triangle tri{};
                tri.a = (uint32_t)(vOffset + ia);
                tri.b = (uint32_t)(vOffset + ib);
                tri.c = (uint32_t)(vOffset + ic);
                tri.an = hasNormals ? (uint32_t)(nOffset + ia) : (uint32_t)-1;
                tri.bn = hasNormals ? (uint32_t)(nOffset + ib) : (uint32_t)-1;
                tri.cn = hasNormals ? (uint32_t)(nOffset + ic) : (uint32_t)-1;
                tri.material = materialIndex;
                geometry.triangles.push_back(tri);
            }
        }
    }

    return true;
}

static bool LoadOBJFile(
    const std::string& filepath,
    const Transform& transform,
    Geometry& geometry,
    const Material* overrideMaterial) {
    tinyobj::ObjReaderConfig cfg;
    cfg.triangulate = true;
    cfg.vertex_color = false;
    cfg.mtl_search_path = std::filesystem::path(filepath).parent_path().string();

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(filepath, cfg)) {
        const std::string err = reader.Error();
        ERROR("tinyobj failed to load '%s': %s", filepath.c_str(), err.c_str());
        return false;
    }
    if (!reader.Warning().empty()) {
        WARN("tinyobj warning for '%s': %s", filepath.c_str(), reader.Warning().c_str());
    }

    const tinyobj::attrib_t& attrib = reader.GetAttrib();
    const std::vector<tinyobj::shape_t>& shapes = reader.GetShapes();
    const std::vector<tinyobj::material_t>& materials = reader.GetMaterials();

    if (attrib.vertices.empty()) {
        ERROR("OBJ '%s' contains no vertices", filepath.c_str());
        return false;
    }

    const glm::mat4 model = ComposeTransform(transform);
    const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));

    const size_t vertexOffset = geometry.vertices.size();
    const size_t normalOffset = geometry.normals.size();
    const size_t materialOffset = geometry.materials.size();

    if (overrideMaterial) {
        geometry.materials.push_back(*overrideMaterial);
    } else {
        if (materials.empty()) {
            Material fallback{};
            fallback.ior = 1.0f;
            geometry.materials.push_back(fallback);
        } else {
            for (const tinyobj::material_t& m : materials) {
                geometry.materials.push_back(ConvertMaterial(m));
            }
        }
    }

    const size_t vertexCount = attrib.vertices.size() / 3;
    geometry.vertices.reserve(geometry.vertices.size() + vertexCount);
    for (size_t i = 0; i < vertexCount; i++) {
        const glm::vec3 v(
            attrib.vertices[3 * i + 0],
            attrib.vertices[3 * i + 1],
            attrib.vertices[3 * i + 2]);
        geometry.vertices.push_back(glm::vec4(glm::vec3(model * glm::vec4(v, 1.0f)), 0.0f));
    }

    const size_t normalCount = attrib.normals.size() / 3;
    geometry.normals.reserve(geometry.normals.size() + normalCount);
    for (size_t i = 0; i < normalCount; i++) {
        glm::vec3 n(
            attrib.normals[3 * i + 0],
            attrib.normals[3 * i + 1],
            attrib.normals[3 * i + 2]);
        n = normalMatrix * n;
        if (glm::length(n) > 0.0f) {
            n = glm::normalize(n);
        }
        geometry.normals.push_back(glm::vec4(n, 0.0f));
    }

    for (const tinyobj::shape_t& shape : shapes) {
        size_t indexOffset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            const int fv = shape.mesh.num_face_vertices[f];
            if (fv != 3) {
                indexOffset += (size_t)fv;
                continue;
            }

            const tinyobj::index_t i0 = shape.mesh.indices[indexOffset + 0];
            const tinyobj::index_t i1 = shape.mesh.indices[indexOffset + 1];
            const tinyobj::index_t i2 = shape.mesh.indices[indexOffset + 2];
            indexOffset += 3;

            if (i0.vertex_index < 0 || i1.vertex_index < 0 || i2.vertex_index < 0) {
                WARN("Skipping invalid face in '%s' with missing vertex index", filepath.c_str());
                continue;
            }

            uint32_t materialIndex = (uint32_t)materialOffset;
            if (!overrideMaterial && !materials.empty()) {
                const int matId = (f < shape.mesh.material_ids.size()) ? shape.mesh.material_ids[f] : -1;
                if (matId >= 0 && (size_t)matId < materials.size()) {
                    materialIndex = (uint32_t)(materialOffset + (size_t)matId);
                }
            }

            const bool hasNormals =
                i0.normal_index >= 0 &&
                i1.normal_index >= 0 &&
                i2.normal_index >= 0 &&
                (size_t)i0.normal_index < normalCount &&
                (size_t)i1.normal_index < normalCount &&
                (size_t)i2.normal_index < normalCount;

            Triangle tri{};
            tri.a = (uint32_t)(vertexOffset + (size_t)i0.vertex_index);
            tri.b = (uint32_t)(vertexOffset + (size_t)i1.vertex_index);
            tri.c = (uint32_t)(vertexOffset + (size_t)i2.vertex_index);
            tri.an = hasNormals ? (uint32_t)(normalOffset + (size_t)i0.normal_index) : (uint32_t)-1;
            tri.bn = hasNormals ? (uint32_t)(normalOffset + (size_t)i1.normal_index) : (uint32_t)-1;
            tri.cn = hasNormals ? (uint32_t)(normalOffset + (size_t)i2.normal_index) : (uint32_t)-1;
            tri.material = materialIndex;
            geometry.triangles.push_back(tri);
        }
    }

    return true;
}

static void RebuildDerivedGeometry(Geometry& geometry) {
    geometry.emissives.clear();
    for (size_t i = 0; i < geometry.triangles.size(); i++) {
        uint32_t m = geometry.triangles[i].material;
        if (m < geometry.materials.size()) {
            const glm::vec3 e = geometry.materials[m].emission;
            if (e.x > 0.0f || e.y > 0.0f || e.z > 0.0f) {
                geometry.emissives.push_back((uint32_t)i);
            }
        }
    }

    INFO("Building BVH for geometry with %zu triangles...", geometry.triangles.size());
    geometry.bvh = BVH::Create(geometry.triangles, geometry.vertices);
    INFO("BVH built with %zu nodes", geometry.bvh.size());
    geometry.bvh_size = geometry.bvh.size();
    geometry.vertices_size = geometry.vertices.size();
    geometry.normals_size = geometry.normals.size();
    geometry.triangles_size = geometry.triangles.size();
    geometry.emissives_size = geometry.emissives.size();
    geometry.materials_size = geometry.materials.size();
}

static void ResetGeometry(Geometry& geometry) {
    geometry.bvh.clear();
    geometry.vertices.clear();
    geometry.normals.clear();
    geometry.triangles.clear();
    geometry.emissives.clear();
    geometry.materials.clear();
    geometry.bvh_size = 0;
    geometry.vertices_size = 0;
    geometry.normals_size = 0;
    geometry.triangles_size = 0;
    geometry.emissives_size = 0;
    geometry.materials_size = 0;
}

static bool LoadYAMLScene(const std::string& filepath, Geometry& geometry) {
    YAML::Node root;
    try {
        root = YAML::LoadFile(filepath);
    } catch (const std::exception& e) {
        ERROR("Failed to parse YAML scene '%s': %s", filepath.c_str(), e.what());
        return false;
    }

    if (root["camera"]) {
        ApplyCamera(root["camera"]);
    }

    if (root["meshes"]) {
        if (!root["meshes"].IsSequence()) {
            ERROR("Scene YAML field 'meshes' must be a sequence");
            return false;
        }
        for (size_t i = 0; i < root["meshes"].size(); i++) {
            const YAML::Node mesh = root["meshes"][i];
            if (!mesh["obj"]) {
                if (!mesh["file"]) {
                    ERROR("Mesh entry %d is missing required field 'obj' or 'file'", (int)i);
                    return false;
                }
            }
            const YAML::Node fileNode = mesh["file"] ? mesh["file"] : mesh["obj"];
            const std::string meshPath = ResolvePath(filepath, fileNode.as<std::string>());
            const std::string meshExt = ToLower(std::filesystem::path(meshPath).extension().string());
            const Transform transform = ParseTransform(mesh["transform"]);
            Material overrideMaterial{};
            const Material* overridePtr = nullptr;
            if (ParseMaterialNode(mesh["material"], overrideMaterial)) {
                overridePtr = &overrideMaterial;
            }

            if (meshExt == ".obj") {
                if (!LoadOBJFile(meshPath, transform, geometry, overridePtr)) {
                    return false;
                }
            } else if (meshExt == ".gltf" || meshExt == ".glb") {
                if (!LoadGLTFFile(meshPath, transform, geometry, overridePtr)) {
                    return false;
                }
            } else {
                ERROR("Unsupported mesh extension '%s' for mesh entry %d", meshExt.c_str(), (int)i);
                return false;
            }
        }
    }

    if (root["lights"]) {
        if (!root["lights"].IsSequence()) {
            ERROR("Scene YAML field 'lights' must be a sequence");
            return false;
        }
        for (size_t i = 0; i < root["lights"].size(); i++) {
            if (!AppendEmissiveQuad(root["lights"][i], geometry)) {
                ERROR("Failed to append light entry %d", (int)i);
                return false;
            }
        }
    }

    RebuildDerivedGeometry(geometry);
    return true;
}

} // namespace

bool LoadScene(const std::string& filepath, Geometry& geometry) {
    const std::string ext = ToLower(std::filesystem::path(filepath).extension().string());
    ResetGeometry(geometry);

    if (ext == ".yaml" || ext == ".yml") {
        return LoadYAMLScene(filepath, geometry);
    }
    if (ext == ".obj") {
        const Transform identity;
        if (!LoadOBJFile(filepath, identity, geometry, nullptr)) {
            return false;
        }
        RebuildDerivedGeometry(geometry);
        return true;
    }
    if (ext == ".gltf" || ext == ".glb") {
        const Transform identity;
        if (!LoadGLTFFile(filepath, identity, geometry, nullptr)) {
            return false;
        }
        RebuildDerivedGeometry(geometry);
        return true;
    }

    ERROR("Unsupported scene file extension for '%s'. Expected .yaml/.yml, .obj, .gltf, or .glb", filepath.c_str());
    return false;
}

} // namespace VSTIR::SceneLoader

