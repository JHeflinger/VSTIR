find_program(GLSLC glslc HINTS
    $ENV{VULKAN_SDK}/bin
    /usr/bin
    /usr/local/bin
    /opt/homebrew/bin
)

if(NOT GLSLC)
    message(FATAL_ERROR "glslc not found — install the Vulkan SDK")
endif()

file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS "*.vert" "*.frag" "*.comp")
file(GLOB SHADER_INCLUDES CONFIGURE_DEPENDS "*.glsl")

set(SPIRV_OUTPUTS)
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(FNAME ${SHADER} NAME)
    set(SPIRV "${CMAKE_BINARY_DIR}/bin/shaders/${FNAME}.spv")

    add_custom_command(
        OUTPUT  ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/shaders"
        COMMAND ${GLSLC} -I ${CMAKE_CURRENT_SOURCE_DIR} ${SHADER} -o ${SPIRV}
        DEPENDS ${SHADER} ${SHADER_INCLUDES}
        VERBATIM
    )
    list(APPEND SPIRV_OUTPUTS ${SPIRV})
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SPIRV_OUTPUTS})
