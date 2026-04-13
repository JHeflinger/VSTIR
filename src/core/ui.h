#pragma once
#include "GLFW/glfw3.h"
#include <cstdint>


class UI {
public:
    static void initialize(GLFWwindow* window);
    static void recreateSwapchainResources();
    static void onResize(float width, float height);
    static void setImageIndex(uint32_t imageIndex);

    static void drawUI();
    static void beginDraw(float x_pos, float y_pos, float width, float height);
    static void endDraw();


};
