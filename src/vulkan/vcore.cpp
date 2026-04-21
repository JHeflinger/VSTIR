#include "vcore.h"
#include "core/get.h"
#include "util/log.h"
#include "vulkan/vutil.h"
#include "vulkan/vshaders.h"

namespace VSTIR {

    void VCore::InitializeBridge() {
        VUTILS::CreateBuffer(
            _render_width * _render_height * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
            &m_Bridge);
        vkMapMemory(_interface, m_Bridge.memory, 0, VK_WHOLE_SIZE, 0, &(_swapchain.reference));
    }

    void VCore::RecreateBridge() {
        vkUnmapMemory(_interface, m_Bridge.memory);
        VUTILS::DestroyBuffer(m_Bridge);
        m_Bridge = {};
        InitializeBridge();
    }

    void VCore::Reconstruct() {
        VUTILS::DestroyBuffer(m_Geometry.bvh);
        VUTILS::DestroyBuffer(m_Geometry.normals);
        VUTILS::DestroyBuffer(m_Geometry.vertices);
        VUTILS::DestroyBuffer(m_Geometry.emissives);
        VUTILS::DestroyBuffer(m_Geometry.materials);
        VUTILS::DestroyBuffer(m_Geometry.triangles);
        InitializeGeometry();
        m_Context.Reconstruct();
    }

    void VCore::InitializeShaders() {
        m_Shaders.push_back(VSHADERS::GenerateShader("shaders/render.comp", "build/bin/shaders/render.comp.spv"));
    }

    void VCore::InitializeGeometry() {
        InitializeGeometryBuffer(_renderer.GetGeometry().bvh.size(), sizeof(NodeBVH), &(m_Geometry.bvh), _renderer.GetGeometry().bvh.data());
        InitializeGeometryBuffer(_renderer.GetGeometry().normals.size(), sizeof(glm::vec4), &(m_Geometry.normals), _renderer.GetGeometry().normals.data());
        InitializeGeometryBuffer(_renderer.GetGeometry().vertices.size(), sizeof(glm::vec4), &(m_Geometry.vertices), _renderer.GetGeometry().vertices.data());
        InitializeGeometryBuffer(_renderer.GetGeometry().triangles.size(), sizeof(Triangle), &(m_Geometry.triangles), _renderer.GetGeometry().triangles.data());
        InitializeGeometryBuffer(_renderer.GetGeometry().emissives.size(), sizeof(uint32_t), &(m_Geometry.emissives), _renderer.GetGeometry().emissives.data());
        InitializeGeometryBuffer(_renderer.GetGeometry().materials.size(), sizeof(Material), &(m_Geometry.materials), _renderer.GetGeometry().materials.data());
    }

    void VCore::InitializeGeometryBuffer(size_t size, size_t esize, VulkanDataBuffer* buffer, void* start) {
        size_t arrsize = esize * size;
        arrsize = arrsize > 0 ? arrsize : 1;
        VUTILS::CreateBuffer(
            arrsize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            buffer);
        if (size > 0) {
            VUTILS::CopyHostToBuffer(
                start,
                esize * size,
                esize * size,
                buffer->buffer);
        }
    }

    void VCore::Initialize() {
        InitializeShaders();
        m_General.Initialize();
        InitializeGeometry();
        m_Scheduler.Initialize();
        InitializeBridge();
        m_Context.Initialize();
    }

}
