#include "vcore.h"
#include "core/get.h"
#include "vulkan/vutil.h"
#include "vulkan/vshaders.h"

namespace VSTIR {

    void VCore::InitializeBridge() {
        VUTILS::CreateBuffer(
            _width * _height * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
            &m_Bridge);
        vkMapMemory(_interface, m_Bridge.memory, 0, VK_WHOLE_SIZE, 0, &(_swapchain.reference));
    }

    void VCore::InitializeShaders() {
        m_Shaders.push_back(VSHADERS::GenerateShader("shaders/render.comp", "build/bin/shaders/render.comp.spv"));
    }

    void VCore::Initialize() {
        InitializeShaders();
        m_General.Initialize();
        // TODO: m_Geometry.Initialize();
        m_Scheduler.Initialize();
        InitializeBridge();
        m_Context.Initialize();
    }

}
