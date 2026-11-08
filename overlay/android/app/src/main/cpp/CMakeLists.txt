cmake_minimum_required(VERSION 3.22.1)
project(libertyvr C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Repository root, from android/app/src/main/cpp
get_filename_component(RE3_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
set(REVC_ROOT "${RE3_ROOT}")
set(LIBRW_DIR "${RE3_ROOT}/vendor/librw")

# Optional OpenXR/Vulkan-only target for toolchain diagnostics.
option(LIBERTYVR_BRINGUP "Build only the OpenXR/Vulkan bring-up target" OFF)
option(MIAMIVR_BRINGUP "Alias for LIBERTYVR_BRINGUP" OFF)
if(MIAMIVR_BRINGUP)
	set(LIBERTYVR_BRINGUP ON)
endif()

# Explicit opt-in; neither Gradle debug nor RelWithDebInfo enables tools.
# Keep C++ and every shader on the same definition because the debug scene
# uniform and draw tint do not exist in player builds.
option(LIBERTYVR_DEV_TOOLS "Include developer-only diagnostics and test tools" OFF)
option(LIBERTYVR_VEHICLE_CAPTURE "Include opt-in vehicle capture/replay diagnostics" OFF)
option(MIAMIVR_DEV_TOOLS "Alias for LIBERTYVR_DEV_TOOLS" OFF)
if(MIAMIVR_DEV_TOOLS)
	set(LIBERTYVR_DEV_TOOLS ON)
endif()

# Optional AddressSanitizer instrumentation for native memory diagnostics.
option(LIBERTYVR_ASAN "Build with AddressSanitizer" OFF)
option(MIAMIVR_ASAN "Alias for LIBERTYVR_ASAN" OFF)
if(LIBERTYVR_ASAN OR MIAMIVR_ASAN)
	add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
	add_link_options(-fsanitize=address)
	message(STATUS "libertyvr: AddressSanitizer enabled")
endif()

# The AAR is published as org.khronos.openxr:openxr_loader_for_android, but the
# prefab package it exposes is named OpenXR, with targets OpenXR::openxr_loader
# and OpenXR::headers.
find_package(OpenXR REQUIRED CONFIG)

# ---------------------------------------------------------------------------
# Shaders: GLSL -> SPIR-V, embedded as C arrays.
# ---------------------------------------------------------------------------
set(LIBERTYVR_OVERLAY_DIR "${RE3_ROOT}")
include("${RE3_ROOT}/cmake/LibertyVulkan.cmake")

# ---------------------------------------------------------------------------
# native_app_glue
# ---------------------------------------------------------------------------
add_library(native_app_glue STATIC
	"${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c")
target_include_directories(native_app_glue PUBLIC
	"${ANDROID_NDK}/sources/android/native_app_glue")
set_target_properties(native_app_glue PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter")

# ---------------------------------------------------------------------------
# Main shared library loaded by NativeActivity (android.app.lib_name=libertyvr)
# ---------------------------------------------------------------------------
set(LIBERTYVR_SOURCES
	android_main.cpp
	xr_vulkan_session.cpp
	platform_android.cpp)

if(NOT LIBERTYVR_BRINGUP)
	message(STATUS "libertyvr: full game build selected")

	set(LIBERTYVR_APP_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
	include("${RE3_ROOT}/cmake/LibertyGameSources.cmake")
endif()

if(LIBERTYVR_BRINGUP)
    set(QUEST_ENGINE_TARGETS libertyvr)
else()
    set(QUEST_ENGINE_TARGETS libertyvr_classic libertyvr_upstate)
endif()
foreach(QUEST_ENGINE ${QUEST_ENGINE_TARGETS})
add_library(${QUEST_ENGINE} SHARED ${LIBERTYVR_SOURCES})
add_dependencies(${QUEST_ENGINE} libertyvr_shaders)

target_include_directories(${QUEST_ENGINE} PRIVATE
    "${RE3_ROOT}/src/xr"
	"${CMAKE_CURRENT_SOURCE_DIR}"
	"${SPIRV_OUT_DIR}"
	"${LIBRW_DIR}"
	${GAME_INCLUDES})

target_compile_definitions(${QUEST_ENGINE} PRIVATE
	LIBERTYVR_VEHICLE_CAPTURE=$<BOOL:${LIBERTYVR_VEHICLE_CAPTURE}>
	LIBERTYVR_DEV_TOOLS=$<BOOL:${LIBERTYVR_DEV_TOOLS}>
	XR_USE_PLATFORM_ANDROID
	XR_USE_GRAPHICS_API_VULKAN
	VK_USE_PLATFORM_ANDROID_KHR)

if(NOT LIBERTYVR_BRINGUP)
	target_compile_definitions(${QUEST_ENGINE} PRIVATE
		LIBRW RW_VULKAN AUDIO_ANDROID GTA_VR_WEAPONS)
	# 2003-era C++ read by a 2024 clang. These are style diagnostics, not
	# defects, and at this volume they bury the errors that matter.
	target_compile_options(${QUEST_ENGINE} PRIVATE
		-Wno-invalid-offsetof
		-Wno-writable-strings
		-Wno-unused-value
		-Wno-parentheses
		-Wno-switch
		-Wno-format-security)
endif()

if(LIBERTYVR_BRINGUP)
	target_compile_definitions(${QUEST_ENGINE} PRIVATE LIBERTYVR_BRINGUP=1 MIAMIVR_BRINGUP=1)
endif()

target_link_libraries(${QUEST_ENGINE}
	native_app_glue
	librw
	OpenXR::openxr_loader
	vulkan
	android
	aaudio
	dl
	log)

# NativeActivity resolves ANativeActivity_onCreate from the glue; without this
# the linker drops it as unreferenced and the app dies at startup.
target_link_options(${QUEST_ENGINE} PRIVATE "-u" "ANativeActivity_onCreate")
if(QUEST_ENGINE STREQUAL "libertyvr_upstate")
    target_compile_definitions(${QUEST_ENGINE} PRIVATE LCVR_UPSTATE_ENGINE=1)
endif()
endforeach()

if(NOT LIBERTYVR_BRINGUP)
    add_library(libertyvr SHARED quest_engine_selector.cpp)
    target_include_directories(libertyvr PRIVATE "${RE3_ROOT}/src")
    target_compile_definitions(libertyvr PRIVATE LCVR_UPSTATE_ADAPTER_READY=1)
    target_link_libraries(libertyvr android dl log)
    add_dependencies(libertyvr libertyvr_classic libertyvr_upstate)
endif()
