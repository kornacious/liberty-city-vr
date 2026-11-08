# Shared Vulkan backend and SPIR-V build for Quest and Windows.
find_program(GLSLC glslc
	HINTS "$ENV{VULKAN_SDK}/Bin"
          "${ANDROID_NDK}/shader-tools/${ANDROID_HOST_TAG}"
	      "${ANDROID_NDK}/shader-tools/windows-x86_64"
	      "${ANDROID_NDK}/shader-tools/linux-x86_64"
	REQUIRED)

set(SPIRV_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/spirv")
file(MAKE_DIRECTORY "${SPIRV_OUT_DIR}")

# compile_shader(<source> <stage> <symbol> <outdir> <includedir>)
# Emits a C initialiser list, consumed as:  static const uint32_t x[] =
# #include "x.h"
# ;
function(compile_shader SRC STAGE SYMBOL OUTDIR INCDIR)
	set(OUT "${OUTDIR}/${SYMBOL}.h")
	# Track GLSL includes so uniform changes rebuild dependent shaders.
	add_custom_command(
		OUTPUT "${OUT}"
		COMMAND "${GLSLC}" -fshader-stage=${STAGE} --target-env=vulkan1.1 -O ${ARGN}
		        -DLIBERTYVR_DEV_TOOLS=$<BOOL:${LIBERTYVR_DEV_TOOLS}>
		        -I "${INCDIR}" -MD -MF "${OUT}.d" -mfmt=c -o "${OUT}" "${SRC}"
		DEPENDS "${SRC}"
		DEPFILE "${OUT}.d"
		COMMENT "glslc ${SYMBOL}"
		VERBATIM)
	set(SHADER_OUTPUTS ${SHADER_OUTPUTS} "${OUT}" PARENT_SCOPE)
endfunction()

# App bring-up shaders
set(SHADER_DIR "${LIBERTYVR_OVERLAY_DIR}/android/app/src/main/cpp/shaders")
set(SHADER_OUTPUTS "")
compile_shader("${SHADER_DIR}/bringup.vert" vertex bringup_vert "${SPIRV_OUT_DIR}" "${SHADER_DIR}")
set(MIAMIVR_SPIRV_HEADERS ${SHADER_OUTPUTS})
compile_shader("${SHADER_DIR}/bringup.frag" fragment bringup_frag "${SPIRV_OUT_DIR}" "${SHADER_DIR}")
list(APPEND MIAMIVR_SPIRV_HEADERS ${SHADER_OUTPUTS})

add_custom_target(libertyvr_shaders DEPENDS ${MIAMIVR_SPIRV_HEADERS})
add_custom_target(miamivr_shaders DEPENDS ${MIAMIVR_SPIRV_HEADERS})

# librw backend shaders. They live with the backend, and rw_common.glsl is
# pulled in by all of them, hence the include directory.
set(LIBRW_SHADER_DIR "${LIBRW_DIR}/src/vulkan/shaders")
set(LIBRW_SPIRV_DIR "${CMAKE_CURRENT_BINARY_DIR}/spirv-librw")
file(MAKE_DIRECTORY "${LIBRW_SPIRV_DIR}")

set(LIBRW_SPIRV_HEADERS "")
foreach(SHADER rw_world:vert:vertex rw_world:frag:fragment
               rw_im2d:vert:vertex  rw_im2d:frag:fragment
               rw_im3d:vert:vertex  rw_skin:vert:vertex
               rw_postfx:vert:vertex rw_postfx:frag:fragment
               rw_motion:frag:fragment rw_sgsr2_convert:frag:fragment)
	string(REPLACE ":" ";" PARTS "${SHADER}")
	list(GET PARTS 0 SHADER_NAME)
	list(GET PARTS 1 SHADER_EXT)
	list(GET PARTS 2 SHADER_STAGE)
	set(SHADER_OUTPUTS "")
	compile_shader("${LIBRW_SHADER_DIR}/${SHADER_NAME}.${SHADER_EXT}"
	               ${SHADER_STAGE} "${SHADER_NAME}_${SHADER_EXT}"
	               "${LIBRW_SPIRV_DIR}" "${LIBRW_SHADER_DIR}")
	list(APPEND LIBRW_SPIRV_HEADERS ${SHADER_OUTPUTS})
endforeach()

foreach(NEO_ENV 0 1)
    set(SHADER_OUTPUTS "")
    compile_shader("${LIBRW_SHADER_DIR}/rw_world.frag" fragment "rw_world_neo${NEO_ENV}_frag"
        "${LIBRW_SPIRV_DIR}" "${LIBRW_SHADER_DIR}" -DRW_NEO_RESOURCES=1 -DRW_WORLD_EFFECTS_RESOURCES=${NEO_ENV})
    list(APPEND LIBRW_SPIRV_HEADERS ${SHADER_OUTPUTS})
endforeach()

set(SHADER_OUTPUTS "")
compile_shader("${LIBRW_SHADER_DIR}/rw_world.vert" vertex
               "rw_world_instanced_vert" "${LIBRW_SPIRV_DIR}"
               "${LIBRW_SHADER_DIR}" -DRW_STATIC_INSTANCING=1)
list(APPEND LIBRW_SPIRV_HEADERS ${SHADER_OUTPUTS})

# Build the baseline world fragment interface without effects resources. A
# specialization-only variant still advertises set 3 to capture reflection.
set(SHADER_OUTPUTS "")
compile_shader("${LIBRW_SHADER_DIR}/rw_world.frag" fragment
               "rw_world_noeffects_frag" "${LIBRW_SPIRV_DIR}"
               "${LIBRW_SHADER_DIR}" -DRW_WORLD_EFFECTS_RESOURCES=0)
list(APPEND LIBRW_SPIRV_HEADERS ${SHADER_OUTPUTS})

set(SHADER_OUTPUTS "")
compile_shader("${LIBRW_SHADER_DIR}/rw_postfx.frag" fragment
               "rw_postfx_native_frag" "${LIBRW_SPIRV_DIR}"
               "${LIBRW_SHADER_DIR}" -DRW_POSTFX_NATIVE=1)
list(APPEND LIBRW_SPIRV_HEADERS ${SHADER_OUTPUTS})

set(SHADER_OUTPUTS "")
compile_shader("${LIBRW_SHADER_DIR}/rw_postfx.frag" fragment
               "rw_postfx_native_filtered_frag" "${LIBRW_SPIRV_DIR}"
               "${LIBRW_SHADER_DIR}" -DRW_POSTFX_NATIVE=1 -DRW_POSTFX_INTEGER_STENCIL=0)
list(APPEND LIBRW_SPIRV_HEADERS ${SHADER_OUTPUTS})

set(SHADER_OUTPUTS "")
compile_shader("${LIBRW_SHADER_DIR}/rw_im2d.frag" fragment
               "rw_im2d_nocoverage_frag" "${LIBRW_SPIRV_DIR}"
               "${LIBRW_SHADER_DIR}" -DRW_IM2D_COVERAGE=0)
list(APPEND LIBRW_SPIRV_HEADERS ${SHADER_OUTPUTS})
add_custom_target(librw_shaders DEPENDS ${LIBRW_SPIRV_HEADERS})

# ---------------------------------------------------------------------------
# librw with the Vulkan backend
#
# Every backend directory is compiled, not just src/vulkan. The other backends
# are disabled by their own #ifdefs and collapse to almost nothing, but librw's
# core calls into them unconditionally -- rw::gl3::initSkin, d3d8::initMatFX,
# ps2::readNativeTexture and so on -- so their stub definitions have to exist
# or the link fails.
# ---------------------------------------------------------------------------
file(GLOB LIBRW_CORE_SOURCES "${LIBRW_DIR}/src/*.cpp")
file(GLOB LIBRW_VULKAN_SOURCES CONFIGURE_DEPENDS "${LIBRW_DIR}/src/vulkan/*.cpp")
file(GLOB LIBRW_D3D_SOURCES "${LIBRW_DIR}/src/d3d/*.cpp")
file(GLOB LIBRW_D3D12_SOURCES "${LIBRW_DIR}/src/d3d12/*.cpp")
file(GLOB LIBRW_GL_SOURCES "${LIBRW_DIR}/src/gl/*.cpp")
file(GLOB LIBRW_PS2_SOURCES "${LIBRW_DIR}/src/ps2/*.cpp")
file(GLOB LIBRW_LODEPNG_SOURCES "${LIBRW_DIR}/src/lodepng/*.cpp")

add_library(librw STATIC
	${LIBRW_CORE_SOURCES} ${LIBRW_VULKAN_SOURCES}
	${LIBRW_D3D_SOURCES} ${LIBRW_D3D12_SOURCES}
	${LIBRW_GL_SOURCES} ${LIBRW_PS2_SOURCES} ${LIBRW_LODEPNG_SOURCES})
add_dependencies(librw librw_shaders)
target_include_directories(librw PUBLIC "${LIBRW_DIR}" "${LIBRW_DIR}/src")
target_include_directories(librw PRIVATE "${LIBRW_SPIRV_DIR}")
target_compile_definitions(librw PUBLIC RW_VULKAN LIBERTYVR_DEV_TOOLS=$<BOOL:${LIBERTYVR_DEV_TOOLS}>)
if(ANDROID)
    target_link_libraries(librw vulkan log)
else()
    target_link_libraries(librw Vulkan::Vulkan)
endif()
if(MSVC)
    target_compile_definitions(librw PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
target_compile_options(librw PRIVATE
	-Wno-unused-parameter
	-Wno-unused-variable
	-Wno-unused-function
	# librw casts between plugin storage and concrete types throughout; the
	# warning fires thousands of times and hides real diagnostics.
	-Wno-invalid-offsetof)

endif()
