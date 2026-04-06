#pragma once

#include "vulkan/vstructs.h"

namespace VSTIR {

    class VMetadata {
    public:
        VMetadata();
        ~VMetadata();
    public:
        void Initialize();
        std::vector<std::string>& Validation() { return m_Validation; }
        VExtensionData& Extensions() { return m_Extensions; }
    private:
        std::vector<std::string> m_Validation;
        VExtensionData m_Extensions;
    };

}
