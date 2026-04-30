# Automatically collect all sources and headers under src/
file(GLOB_RECURSE APP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(vstir ${APP_SOURCES})

# Shaders must compile before the app links
add_dependencies(vstir Shaders)

target_include_directories(vstir PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(vstir PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
    yaml-cpp
    imgui
    GPUOpen::VulkanMemoryAllocator
    nfd
)

# Platform-specific flags
if(WIN32)
    target_compile_definitions(vstir PRIVATE VK_USE_PLATFORM_WIN32_KHR)
elseif(APPLE)
    target_compile_definitions(vstir PRIVATE VK_USE_PLATFORM_MACOS_MVK)
    target_link_libraries(vstir PRIVATE "-framework Metal" "-framework QuartzCore")
elseif(UNIX)
    target_compile_definitions(vstir PRIVATE VK_USE_PLATFORM_XCB_KHR)
endif()

target_compile_definitions(vstir PRIVATE
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    $<$<CONFIG:Debug>:VK_ENABLE_VALIDATION_LAYERS>
)
