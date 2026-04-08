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
        uint32_t imageIndex;
        vkAcquireNextImageKHR(
            _interface, _context.Swapchain().swapchain, UINT64_MAX,
            _scheduler.Syncro().imageAvailable, VK_NULL_HANDLE, &imageIndex);

        vkResetCommandBuffer(_scheduler.Commands().command, 0);
        _data.UpdateUBOs();
        RecordCommand(imageIndex);

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount   = 1;
        submitInfo.pWaitSemaphores      = &_scheduler.Syncro().imageAvailable;
        submitInfo.pWaitDstStageMask    = &waitStage;
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &_scheduler.Commands().command;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores    = &_scheduler.Syncro().renderFinished;
        vkQueueSubmit(_scheduler.Queue(), 1, &submitInfo, VK_NULL_HANDLE);

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores    = &_scheduler.Syncro().renderFinished;
        presentInfo.swapchainCount     = 1;
        presentInfo.pSwapchains        = &_context.Swapchain().swapchain;
        presentInfo.pImageIndices      = &imageIndex;
        vkQueuePresentKHR(_scheduler.Queue(), &presentInfo);
        vkQueueWaitIdle(_scheduler.Queue());
    }

    void Renderer::RecordCommand(uint32_t imageIndex) {
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

        // Blit image
        {
            VkImage swapImg = _context.Swapchain().images[imageIndex];
            VkImageMemoryBarrier toTransferDst{};
            toTransferDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            toTransferDst.srcAccessMask = 0;
            toTransferDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toTransferDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            toTransferDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toTransferDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransferDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransferDst.image = swapImg;
            toTransferDst.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            vkCmdPipelineBarrier(_scheduler.Commands().command,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &toTransferDst);
            VkImageBlit blit{};
            blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            blit.srcOffsets[0]  = { 0, 0, 0 };
            blit.srcOffsets[1]  = { (int32_t)_width, (int32_t)_height, 1 };
            blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            blit.dstOffsets[0]  = { 0, 0, 0 };
            blit.dstOffsets[1]  = { (int32_t)_width, (int32_t)_height, 1 };
            vkCmdBlitImage(_scheduler.Commands().command,
                _context.Target().image, VK_IMAGE_LAYOUT_GENERAL,
                swapImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit, VK_FILTER_NEAREST);
            VkImageMemoryBarrier toPresent = toTransferDst;
            toPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toPresent.dstAccessMask = 0;
            toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            vkCmdPipelineBarrier(_scheduler.Commands().command,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                0, 0, nullptr, 0, nullptr, 1, &toPresent);
        }

        // End command
        result = vkEndCommandBuffer(_scheduler.Commands().command);
        if (result != VK_SUCCESS) FATAL("Failed to record command!");
    }

}
