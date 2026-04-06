#include "vcore.h"

namespace VSTIR {

    VCore::VCore() {

    }

    VCore::~VCore() {

    }

    void VCore::Initialize() {
        // TODO: m_Shaders.Initialize();
        m_General.Initialize();
        // TODO: m_Geometry.Initialize();
        m_Scheduler.Initialize();
    }

}
