#pragma once

#include <string>

#include "vulkan/vstructs.h"

namespace VSTIR::SceneLoader {

    // Loads either a .yaml/.yml scene file or a direct mesh file
    // (.obj, .gltf, .glb) into geometry.
    // Returns true on success and false if parsing/IO/validation fails.
    bool LoadScene(const std::string& filepath, Geometry& geometry);

}

