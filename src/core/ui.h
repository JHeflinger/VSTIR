#pragma once
#include "GLFW/glfw3.h"
#include <cstdint>
#include <vector>

namespace VSTIR {
    struct FileFilters {
        FileFilters(const std::string& filter_name, const std::string& filter_spec) : name(filter_name), spec(filter_spec) {}
        std::string name;
        std::string spec;
    };

    class UI {
    public:
        static void initialize(GLFWwindow* window);
        static void recreateSwapchainResources();
        static void onResize(float width, float height);
        static void setImageIndex(uint32_t imageIndex);
        static void setTheme();

        static void drawUI();
        static void beginDraw(float x_pos, float y_pos, float width, float height);
        static void endDraw();
        static std::string openFileExplorer(std::vector<FileFilters> filters);
    private:
        static void sectionScene();
        static void sectionPerformance(int viewportWidthPx, int viewportHeightPx);
        static void sectionCamera();
        static void sectionRenderSettings();
        static void sectionEnvironment();
        static void sectionPostProcessing();
        static void sectionObjects();
        static void openFileButton();
    };
}