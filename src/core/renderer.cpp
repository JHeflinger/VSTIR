#include "renderer.h"
#include "core/editor.h"
#include "core/get.h"
#include "core/scene_loader.h"
#include "core/ui.h"
#include "util/log.h"
#include "vulkan/vutil.h"
#include <vulkan/vulkan.h>
#include <vector>

#define INVOCATION_GROUP_SIZE 256
namespace VSTIR {

    static std::vector<bool> s_SwapImageInitialized;

    enum class ShaderStage {
        Render,
        Reservoir,
        Temporal,
        Spatial,
        Compile,
        Bilateral,
        History,
        Merge,
    };

    struct ShaderStageInfo {
        ShaderStage stage;
        bool requiresRestir;
        bool isFilter;
    };

    static constexpr ShaderStageInfo kShaderStages[] = {
        { ShaderStage::Render,    false, false },
        { ShaderStage::Reservoir, true,  false },
        { ShaderStage::Temporal,  true,  false },
        { ShaderStage::Spatial,   true,  false },
        { ShaderStage::Compile,   true,  false },
        { ShaderStage::Bilateral, true,  true  },
        { ShaderStage::History,   true,  false },
        { ShaderStage::Merge,     false, false },
    };
    static constexpr size_t kShaderStageCount = sizeof(kShaderStages) / sizeof(kShaderStages[0]);

    static uint32_t WorkgroupCount1D(uint32_t count, uint32_t localSize) {
        return (count + localSize - 1u) / localSize;
    }

    void Renderer::Initialize() {
        m_Backend.Initialize();
        _scheduler.RecreateRenderFinishedSemaphores((uint32_t)_context.Swapchain().images.size());
        s_SwapImageInitialized.assign(_context.Swapchain().images.size(), false);
        _render_settings._last_render_width = _render_width;
        _render_settings._last_render_height = _render_height;
        UI::initialize(_window);
        m_Camera = camera();
    }

    void Renderer::Render() {
        int fbWidth = 0, fbHeight = 0;
        glfwGetWindowSize(_window, &fbWidth, &fbHeight);
        if (fbWidth <= 0 || fbHeight <= 0) {
            return;
        }

        // CPU/GPU frame pacing without queue idle every frame.
        VkResult fenceWait = vkWaitForFences(_interface, 1, &_scheduler.Syncro().fence, VK_TRUE, UINT64_MAX);
        if (fenceWait != VK_SUCCESS) {
            FATAL("vkWaitForFences failed with VkResult=%d", (int)fenceWait);
        }

        const bool extentMismatch =
            _context.Swapchain().extent.width != (uint32_t)fbWidth ||
            _context.Swapchain().extent.height != (uint32_t)fbHeight;
        const bool scaleMismatch =
            _render_width != _render_settings._last_render_width ||
            _render_height != _render_settings._last_render_height;
        if (extentMismatch || scaleMismatch) {
            Resize((uint32_t)fbWidth, (uint32_t)fbHeight);
            return;
        }

        uint32_t imageIndex;
        VkResult acquireResult = vkAcquireNextImageKHR(
            _interface, _context.Swapchain().swapchain, UINT64_MAX,
            _scheduler.Syncro().imageAvailable, VK_NULL_HANDLE, &imageIndex);
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
            Resize((uint32_t)fbWidth, (uint32_t)fbHeight);
            return;
        }
        if (acquireResult == VK_SUBOPTIMAL_KHR) {
            // Continue this frame so imageAvailable semaphore is consumed by submit.
        } else if (acquireResult != VK_SUCCESS) {
            FATAL("vkAcquireNextImageKHR failed with VkResult=%d", (int)acquireResult);
        }
        _swapchain.index = imageIndex;

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
        VkSemaphore renderFinishedSemaphore = _scheduler.Syncro().renderFinished[imageIndex % _scheduler.Syncro().renderFinished.size()];
        submitInfo.pSignalSemaphores    = &renderFinishedSemaphore;
        VkResult fenceReset = vkResetFences(_interface, 1, &_scheduler.Syncro().fence);
        if (fenceReset != VK_SUCCESS) {
            FATAL("vkResetFences failed with VkResult=%d", (int)fenceReset);
        }
        VkResult submitResult = vkQueueSubmit(_scheduler.Queue(), 1, &submitInfo, _scheduler.Syncro().fence);
        if (submitResult != VK_SUCCESS) {
            FATAL("vkQueueSubmit failed with VkResult=%d", (int)submitResult);
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores    = &renderFinishedSemaphore;
        presentInfo.swapchainCount     = 1;
        presentInfo.pSwapchains        = &_context.Swapchain().swapchain;
        presentInfo.pImageIndices      = &imageIndex;
        VkResult presentResult = vkQueuePresentKHR(_scheduler.Queue(), &presentInfo);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
            Resize((uint32_t)fbWidth, (uint32_t)fbHeight);
            return;
        }
        if (presentResult == VK_SUBOPTIMAL_KHR) {
            // Ignore persistent suboptimal states until extent actually changes.
        } else if (presentResult != VK_SUCCESS) {
            FATAL("vkQueuePresentKHR failed with VkResult=%d", (int)presentResult);
        }
    }

    void Renderer::Resize(uint32_t width, uint32_t height) {
        if (width == 0 || height == 0) {
            return;
        }

        const bool extentMismatch =
            _context.Swapchain().extent.width != width ||
            _context.Swapchain().extent.height != height;
        const bool scaleMismatch =
            _render_width != _render_settings._last_render_width ||
            _render_height != _render_settings._last_render_height;

        if (!extentMismatch && !scaleMismatch) {
            return;
        }

        vkDeviceWaitIdle(_interface);

        if (extentMismatch) {
            _context.ResizeSwapchain(width, height);
            _scheduler.RecreateRenderFinishedSemaphores((uint32_t)_context.Swapchain().images.size());
            s_SwapImageInitialized.assign(_context.Swapchain().images.size(), false);
            UI::recreateSwapchainResources();
        }

        if (scaleMismatch) {
            _context.ResizeTarget();
            _data.RecreateSSBO();
            _render_settings.sample_count = 0;
            _render_settings.restir_history_count = 0;
        }

        if (scaleMismatch) {
            _data.UpdateDescriptors();
        }

        _render_settings._last_render_width  = _render_width;
        _render_settings._last_render_height = _render_height;
    }

    void Renderer::LoadScene(std::string filepath) {
        vkDeviceWaitIdle(_interface);
        if (!SceneLoader::LoadScene(filepath, m_Geometry)) {
            return;
        }
        m_settings.sample_count = 0;
        m_settings.restir_history_count = 0;
        m_Backend.Reconstruct();
    }

    void Renderer::RecordCommand(uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        VkResult result = vkBeginCommandBuffer(_scheduler.Commands().command, &beginInfo);
        ASSERT(result == VK_SUCCESS, "Failed to begin recording command buffer!");

        // Execute shader stages in the same order as VCore::InitializeShaders().
        if (_shaders.size() != kShaderStageCount) {
            FATAL("Shader registration count does not match render stage metadata");
        }
        constexpr uint32_t atrousPasses = 4;
        const bool anyRestir =
            _render_settings.show_divider ?
            (_render_settings.restir || _render_settings.restir_right) :
            _render_settings.restir;
        const bool anyBilateral =
            _render_settings.show_divider ?
            ((_render_settings.restir && _render_settings.bilateral) ||
             (_render_settings.restir_right && _render_settings.bilateral_right)) :
            (_render_settings.restir && _render_settings.bilateral);
        for (size_t i = 0; i < _shaders.size(); i++) {
            const ShaderStageInfo& stage = kShaderStages[i];
            if (stage.requiresRestir && !anyRestir) {
                continue;
            }
            if (stage.isFilter && !anyBilateral) {
                continue;
            }
            const uint32_t passCount = stage.isFilter ? atrousPasses : 1u;
            for (uint32_t pass = 0; pass < passCount; pass++) {
                uint32_t invocations = _render_width * _render_height;
                if (stage.isFilter) {
                    uint32_t pc = pass;
                    vkCmdPushConstants(
                        _scheduler.Commands().command,
                        _context.Pipeline().layout[i],
                        VK_SHADER_STAGE_COMPUTE_BIT,
                        0, sizeof(uint32_t), &pc);
                }
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
                vkCmdDispatch(_scheduler.Commands().command, WorkgroupCount1D(invocations, INVOCATION_GROUP_SIZE), 1, 1);
                VUTILS::RecordGeneralBarrier(_scheduler.Commands().command);
            }
        }

        // Prepare shader output for the swapchain blit.
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
        }

        // Blit image
        {
            VkImage swapImg = _context.Swapchain().images[imageIndex];
            const bool wasInitialized = imageIndex < s_SwapImageInitialized.size() && s_SwapImageInitialized[imageIndex];
            VkImageMemoryBarrier toTransferDst{};
            toTransferDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            toTransferDst.srcAccessMask = 0;
            toTransferDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toTransferDst.oldLayout = wasInitialized ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_UNDEFINED;
            toTransferDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toTransferDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransferDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransferDst.image = swapImg;
            toTransferDst.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            vkCmdPipelineBarrier(_scheduler.Commands().command,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &toTransferDst);

            VkClearColorValue clearColor = { { 0.02f, 0.02f, 0.02f, 1.0f } };
            VkImageSubresourceRange clearRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            vkCmdClearColorImage(
                _scheduler.Commands().command,
                swapImg,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                &clearColor,
                1,
                &clearRange);

            const int swapW = (int)_context.Swapchain().extent.width;
            const int swapH = (int)_context.Swapchain().extent.height;
            const int viewportW = (int)(_viewport_width > (size_t)swapW ? (size_t)swapW : _viewport_width);
            const int viewportH = (int)(_viewport_height > (size_t)swapH ? (size_t)swapH : _viewport_height);
            float scaleX = (float)_render_width  / (float)viewportW;
            float scaleY = (float)_render_height / (float)viewportH;
            float scale  = (scaleX < scaleY) ? scaleX : scaleY;
            float srcW = viewportW * scale;
            float srcH = viewportH * scale;
            int32_t srcX0 = (int32_t)(((float)_render_width  - srcW) * 0.5f);
            int32_t srcY0 = (int32_t)(((float)_render_height - srcH) * 0.5f);
            int32_t srcX1 = srcX0 + (int32_t)srcW;
            int32_t srcY1 = srcY0 + (int32_t)srcH;
            VkImageBlit blit{};
            blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            blit.srcOffsets[0]  = { srcX0, srcY0, 0 };
            blit.srcOffsets[1]  = { srcX1, srcY1, 1 };
            blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            blit.dstOffsets[0]  = { 0, 0, 0 };
            blit.dstOffsets[1]  = { viewportW, viewportH, 1 };
            vkCmdBlitImage(_scheduler.Commands().command,
                _context.Target().image, VK_IMAGE_LAYOUT_GENERAL,
                swapImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit, VK_FILTER_LINEAR);

            if (imageIndex < s_SwapImageInitialized.size()) {
                s_SwapImageInitialized[imageIndex] = true;
            }
        }

        UI::setImageIndex(_swapchain.index);
        UI::drawUI();

        // End command
        result = vkEndCommandBuffer(_scheduler.Commands().command);
        if (result != VK_SUCCESS) FATAL("Failed to record command!");
    }

}
