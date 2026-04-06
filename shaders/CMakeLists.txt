find_program(GLSLC glslc HINTS
    $ENV{VULKAN_SDK}/bin
    /usr/bin
    /usr/local/bin
    /opt/homebrew/bin
)

if(NOT GLSLC)
    message(FATAL_ERROR "glslc not found — install the Vulkan SDK")
endif()

file(GLOB SHADER_SOURCES "*.vert" "*.frag" "*.comp")

set(SPIRV_OUTPUTS)
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(FNAME ${SHADER} NAME)
    set(SPIRV "${CMAKE_BINARY_DIR}/bin/shaders/${FNAME}.spv")

    add_custom_command(
        OUTPUT  ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/shaders"
        COMMAND ${GLSLC} ${SHADER} -o ${SPIRV}
        DEPENDS ${SHADER}
        VERBATIM
    )
    list(APPEND SPIRV_OUTPUTS ${SPIRV})
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SPIRV_OUTPUTS})
