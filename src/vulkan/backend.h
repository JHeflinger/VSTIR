#pragma once

#include "vulkan/vcore.h"
#include "vulkan/vmetadata.h"

namespace VSTIR {

    class Backend {
    public:
        Backend();
        ~Backend();
    public:
        void Initialize();
        VMetadata& Metadata() { return m_Metadata; }
    private:
        VCore m_Core;
        VMetadata m_Metadata;
    };

}
