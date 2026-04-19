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
#include <string>
#include <glm/glm.hpp>

#include "get.h"
#include "nfd.h"

// ============================================================
// Internal state
// ============================================================

namespace VSTIR {


// ----- Vulkan UI resources -----

bool                       s_Initialized    = false;
uint32_t                   s_ImageIndex     = 0;
VkDescriptorPool           s_DescriptorPool = VK_NULL_HANDLE;
VkRenderPass               s_RenderPass     = VK_NULL_HANDLE;
std::vector<VkFramebuffer> s_Framebuffers;
float                      s_WindowWidth    = 420.0f;
float                      s_WindowHeight   = 560.0f;
bool                       s_ForceLayout    = false;

// ----- Performance state -----

std::array<float, 180> s_FrameTimesMs{};
int                    s_FrameTimeCursor = 0;
int                    s_FrameSamples    = 0;

// ============================================================
// Vulkan helpers
// ============================================================

void CheckVkResult(VkResult result) {
    if (result != VK_SUCCESS)
        FATAL("ImGui Vulkan backend failed with VkResult=%d", (int)result);
}

void CreateDescriptorPool() {
    const VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER,                64 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          64 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          64 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   64 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   64 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         64 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         64 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 64 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 64 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       64 },
    };
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets       = 64 * (uint32_t)(sizeof(poolSizes) / sizeof(poolSizes[0]));
    poolInfo.poolSizeCount = (uint32_t)(sizeof(poolSizes) / sizeof(poolSizes[0]));
    poolInfo.pPoolSizes    = poolSizes;
    CheckVkResult(vkCreateDescriptorPool(_interface, &poolInfo, nullptr, &s_DescriptorPool));
}

void CreateRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = _context.Swapchain().format;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorRef;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments    = &colorAttachment;
    rpInfo.subpassCount    = 1;
    rpInfo.pSubpasses      = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies   = &dep;

    CheckVkResult(vkCreateRenderPass(_interface, &rpInfo, nullptr, &s_RenderPass));
}

void CreateFramebuffers() {
    const auto& swap = _context.Swapchain();
    s_Framebuffers.resize(swap.views.size());
    for (size_t i = 0; i < swap.views.size(); i++) {
        VkImageView           attachment = swap.views[i];
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass      = s_RenderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments    = &attachment;
        fbInfo.width           = swap.extent.width;
        fbInfo.height          = swap.extent.height;
        fbInfo.layers          = 1;
        CheckVkResult(vkCreateFramebuffer(_interface, &fbInfo, nullptr, &s_Framebuffers[i]));
    }
}

void DestroyFramebuffers() {
    for (VkFramebuffer fb : s_Framebuffers)
        vkDestroyFramebuffer(_interface, fb, nullptr);
    s_Framebuffers.clear();
}

void DestroyRenderPass() {
    if (s_RenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(_interface, s_RenderPass, nullptr);
        s_RenderPass = VK_NULL_HANDLE;
    }
}

} // namespace

// ============================================================
// Widgets
// ============================================================

// ============================================================
// Theme
// ============================================================

namespace VSTIR {
    void UI::setTheme() {
        ImGuiStyle& style = ImGui::GetStyle();

        style.WindowRounding    = 4.0f;
        style.FrameRounding     = 3.0f;
        style.GrabRounding      = 3.0f;
        style.ScrollbarRounding = 3.0f;
        style.TabRounding       = 3.0f;
        style.PopupRounding     = 3.0f;
        style.ChildRounding     = 3.0f;

        style.WindowPadding    = ImVec2(10.0f, 10.0f);
        style.FramePadding     = ImVec2(6.0f,   4.0f);
        style.ItemSpacing      = ImVec2(8.0f,   6.0f);
        style.ItemInnerSpacing = ImVec2(6.0f,   4.0f);
        style.IndentSpacing    = 14.0f;
        style.ScrollbarSize    = 12.0f;
        style.GrabMinSize      = 10.0f;
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize  = 1.0f;

        ImVec4* c = style.Colors;

        c[ImGuiCol_Text]         = ImVec4(0.93f, 0.87f, 0.87f, 1.00f);
        c[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.32f, 0.32f, 1.00f);

        c[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.02f, 0.02f, 0.97f);
        c[ImGuiCol_ChildBg]  = ImVec4(0.08f, 0.03f, 0.03f, 0.80f);
        c[ImGuiCol_PopupBg]  = ImVec4(0.08f, 0.03f, 0.03f, 0.97f);

        c[ImGuiCol_Border]       = ImVec4(0.38f, 0.09f, 0.09f, 0.65f);
        c[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

        c[ImGuiCol_FrameBg]        = ImVec4(0.13f, 0.04f, 0.04f, 1.00f);
        c[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.07f, 0.07f, 1.00f);
        c[ImGuiCol_FrameBgActive]  = ImVec4(0.30f, 0.10f, 0.10f, 1.00f);

        c[ImGuiCol_TitleBg]          = ImVec4(0.08f, 0.02f, 0.02f, 1.00f);
        c[ImGuiCol_TitleBgActive]    = ImVec4(0.25f, 0.05f, 0.05f, 1.00f);
        c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.05f, 0.01f, 0.01f, 0.80f);

        c[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.03f, 0.03f, 1.00f);

        c[ImGuiCol_ScrollbarBg]          = ImVec4(0.04f, 0.01f, 0.01f, 0.90f);
        c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.40f, 0.08f, 0.08f, 1.00f);
        c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.55f, 0.12f, 0.12f, 1.00f);
        c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.70f, 0.16f, 0.16f, 1.00f);

        c[ImGuiCol_CheckMark]        = ImVec4(0.88f, 0.25f, 0.25f, 1.00f);
        c[ImGuiCol_SliderGrab]       = ImVec4(0.65f, 0.13f, 0.13f, 1.00f);
        c[ImGuiCol_SliderGrabActive] = ImVec4(0.85f, 0.22f, 0.22f, 1.00f);

        c[ImGuiCol_Button]        = ImVec4(0.33f, 0.06f, 0.06f, 1.00f);
        c[ImGuiCol_ButtonHovered] = ImVec4(0.50f, 0.10f, 0.10f, 1.00f);
        c[ImGuiCol_ButtonActive]  = ImVec4(0.68f, 0.16f, 0.16f, 1.00f);

        c[ImGuiCol_Header]        = ImVec4(0.30f, 0.06f, 0.06f, 1.00f);
        c[ImGuiCol_HeaderHovered] = ImVec4(0.45f, 0.09f, 0.09f, 1.00f);
        c[ImGuiCol_HeaderActive]  = ImVec4(0.60f, 0.14f, 0.14f, 1.00f);

        c[ImGuiCol_Separator]        = ImVec4(0.40f, 0.10f, 0.10f, 0.80f);
        c[ImGuiCol_SeparatorHovered] = ImVec4(0.60f, 0.15f, 0.15f, 1.00f);
        c[ImGuiCol_SeparatorActive]  = ImVec4(0.75f, 0.20f, 0.20f, 1.00f);

        c[ImGuiCol_ResizeGrip]        = ImVec4(0.40f, 0.08f, 0.08f, 0.40f);
        c[ImGuiCol_ResizeGripHovered] = ImVec4(0.60f, 0.13f, 0.13f, 0.70f);
        c[ImGuiCol_ResizeGripActive]  = ImVec4(0.80f, 0.18f, 0.18f, 1.00f);

        c[ImGuiCol_Tab]                = ImVec4(0.20f, 0.04f, 0.04f, 1.00f);
        c[ImGuiCol_TabHovered]         = ImVec4(0.45f, 0.09f, 0.09f, 1.00f);
        c[ImGuiCol_TabActive]          = ImVec4(0.35f, 0.08f, 0.08f, 1.00f);
        c[ImGuiCol_TabUnfocused]       = ImVec4(0.10f, 0.02f, 0.02f, 1.00f);
        c[ImGuiCol_TabUnfocusedActive] = ImVec4(0.22f, 0.05f, 0.05f, 1.00f);

        c[ImGuiCol_PlotLines]            = ImVec4(0.80f, 0.30f, 0.30f, 1.00f);
        c[ImGuiCol_PlotLinesHovered]     = ImVec4(1.00f, 0.45f, 0.45f, 1.00f);
        c[ImGuiCol_PlotHistogram]        = ImVec4(0.70f, 0.15f, 0.15f, 1.00f);
        c[ImGuiCol_PlotHistogramHovered] = ImVec4(0.90f, 0.25f, 0.25f, 1.00f);

        c[ImGuiCol_TableHeaderBg]     = ImVec4(0.20f, 0.04f, 0.04f, 1.00f);
        c[ImGuiCol_TableBorderStrong] = ImVec4(0.40f, 0.08f, 0.08f, 1.00f);
        c[ImGuiCol_TableBorderLight]  = ImVec4(0.25f, 0.05f, 0.05f, 1.00f);
        c[ImGuiCol_TableRowBg]        = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        c[ImGuiCol_TableRowBgAlt]     = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);

        c[ImGuiCol_TextSelectedBg]        = ImVec4(0.50f, 0.10f, 0.10f, 0.50f);
        c[ImGuiCol_DragDropTarget]        = ImVec4(0.90f, 0.25f, 0.25f, 0.90f);
        c[ImGuiCol_NavHighlight]          = ImVec4(0.85f, 0.20f, 0.20f, 1.00f);
        c[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        c[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.10f, 0.02f, 0.02f, 0.50f);
    }

    // ============================================================
    // Lifecycle
    // ============================================================

    void UI::initialize(GLFWwindow* window) {
        if (s_Initialized) return;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        setTheme();

        ImGui_ImplGlfw_InitForVulkan(window, true);
        CreateDescriptorPool();
        CreateRenderPass();
        CreateFramebuffers();

        VSTIR::VulkanFamilyGroup families = VSTIR::VUTILS::FindQueueFamilies(_gpu);
        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.Instance        = _general.Instance();
        initInfo.PhysicalDevice  = _gpu;
        initInfo.Device          = _interface;
        initInfo.QueueFamily     = families.graphics.value;
        initInfo.Queue           = _scheduler.Queue();
        initInfo.PipelineCache   = VK_NULL_HANDLE;
        initInfo.DescriptorPool  = s_DescriptorPool;
        initInfo.RenderPass      = s_RenderPass;
        initInfo.Subpass         = 0;
        initInfo.MinImageCount   = (uint32_t)_context.Swapchain().images.size();
        initInfo.ImageCount      = (uint32_t)_context.Swapchain().images.size();
        initInfo.MSAASamples     = VK_SAMPLE_COUNT_1_BIT;
        initInfo.Allocator       = nullptr;
        initInfo.CheckVkResultFn = CheckVkResult;

        ImGui_ImplVulkan_Init(&initInfo);

        if (!ImGui_ImplVulkan_CreateFontsTexture())
            FATAL("Failed to create ImGui font texture");

        s_Initialized = true;
    }

    void UI::setImageIndex(uint32_t imageIndex) {
        s_ImageIndex = imageIndex;
    }

    void UI::recreateSwapchainResources() {
        if (!s_Initialized) return;
        DestroyFramebuffers();
        DestroyRenderPass();
        CreateRenderPass();
        CreateFramebuffers();
        ImGui_ImplVulkan_SetMinImageCount((uint32_t)_context.Swapchain().images.size());
    }

    void UI::onResize(float width, float height) {
        if (width <= 0.0f || height <= 0.0f) return;
        const float viewportW = (float)_viewport_width;
        const float viewportH = (float)_viewport_height;
        s_WindowWidth  = std::max(220.0f, width - viewportW);
        s_WindowHeight = std::max(220.0f, viewportH);
        s_ForceLayout  = true;
    }

    // ============================================================
    // Main draw dispatch
    // ============================================================

    void UI::drawUI() {
        int winW = 0, winH = 0;
        glfwGetWindowSize(VSTIR::Editor::Get()->Window(), &winW, &winH);
        const float totalW    = winW > 0 ? (float)winW : (float)_context.Swapchain().extent.width;
        const float totalH    = winH > 0 ? (float)winH : (float)_context.Swapchain().extent.height;
        const float viewportW = (float)_viewport_width;
        const float panelW    = totalW - viewportW;

        beginDraw(viewportW, 0.0f, panelW, totalH);

        constexpr ImGuiWindowFlags kPanelFlags =
            ImGuiWindowFlags_NoMove     |
            ImGuiWindowFlags_NoResize   |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoTitleBar;

        ImGui::Begin("##vstir_panel", nullptr, kPanelFlags);

        // Branding header
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.22f, 0.22f, 1.0f));
        ImGui::SetWindowFontScale(1.18f);
        ImGui::Text("VSTIR");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.33f, 0.33f, 1.0f));
        ImGui::Text("Path Tracer");
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();

        sectionControls();
        sectionScene();
        sectionPerformance();
        sectionCamera();
        sectionRenderSettings();

        sectionPostProcessing();
        sectionObjects();

        ImGui::End();
        endDraw();
    }

    // ============================================================
    // Shared layout helpers (file-local)
    // ============================================================

    static void SubHeading(const char* label) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.35f, 0.35f, 1.0f));
        ImGui::Text("%s", label);
        ImGui::PopStyleColor();
        ImGui::Separator();
    }

    static void LabelValue(const char* label, const char* fmt, ...) {
        ImGui::Text("%s", label);
        ImGui::SameLine(110.0f);
        char buf[128];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        ImGui::TextColored(ImVec4(0.82f, 0.48f, 0.48f, 1.0f), "%s", buf);
    }

    static void LabelSliderFloat(const char* id, const char* label,
                                  float* v, float mn, float mx,
                                  const char* fmt = "%.2f") {
        ImGui::Text("%s", label);
        ImGui::SameLine(110.0f);
        ImGui::PushItemWidth(-1.0f);
        ImGui::SliderFloat(id, v, mn, mx, fmt);
        ImGui::PopItemWidth();
    }

    static void LabelSliderInt(const char* id, const char* label, int* v, int mn, int mx) {
        ImGui::Text("%s", label);
        ImGui::SameLine(110.0f);
        ImGui::PushItemWidth(-1.0f);
        ImGui::SliderInt(id, v, mn, mx);
        ImGui::PopItemWidth();
    }

    static bool SegmentedToggle(const char* labelTrue, const char* labelFalse, bool* value) {
        ImGuiStyle& style = ImGui::GetStyle();
        bool changed = false;

        const ImVec4 activeBtn     = style.Colors[ImGuiCol_ButtonActive];
        const ImVec4 activeHovered = style.Colors[ImGuiCol_ButtonActive];  // keep same on hover — it's already selected
        const ImVec4 activeText    = style.Colors[ImGuiCol_Text];
        const ImVec4 inactiveBtn   = style.Colors[ImGuiCol_Button];
        const ImVec4 inactiveHov   = style.Colors[ImGuiCol_ButtonHovered];
        const ImVec4 inactiveText  = style.Colors[ImGuiCol_TextDisabled];

        // Both buttons share the same fixed width so the control never resizes
        const float  btnWidth = std::max(
            ImGui::CalcTextSize(labelTrue).x,
            ImGui::CalcTextSize(labelFalse).x) + style.FramePadding.x * 2.0f;
        const ImVec2 btnSize(btnWidth, 0.0f);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

        // Left segment — true state
        ImGui::PushStyleColor(ImGuiCol_Text,          *value ? activeText   : inactiveText);
        ImGui::PushStyleColor(ImGuiCol_Button,         *value ? activeBtn   : inactiveBtn);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  *value ? activeHovered : inactiveHov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,   activeBtn);
        if (ImGui::Button(labelTrue, btnSize) && !*value) { *value = true;  changed = true; }
        ImGui::PopStyleColor(4);

        ImGui::SameLine();

        // Right segment — false state
        ImGui::PushStyleColor(ImGuiCol_Text,          !*value ? activeText   : inactiveText);
        ImGui::PushStyleColor(ImGuiCol_Button,        !*value ? activeBtn    : inactiveBtn);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, !*value ? activeHovered : inactiveHov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,   activeBtn);
        if (ImGui::Button(labelFalse, btnSize) && *value) { *value = false; changed = true; }
        ImGui::PopStyleColor(4);

        ImGui::PopStyleVar(2);
        return changed;
    }

    void UI::sectionControls() {
        if (!ImGui::CollapsingHeader("  Controls"))
            return;

        ImGui::Text("  Camera mode:");
        ImGui::SameLine(110.f);
        ImGui::TextColored(ImVec4(0.82f, 0.48f, 0.48f, 1.0f), "%s", _camera.IsOrbiting() ? "Orbit" : "Free");
        if (_camera.IsOrbiting()) {
            ImGui::Text("  [Mouse + Drag] Rotate camera around the target point");
            ImGui::Text("  [Mouse Wheel] Zoom in/out");
        } else {
            ImGui::Text("  [Mouse + Drag] Look around");
            ImGui::Text("  [WASD] Move Forward/Left/Back/Right");
            ImGui::Text("  [Space] Go Up");
            ImGui::Text("  [Shift] Go Down");
        }
        ImGui::Text("  Change camera settings under \"Camera\" section below");


    }

    // ============================================================
    // sectionScene
    // ============================================================

    void UI::sectionScene() {
        if (!ImGui::CollapsingHeader("  Scene"))
            return;

        openFileButton();

        const VSTIR::Geometry& geo = VSTIR::Editor::Get()->GetRenderer().GetGeometry();
        ImGui::Spacing();

        if (geo.triangles_size == 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.28f, 0.28f, 1.0f));
            ImGui::TextWrapped("No scene loaded. Open an OBJ file to begin.");
            ImGui::PopStyleColor();
        } else {
            SubHeading("Stats");
            LabelValue("  Triangles :", "%zu", geo.triangles_size);
            LabelValue("  Vertices  :", "%zu", geo.vertices_size);
            LabelValue("  Normals   :", "%zu", geo.normals_size);
            LabelValue("  Materials :", "%zu", geo.materials_size);
            LabelValue("  BVH Nodes :", "%zu", geo.bvh_size);
        }

        ImGui::Spacing();
    }

    // ============================================================
    // sectionPerformance
    // ============================================================

    void UI::sectionPerformance() {
        if (!ImGui::CollapsingHeader("  Performance", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        const ImGuiIO& io   = ImGui::GetIO();
        const float fps     = io.Framerate;
        const float frameMs = fps > 0.0f ? (1000.0f / fps) : 0.0f;

        s_FrameTimesMs[s_FrameTimeCursor] = frameMs;
        s_FrameTimeCursor = (s_FrameTimeCursor + 1) % (int)s_FrameTimesMs.size();
        if (s_FrameSamples < (int)s_FrameTimesMs.size()) s_FrameSamples++;

        float avgMs = 0.0f, maxMs = 0.0f;
        for (int i = 0; i < s_FrameSamples; i++) {
            avgMs += s_FrameTimesMs[i];
            maxMs  = std::max(maxMs, s_FrameTimesMs[i]);
        }
        avgMs = s_FrameSamples > 0 ? (avgMs / (float)s_FrameSamples) : 0.0f;

        // Color-coded FPS
        const ImVec4 fpsColor =
            fps >= 60.0f ? ImVec4(0.35f, 0.85f, 0.45f, 1.0f) :
            fps >= 30.0f ? ImVec4(0.90f, 0.80f, 0.20f, 1.0f) :
                           ImVec4(0.90f, 0.25f, 0.25f, 1.0f);

        ImGui::Text("  FPS        :");
        ImGui::SameLine(110.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, fpsColor);
        ImGui::Text("%.1f", fps);
        ImGui::PopStyleColor();

        // ImGui::SameLine();
        LabelValue("  Frame Time :", "%.2f ms", frameMs);
        // LabelValue("  Avg (180f) :", "%.2f ms", avgMs);


        ImGui::Spacing();

        // Frame-time graph
        const float graphMinMs = 0.0f;
        const float graphMaxMs = std::max(33.0f, maxMs * 1.2f);
        const float budgetMs   = 1000.0f / 60.0f;

        constexpr float kPlotHeight = 80.0f;
        constexpr float kAxisLabelGutterWidth = 56.0f;
        constexpr float kAxisLabelPad = 4.0f;

        const ImVec2 labelMin = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(kAxisLabelGutterWidth, kPlotHeight));
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::PlotLines(
            "##frametimes",
            s_FrameTimesMs.data(), s_FrameSamples, s_FrameTimeCursor,
            nullptr, graphMinMs, graphMaxMs, ImVec2(-1.0f, kPlotHeight));

        // 60-fps budget reference line
        ImDrawList*  dl      = ImGui::GetWindowDrawList();
        const ImVec2 plotMin = ImGui::GetItemRectMin();
        const ImVec2 plotMax = ImGui::GetItemRectMax();
        const float  t       = (budgetMs - graphMinMs) / std::max(0.001f, graphMaxMs - graphMinMs);
        const float  budgetY = plotMax.y - t * (plotMax.y - plotMin.y);
        dl->AddLine(ImVec2(plotMin.x, budgetY), ImVec2(plotMax.x, budgetY),
                    IM_COL32(255, 255, 255, 100), 1.5f);
        char budgetLabel[64];
        snprintf(budgetLabel, sizeof(budgetLabel), "60fps (%.1fms)", budgetMs);
        dl->AddText(ImVec2(plotMin.x + 4.0f, budgetY - 14.0f),
                    IM_COL32(255, 110, 110, 255), budgetLabel);

        char topLabel[32];
        char botLabel[32];
        snprintf(topLabel, sizeof(topLabel), "%.1f ms", graphMaxMs);
        snprintf(botLabel, sizeof(botLabel), "%.1f ms", graphMinMs);
        const float topLabelWidth = ImGui::CalcTextSize(topLabel).x;
        const float botLabelWidth = ImGui::CalcTextSize(botLabel).x;
        dl->AddText(
            ImVec2(labelMin.x + kAxisLabelGutterWidth - topLabelWidth - kAxisLabelPad, plotMin.y + 2.0f),
            IM_COL32(210, 150, 150, 255),
            topLabel);
        dl->AddText(
            ImVec2(labelMin.x + kAxisLabelGutterWidth - botLabelWidth - kAxisLabelPad, plotMax.y - 16.0f),
            IM_COL32(210, 150, 150, 255),
            botLabel);

        const char* dirLabel = "oldest -> newest";
        const float dirWidth = ImGui::CalcTextSize(dirLabel).x;
        dl->AddText(
            ImVec2(plotMax.x - dirWidth - 6.0f, plotMax.y - 16.0f),
            IM_COL32(170, 120, 120, 255),
            dirLabel);

        ImGui::Spacing();
    }

    // ============================================================
    // sectionCamera
    // ============================================================

    void UI::sectionCamera() {
        if (!ImGui::CollapsingHeader("  Camera", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        constexpr int bar_start = 150;

        Camera& cam = _camera;

        // SubHeading("Live State");

        ImGui::Text("  Mode      :");
        ImGui::SameLine(bar_start);
        SegmentedToggle("Orbit", "Free Look", &cam.IsOrbiting());

        ImGui::Spacing();

        if (cam.IsOrbiting()) {
            ImGui::Text("  Target Pos:");
            ImGui::SameLine(bar_start);
            ImGui::DragFloat3("##Target Pos:", &cam.OrbitTarget()[0]);
        } else {
            ImGui::Text("  Position:");
            ImGui::SameLine(bar_start);
            ImGui::DragFloat3("##Pos:", &cam.Position()[0]);

        }

        ImGui::Text("  Look Sensitivity:");
        ImGui::SameLine(bar_start);
        ImGui::SliderFloat("##look sens", &cam.LookSensitivity(), 0.1f, 5.0f);

        if (cam.IsOrbiting()) {
            ImGui::Text("  Zoom sensitivity:");
            ImGui::SameLine(bar_start);
            ImGui::SliderFloat("##zoom sens", &cam.ZoomSensitivity(), 0.1f, 5.0f);
        } else {
            ImGui::Text("  Movement Speed:");
            ImGui::SameLine(bar_start);
            ImGui::SliderFloat("##move speed", &cam.MovementSpeed(), 0.1f, 20.0f);
        }

        ImGui::Text("  FOV: ");
        ImGui::SameLine(150.0f);
        ImGui::SliderFloat("##fov", &cam.Fov(), 30.0f, 140.0f, "%.1f deg");

        ImGui::Spacing();
        if (ImGui::Button("Reset Camera", ImVec2(-1.0f, 0.0f))) {
            cam.Reset();
        }
        ImGui::Spacing();
    }

    // ============================================================
    // sectionRenderSettings
    // ============================================================
    void UI::sectionRenderSettings() {
        if (!ImGui::CollapsingHeader("  Render Settings", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        // static const char* kRenderModes[] = {
        //     "Full Path Trace", "Direct Light Only", "Albedo", "Normals", "Depth"
        // };

        auto& render_settings = _renderer.GetSettings();

        // ImGui::Text("  Mode       :");
        // ImGui::SameLine(110.0f);
        // ImGui::PushItemWidth(-1.0f);
        // ImGui::Combo("##rendermode", &s_RenderMode, kRenderModes, 5);
        // ImGui::PopItemWidth();



        ImGui::Text("  Render Resolution:");
        ImGui::SameLine();
        ImGui::Text("%d x %d", _render_width, _render_height);
        ImGui::Spacing();
        ImGui::Text("  Resolution Scale:");
        ImGui::SameLine();

        ImGui::SliderFloat("##resscale", &render_settings.resolution_scale, 0.1f, 2.0f, "%.2fx");

        ImGui::Spacing();

        // LabelSliderInt("##spp",     "  Samp/Frame:", &s_SamplesPerFrame, 1, 16);
        // LabelSliderInt("##bounces", "  Max Bounces:", &s_MaxBounces,      1, 32);

        // ImGui::Spacing();

        ImGui::Text("  Accumulate Samples:");
        ImGui::SameLine();
        ImGui::Checkbox("##accum", &render_settings.accumulate_samples);

        if (render_settings.accumulate_samples) {
            ImGui::SameLine();
            ImGui::Text("  Frames: %d", render_settings.sample_count);
            ImGui::SameLine();
            if (ImGui::Button("Reset", ImVec2(-1.0f, 0.0f))) {
                render_settings.sample_count = 0;
            }
        }



        ImGui::Spacing();
    }

    // ============================================================
    // sectionPostProcessing
    // ============================================================

    void UI::sectionPostProcessing() {
        if (!ImGui::CollapsingHeader("  Post-Processing"))
            return;

        // static const char* kToneMaps[] = { "None", "ACES", "Reinhard", "Filmic" };
        //
        // LabelSliderFloat("##exposure", "  Exposure   :", &s_Exposure, 0.1f, 10.0f, "%.2f");
        // LabelSliderFloat("##gamma",    "  Gamma      :", &s_Gamma,    1.0f,  3.0f, "%.2f");
        //
        // ImGui::Text("  Tone Map   :");
        // ImGui::SameLine(110.0f);
        // ImGui::PushItemWidth(-1.0f);
        // ImGui::Combo("##tonemap", &s_ToneMap, kToneMaps, 4);
        // ImGui::PopItemWidth();
        //
        // ImGui::Spacing();
        // SubHeading("Effects");
        //
        // ImGui::Text("  Vignette   :");
        // ImGui::SameLine(110.0f);
        // ImGui::Checkbox("##vignette", &s_Vignette);
        // if (s_Vignette) {
        //     LabelSliderFloat("##vigstr", "  Strength   :", &s_VignetteStrength, 0.0f, 1.0f);
        // }
        //
        // ImGui::Text("  Bloom      :");
        // ImGui::SameLine(110.0f);
        // ImGui::Checkbox("##bloom", &s_Bloom);
        // if (s_Bloom) {
        //     LabelSliderFloat("##bloomthr", "  Threshold  :", &s_BloomThreshold, 0.0f, 5.0f, "%.2f");
        //     LabelSliderFloat("##bloomstr", "  Strength   :", &s_BloomStrength,  0.0f, 1.0f, "%.3f");
        // }

        ImGui::Text("  Denoiser   :");
        ImGui::SameLine(110.0f);
        ImGui::Checkbox("##denoiser", &_render_settings.denoiser);
        if (_render_settings.denoiser) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.30f, 0.30f, 1.0f));
            ImGui::TextWrapped("Denoiser not yet implemented.");
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
    }

    // ============================================================
    // sectionObjects
    // ============================================================

    void UI::sectionObjects() {
        if (!ImGui::CollapsingHeader("  Materials"))
            return;

        const VSTIR::Geometry& geo  = VSTIR::Editor::Get()->GetRenderer().GetGeometry();
        const int              matN = (int)geo.materials_size;

        if (matN == 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.28f, 0.28f, 1.0f));
            ImGui::TextWrapped("No materials loaded.");
            ImGui::PopStyleColor();
            ImGui::Spacing();
            return;
        }

    }

    // ============================================================
    // Open-file button
    // ============================================================

    void UI::openFileButton() {
        if (ImGui::Button("Open OBJ...", ImVec2(-1.0f, 0.0f))) {
            const auto s = openFileExplorer({{"OBJ Files", "obj"}});
            if (s.empty()) return;
            INFO("Selected file: %s", s.c_str());
            Editor::LoadScene(s);
        }
    }

    // ============================================================
    // beginDraw / endDraw
    // ============================================================

    void UI::beginDraw(float x_pos, float y_pos, float width, float height) {
        if (!s_Initialized || s_ImageIndex >= s_Framebuffers.size()) return;

        VkImageMemoryBarrier toColor{};
        toColor.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toColor.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
        toColor.dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        toColor.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toColor.newLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        toColor.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toColor.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toColor.image               = _context.Swapchain().images[s_ImageIndex];
        toColor.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCmdPipelineBarrier(
            _scheduler.Commands().command,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0, 0, nullptr, 0, nullptr, 1, &toColor);

        VkRenderPassBeginInfo rpBegin{};
        rpBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBegin.renderPass        = s_RenderPass;
        rpBegin.framebuffer       = s_Framebuffers[s_ImageIndex];
        rpBegin.renderArea.offset = { 0, 0 };
        rpBegin.renderArea.extent = _context.Swapchain().extent;
        rpBegin.clearValueCount   = 0;
        rpBegin.pClearValues      = nullptr;
        vkCmdBeginRenderPass(_scheduler.Commands().command, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        // The swapchain is kept at logical (non-Retina) resolution, so tell
        // ImGui not to scale its draw commands by the 2x content scale that
        // ImGui_ImplGlfw_NewFrame() picks up from glfwGetFramebufferSize.
        ImGui::GetIO().DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
        ImGui::NewFrame();

        const ImGuiCond cond       = s_ForceLayout ? ImGuiCond_Always : ImGuiCond_Once;
        const float     nextWidth  = s_ForceLayout ? s_WindowWidth  : width;
        const float     nextHeight = s_ForceLayout ? s_WindowHeight : height;
        ImGui::SetNextWindowPos ({ x_pos, y_pos }, cond);
        ImGui::SetNextWindowSize({ nextWidth, nextHeight }, cond);
        s_ForceLayout = false;
    }

    void UI::endDraw() {
        if (!s_Initialized || s_ImageIndex >= s_Framebuffers.size()) return;

        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), _scheduler.Commands().command);
        vkCmdEndRenderPass(_scheduler.Commands().command);
    }

    // ============================================================
    // File explorer
    // ============================================================

    std::string UI::openFileExplorer(std::vector<FileFilters> file_filters) {
        NFD_Init();

        nfdu8filteritem_t filters[file_filters.size()];
        for (int i = 0; i < (int)file_filters.size(); i++) {
            filters[i].name = file_filters[i].name.c_str();
            filters[i].spec = file_filters[i].spec.c_str();
        }

        nfdu8char_t*          outPath = nullptr;
        nfdopendialogu8args_t args    = {};
        args.defaultPath  = PROJECT_ROOT;
        args.filterList   = filters;
        args.filterCount  = (nfdfiltersize_t)file_filters.size();
        nfdresult_t result = NFD_OpenDialogU8_With(&outPath, &args);

        std::string out_string;
        if (result == NFD_OKAY) {
            out_string = std::string(outPath);
            NFD_FreePathU8(outPath);
        } else if (result == NFD_CANCEL) {
            // user cancelled
        } else {
            printf("Error: %s\n", NFD_GetError());
        }

        NFD_Quit();
        return out_string;
    }
}
