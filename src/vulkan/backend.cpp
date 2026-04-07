#include "backend.h"

namespace VSTIR {

    void Backend::Initialize() {
        m_Metadata.Initialize();
        m_Core.Initialize();
    }

}
