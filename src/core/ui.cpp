#include "ui.h"

#include "core/editor.h"
#include "util/log.h"
#include "vulkan/vutil.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "imgui.h"

#include <array>
#include <algorithm>
#include <cstdio>
#include <vector>

namespace {

VSTIR::VCore& Core() {
    return VSTIR::Editor::Get()->GetRenderer().GetBackend().Core();
}

VSTIR::VContext& Context() {
    return Core().Context();
}

VSTIR::VScheduler& Scheduler() {
    return Core().Scheduler();
}

VkDevice Device() {
    return Core().General().Interface();
}

VkPhysicalDevice GPU() {
    return Core().General().GPU();
}

VkInstance Instance() {
    return Core().General().Instance();
}

bool s_Initialized = false;
uint32_t s_ImageIndex = 0;
VkDescriptorPool s_DescriptorPool = VK_NULL_HANDLE;
VkRenderPass s_RenderPass = VK_NULL_HANDLE;
std::vector<VkFramebuffer> s_Framebuffers;
std::array<float, 180> s_FrameTimesMs{};
int s_FrameTimeCursor = 0;
int s_FrameSamples = 0;
float s_WindowWidth = 420.0f;
float s_WindowHeight = 560.0f;
bool s_ForceLayout = false;

void CheckVkResult(VkResult result) {
    if (result != VK_SUCCESS) {
        FATAL("ImGui Vulkan backend failed with VkResult=%d", (int)result);
    }
}

void CreateDescriptorPool() {
    const VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 64 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 64 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 64 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 64 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 64 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 64 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 64 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 64 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 64 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 64 },
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 64 * (uint32_t)(sizeof(poolSizes) / sizeof(poolSizes[0]));
    poolInfo.poolSizeCount = (uint32_t)(sizeof(poolSizes) / sizeof(poolSizes[0]));
    poolInfo.pPoolSizes = poolSizes;
    CheckVkResult(vkCreateDescriptorPool(Device(), &poolInfo, nullptr, &s_DescriptorPool));
}

void CreateRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = Context().Swapchain().format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    CheckVkResult(vkCreateRenderPass(Device(), &renderPassInfo, nullptr, &s_RenderPass));
}

void CreateFramebuffers() {
    const auto& swap = Context().Swapchain();
    s_Framebuffers.resize(swap.views.size());
    for (size_t i = 0; i < swap.views.size(); i++) {
        VkImageView attachment = swap.views[i];
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = s_RenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &attachment;
        framebufferInfo.width = swap.extent.width;
        framebufferInfo.height = swap.extent.height;
        framebufferInfo.layers = 1;
        CheckVkResult(vkCreateFramebuffer(Device(), &framebufferInfo, nullptr, &s_Framebuffers[i]));
    }
}

void DestroyFramebuffers() {
    for (VkFramebuffer framebuffer : s_Framebuffers) {
        vkDestroyFramebuffer(Device(), framebuffer, nullptr);
    }
    s_Framebuffers.clear();
}

void DestroyRenderPass() {
    if (s_RenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(Device(), s_RenderPass, nullptr);
        s_RenderPass = VK_NULL_HANDLE;
    }
}

}


void UI::initialize(GLFWwindow* window) {
    if (s_Initialized) {
        return;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsClassic();
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg].w = 0.7f;

    ImGui_ImplGlfw_InitForVulkan(window, true);
    CreateDescriptorPool();
    CreateRenderPass();
    CreateFramebuffers();

    VSTIR::VulkanFamilyGroup families = VSTIR::VUTILS::FindQueueFamilies(GPU());
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = Instance();
    initInfo.PhysicalDevice = GPU();
    initInfo.Device = Device();
    initInfo.QueueFamily = families.graphics.value;
    initInfo.Queue = Scheduler().Queue();
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = s_DescriptorPool;
    initInfo.RenderPass = s_RenderPass;
    initInfo.Subpass = 0;
    initInfo.MinImageCount = (uint32_t)Context().Swapchain().images.size();
    initInfo.ImageCount = (uint32_t)Context().Swapchain().images.size();
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.Allocator = nullptr;
    initInfo.CheckVkResultFn = CheckVkResult;

    ImGui_ImplVulkan_Init(&initInfo);

    if (!ImGui_ImplVulkan_CreateFontsTexture()) {
        FATAL("Failed to create ImGui font texture");
    }

    s_Initialized = true;
}

void UI::setImageIndex(uint32_t imageIndex) {
    s_ImageIndex = imageIndex;
}

void UI::recreateSwapchainResources() {
    if (!s_Initialized) {
        return;
    }
    DestroyFramebuffers();
    DestroyRenderPass();
    CreateRenderPass();
    CreateFramebuffers();
    ImGui_ImplVulkan_SetMinImageCount((uint32_t)Context().Swapchain().images.size());
}

void UI::onResize(float width, float height) {
    if (width <= 0.0f || height <= 0.0f) {
        return;
    }
    // Keep UI in the right third of the screen.
    s_WindowWidth = std::max(220.0f, width / 3.0f);
    s_WindowHeight = std::max(220.0f, height);
    s_ForceLayout = true;
}

void UI::drawUI() {
    int winW = 0;
    int winH = 0;
    glfwGetWindowSize(VSTIR::Editor::Get()->Window(), &winW, &winH);
    const float totalW = winW > 0 ? (float)winW : (float)Context().Swapchain().extent.width;
    const float totalH = winH > 0 ? (float)winH : (float)Context().Swapchain().extent.height;
    const float viewportW = totalW * (2.0f / 3.0f);
    const int viewportWidthPx = (int)viewportW;
    const int viewportHeightPx = (int)totalH;
    const float panelW = std::max(220.0f, totalW - viewportW);
    beginDraw(viewportW, 0.0f, panelW, totalH);

    ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
    ImGui::Begin("Performance", nullptr, panelFlags);

    const ImGuiIO& io = ImGui::GetIO();
    const float fps = io.Framerate;
    const float frameMs = fps > 0.0f ? (1000.0f / fps) : 0.0f;

    s_FrameTimesMs[s_FrameTimeCursor] = frameMs;
    s_FrameTimeCursor = (s_FrameTimeCursor + 1) % (int)s_FrameTimesMs.size();
    if (s_FrameSamples < (int)s_FrameTimesMs.size()) {
        s_FrameSamples++;
    }

    float avgMs = 0.0f;
    float maxMs = 0.0f;
    for (int i = 0; i < s_FrameSamples; i++) {
        avgMs += s_FrameTimesMs[i];
        maxMs = std::max(maxMs, s_FrameTimesMs[i]);
    }
    avgMs = s_FrameSamples > 0 ? (avgMs / (float)s_FrameSamples) : 0.0f;

    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Frame Time: %.2f ms", frameMs);
    ImGui::Text("Average (180f): %.2f ms", avgMs);
    ImGui::Text("Viewport Resolution: %d x %d", viewportWidthPx, viewportHeightPx);
    ImGui::Text("Frame Time Graph (ms)");

    const float graphMinMs = 0.0f;
    const float graphMaxMs = std::max(33.0f, maxMs * 1.2f);
    const float budgetMs = 1000.0f / 60.0f;
    ImGui::PlotLines(
        "Frame Time (ms)",
        s_FrameTimesMs.data(),
        s_FrameSamples,
        s_FrameTimeCursor,
        nullptr,
        graphMinMs,
        graphMaxMs,
        ImVec2(0.0f, 100.0f));

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 plotMin = ImGui::GetItemRectMin();
    const ImVec2 plotMax = ImGui::GetItemRectMax();
    const float t = (budgetMs - graphMinMs) / std::max(0.001f, graphMaxMs - graphMinMs);
    const float budgetY = plotMax.y - t * (plotMax.y - plotMin.y);
    drawList->AddLine(ImVec2(plotMin.x, budgetY), ImVec2(plotMax.x, budgetY), IM_COL32(255, 80, 80, 220), 1.5f);

    char budgetLabel[64];
    snprintf(budgetLabel, sizeof(budgetLabel), "60 FPS (%.2f ms)", budgetMs);
    drawList->AddText(ImVec2(plotMin.x + 6.0f, budgetY - 16.0f), IM_COL32(255, 100, 100, 255), budgetLabel);

    ImGui::Text("Y: %.1f ms (top) ... %.1f ms (bottom)", graphMaxMs, graphMinMs);
    ImGui::Text("X: oldest -> newest (%d frames)", s_FrameSamples);

    ImGui::End();
    endDraw();
}

void UI::beginDraw(float x_pos, float y_pos, float width, float height) {
    if (!s_Initialized || s_ImageIndex >= s_Framebuffers.size()) {
        return;
    }

    VkImageMemoryBarrier toColor{};
    toColor.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toColor.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toColor.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toColor.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toColor.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toColor.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toColor.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toColor.image = Context().Swapchain().images[s_ImageIndex];
    toColor.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(
        Scheduler().Commands().command,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &toColor);

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = s_RenderPass;
    renderPassInfo.framebuffer = s_Framebuffers[s_ImageIndex];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = Context().Swapchain().extent;
    renderPassInfo.clearValueCount = 0;
    renderPassInfo.pClearValues = nullptr;
    vkCmdBeginRenderPass(Scheduler().Commands().command, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    const ImGuiCond cond = s_ForceLayout ? ImGuiCond_Always : ImGuiCond_Once;
    const float nextWidth = s_ForceLayout ? s_WindowWidth : width;
    const float nextHeight = s_ForceLayout ? s_WindowHeight : height;
    ImGui::SetNextWindowPos({ x_pos, y_pos }, cond);
    ImGui::SetNextWindowSize({ nextWidth, nextHeight }, cond);
    s_ForceLayout = false;
}

void UI::endDraw() {
    if (!s_Initialized || s_ImageIndex >= s_Framebuffers.size()) {
        return;
    }

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), Scheduler().Commands().command);
    vkCmdEndRenderPass(Scheduler().Commands().command);
}