#include "vmetadata.h"
#include "vulkan/vulkan.h"

namespace VSTIR {

    VMetadata::VMetadata() {

    }

    VMetadata::~VMetadata() {

    }

    void VMetadata::Initialize() {
        m_Validation.push_back("VK_LAYER_KHRONOS_validation");
        m_Extensions.required.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

}
