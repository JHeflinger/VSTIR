#include "backend.h"

namespace VSTIR {

    Backend::Backend() {

    }

    Backend::~Backend() {

    }

    void Backend::Initialize() {
        m_Metadata.Initialize();
        m_Core.Initialize();
    }

}
