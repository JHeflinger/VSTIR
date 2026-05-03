#pragma once

#include "vulkan/vstructs.h"
#include <array>
#include <cstddef>

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
        VulkanDataBuffer& DenoiseOpts() { return m_denoise_opts; }
        std::array<VulkanDataBuffer, 6>& CompImgs() {return m_comp_imgs;}
        void UpdateDescriptors();
        void UpdateUBOs();
        size_t* DenoiseCount() {return &denoise_opts_count;}
        size_t* MaxSize() {
            return &max_size;
        }
    private:
        void InitializeSSBO();
        void InitializeCompImages();
        void InitializeDenoiseOpts();
        void InitializeUBO();
        void InitializeDescriptors();
    private:
        VulkanDescriptors* m_Descriptors;
        UBOArray m_UBOs;
        VulkanDataBuffer m_SSBO;
        VulkanDataBuffer m_denoise_opts;
        std::array<VulkanDataBuffer, 6> m_comp_imgs;
        std::size_t denoise_opts_count = 1;
        size_t max_size;
    };

}
