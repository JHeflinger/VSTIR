#include "renderer.h"
#include "core/editor.h"
#include "core/get.h"
#include "util/log.h"
#include "vulkan/vutil.h"
#include <vulkan/vulkan.h>

#define INVOCATION_GROUP_SIZE 256

namespace VSTIR {

    void Renderer::Initialize() {
        m_Backend.Initialize();
    }

    void Renderer::Render() {
        vkDeviceWaitIdle(_interface);
        _data.UpdateUBOs();
        vkResetCommandBuffer(_scheduler.Commands().command, 0);
        RecordCommand();
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 0;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &(_scheduler.Commands().command);
        submitInfo.signalSemaphoreCount = 0;
        VkResult result = vkQueueSubmit(_scheduler.Queue(), 1, &submitInfo, VK_NULL_HANDLE);
        ASSERT(result == VK_SUCCESS, "failed to submit draw command buffer!");
    }

    void Renderer::RecordCommand() {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        VkResult result = vkBeginCommandBuffer(_scheduler.Commands().command, &beginInfo);
        ASSERT(result == VK_SUCCESS, "Failed to begin recording command buffer!");

        // execute shader stages
        for (size_t i = 0; i < _shaders.size(); i++) {
            uint32_t invocations = _width * _height;
            vkCmdBindPipeline(
                _scheduler.Commands().command,
                VK_PIPELINE_BIND_POINT_COMPUTE,
                _context.Pipeline().pipeline[i]);
            vkCmdBindDescriptorSets(
                _scheduler.Commands().command,
                VK_PIPELINE_BIND_POINT_COMPUTE,
                _context.Pipeline().layout[i],
                0,
                1,
                &(_data.Descriptors()[i].set),
                0,
                nullptr);
            vkCmdDispatch(_scheduler.Commands().command, ceil((invocations) / ((float)INVOCATION_GROUP_SIZE)), 1, 1);
            VUTILS::RecordGeneralBarrier(_scheduler.Commands().command);
        }

        // Copy image to staging
        {
            VkImageMemoryBarrier imgBarrier{};
            imgBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imgBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            imgBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            imgBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            imgBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            imgBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imgBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imgBarrier.image = _context.Target().image;
            imgBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imgBarrier.subresourceRange.baseMipLevel = 0;
            imgBarrier.subresourceRange.levelCount = 1;
            imgBarrier.subresourceRange.baseArrayLayer = 0;
            imgBarrier.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(
                _scheduler.Commands().command,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &imgBarrier);
            VkBufferImageCopy region{};
            region.bufferOffset = 0;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = 1;
            region.imageOffset = (VkOffset3D){ 0, 0, 0 };
            region.imageExtent = (VkExtent3D){ (uint32_t)_width, (uint32_t)_height, 1 };
            vkCmdCopyImageToBuffer(
                _scheduler.Commands().command,
                _context.Target().image,
                VK_IMAGE_LAYOUT_GENERAL, _core.Bridge().buffer, 1, &region);
        }

        // End command
        result = vkEndCommandBuffer(_scheduler.Commands().command);
        if (result != VK_SUCCESS) FATAL("Failed to record command!");
    }

}
