#pragma once

#include "vulkan/vstructs.h"

namespace VSTIR {

    class VData {
    public:
        VData() {};
        ~VData() { free(m_Descriptors); };
    public:
        void Initialize();
        void Reconstruct();
        void RecreateSSBO();
        VulkanDescriptors* Descriptors() { return m_Descriptors; }
        UBOArray& UBOs() { return m_UBOs; }
        VulkanDataBuffer& SSBO() { return m_SSBO; }
        void UpdateDescriptors();
        void UpdateUBOs();
    private:
        void InitializeSSBO();
        void InitializeUBO();
        void InitializeDescriptors();
    private:
        VulkanDescriptors* m_Descriptors;
        UBOArray m_UBOs;
        VulkanDataBuffer m_SSBO;
    };

}
