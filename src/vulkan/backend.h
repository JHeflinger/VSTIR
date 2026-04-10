#pragma once

#include "vulkan/vcore.h"
#include "vulkan/vmetadata.h"

namespace VSTIR {

    class Backend {
    public:
        Backend() {};
        ~Backend() {};
    public:
        void Initialize();
        void Reconstruct();
        VMetadata& Metadata() { return m_Metadata; }
        VCore& Core() { return m_Core; }
    private:
        VCore m_Core;
        VMetadata m_Metadata;
    };

}
