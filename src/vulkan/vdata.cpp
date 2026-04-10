#include "vdata.h"
#include "vulkan/vutil.h"
#include "core/get.h"
#include "util/log.h"
#include <cstring>

namespace VSTIR {

    void VData::Initialize() {
        m_Descriptors = (VulkanDescriptors*)calloc(_shaders.size(), sizeof(VulkanDescriptors));
        InitializeSSBO();
        InitializeUBO();
        InitializeDescriptors();
    }

    void VData::Reconstruct() {
        UpdateDescriptors();
    }

    void VData::InitializeSSBO() {
        uint32_t imgw = _width;
        uint32_t imgh = _height;
        RayGenerator* raygens = (RayGenerator*)calloc(imgw * imgh, sizeof(RayGenerator));
        for (size_t i = 0; i < imgw * imgh; i++) raygens[i].tid = (uint32_t)-1;

        VkDeviceSize bufferSize = sizeof(RayGenerator) * imgw * imgh;
        VulkanDataBuffer stagingBuffer;
        VUTILS::CreateBuffer(
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &stagingBuffer);
        void* data;
        vkMapMemory(_interface, stagingBuffer.memory, 0, bufferSize, 0, &data);
        memcpy(data, raygens, (size_t)bufferSize);
        vkUnmapMemory(_interface, stagingBuffer.memory);

        VUTILS::CreateBuffer(
            bufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &(m_SSBO));
        VUTILS::CopyBuffer(stagingBuffer.buffer, m_SSBO.buffer, bufferSize);

        vkDestroyBuffer(_interface, stagingBuffer.buffer, nullptr);
        vkFreeMemory(_interface, stagingBuffer.memory, nullptr);
        free(raygens);
    }

    void VData::InitializeUBO() {
        VUTILS::CreateBuffer(
            sizeof(UniformBufferObject),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &(m_UBOs.object));
        vkMapMemory(
            _interface,
            m_UBOs.object.memory,
            0, sizeof(UniformBufferObject), 0, &(m_UBOs.mapped));
    }

    void VData::InitializeDescriptors() {
        size_t num_shaders = _shaders.size();
        for (size_t i = 0; i < num_shaders; i++) {
            VulkanShader& shader = _shaders[i];
            size_t vars = shader.variables.size();
            VkDescriptorPoolSize* poolSizes = (VkDescriptorPoolSize*)calloc(vars, sizeof(VkDescriptorPoolSize));
            VkDescriptorSetLayoutBinding* bindings = (VkDescriptorSetLayoutBinding*)calloc(vars, sizeof(VkDescriptorSetLayoutBinding));
            for (size_t j = 0; j < vars; j++) {
                bindings[j].binding = j;
                bindings[j].descriptorType = (VkDescriptorType)(shader.variables[j].type);
                bindings[j].descriptorCount = 1;
                bindings[j].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
                poolSizes[j].type = (VkDescriptorType)(shader.variables[j].type);
                poolSizes[j].descriptorCount = 1;
            }
            VkDescriptorSetLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = vars;
            layoutInfo.pBindings = bindings;
            VkResult result = vkCreateDescriptorSetLayout(_interface, &layoutInfo, nullptr, &(m_Descriptors[i].layout));
            if (result != VK_SUCCESS) FATAL("Failed to create descriptor set layout!");
            VkDescriptorPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.poolSizeCount = vars;
            poolInfo.pPoolSizes = poolSizes;
            poolInfo.maxSets = 1;
            result = vkCreateDescriptorPool(_interface, &poolInfo, nullptr, &(m_Descriptors[i].pool));
            if (result != VK_SUCCESS) FATAL("Failed to create descriptor pool!");
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = m_Descriptors[i].pool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &(m_Descriptors[i].layout);
            result = vkAllocateDescriptorSets(_interface, &allocInfo, &(m_Descriptors[i].set));
            if (result != VK_SUCCESS) FATAL("Failed to create descriptor sets!");
            free(poolSizes);
            free(bindings);
        }
        UpdateDescriptors();
    }

    void VData::UpdateDescriptors() {
        size_t num_shaders = _shaders.size();
        for (size_t i = 0; i < num_shaders; i++) {
            VulkanShader& shader = _shaders[i];
            size_t vars = shader.variables.size();
            VkDescriptorBufferInfo* bufferInfos = (VkDescriptorBufferInfo*)calloc(vars, sizeof(VkDescriptorBufferInfo));
            VkDescriptorImageInfo* imageInfos = (VkDescriptorImageInfo*)calloc(vars, sizeof(VkDescriptorImageInfo));
            VkWriteDescriptorSet* descriptorWrites = (VkWriteDescriptorSet*)calloc(vars, sizeof(VkWriteDescriptorSet));
            for (size_t k = 0; k < vars; k++) {
                VulkanBoundVariable var = shader.variables[k];
                descriptorWrites[k].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrites[k].dstSet = m_Descriptors[i].set;
                descriptorWrites[k].dstBinding = k;
                descriptorWrites[k].dstArrayElement = 0;
                descriptorWrites[k].descriptorType = (VkDescriptorType)var.type;
                descriptorWrites[k].descriptorCount = 1;
                if (var.type == STORAGE_IMAGE) {
                    imageInfos[k].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                    imageInfos[k].imageView = var.data.reference ? *((VkImageView*)var.data.value) : (VkImageView)var.data.value;
                    descriptorWrites[k].pImageInfo = &(imageInfos[k]);
                } else {
                    bufferInfos[k].buffer = var.data.reference ? *((VkBuffer*)var.data.value) : (VkBuffer)var.data.value;
                    bufferInfos[k].offset = 0;
                    bufferInfos[k].range = var.size.size * ceil((var.size.count.reference ? 
                        (*((size_t*)var.size.count.value)) :
                        (size_t)var.size.count.value) / (var.size.reduction > 0.0f ? var.size.reduction : 1.0f));
                    bufferInfos[k].range = bufferInfos[k].range > 0 ? bufferInfos[k].range : 1;
                    descriptorWrites[k].pBufferInfo = &(bufferInfos[k]);
                }
            }
            vkUpdateDescriptorSets(_interface, vars, descriptorWrites, 0, nullptr);
            free(bufferInfos);
            free(imageInfos);
            free(descriptorWrites);
        }
    }

    void VData::UpdateUBOs() {
        UniformBufferObject ubo{};
        ubo.triangles = _renderer.GetGeometry().triangles.size();
        ubo.width = _width;
        ubo.height = _height;
        memcpy(m_UBOs.mapped, &ubo, sizeof(UniformBufferObject));
    }

}
