#pragma once

#include "util/safety.h"
#include "vulkan/vcore.h"

namespace VSTIR {

    class Editor {
    public:
        Editor(size_t width, size_t height);
        ~Editor();
    public:
        void Run();
    public:
        GLFWwindow* Window() { return m_Window; }
        size_t Width() { return m_Width; }
        size_t Height() { return m_Height; }
    private:
        size_t m_Width;
        size_t m_Height;
        GLFWwindow* m_Window;
        Scope<VCore> m_Vulkan;
    };
}
