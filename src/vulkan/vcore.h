#pragma once

#include "vulkan/vgeneral.h"
#include "vulkan/vscheduler.h"

namespace VSTIR {

    class VCore {
    public:
        VCore();
        ~VCore();
    public:
        void Initialize();
    private:
        VGeneral m_General;
        VScheduler m_Scheduler;
    };

}
