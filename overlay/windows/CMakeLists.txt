cmake_minimum_required(VERSION 3.22.1)
project(LibertyVRWindows VERSION 0.8.5 LANGUAGES C CXX)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "This bring-up target requires Windows x64.")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(LIBERTYVR_DEV_TOOLS "Compile renderer diagnostics" OFF)
option(LIBERTYVR_GPU_TESTS "Run offscreen tests requiring a working Vulkan GPU" OFF)
option(LIBERTYVR_AUDIO_TESTS "Run Windows platform tests requiring an audio output device" OFF)
set(LIBERTYVR_RE3_ROOT "" CACHE PATH "Manager-assembled patched re3 tree for the experimental game")
find_package(Vulkan REQUIRED)
find_package(OpenXR QUIET CONFIG)
if(NOT TARGET OpenXR::openxr_loader)
    # Source contains generated loader files; Python is not required.
    # FETCHCONTENT_SOURCE_DIR_OPENXR_SDK supports an existing offline checkout.
    include(FetchContent)
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_API_LAYERS OFF CACHE BOOL "" FORCE)
    set(BUILD_CONFORMANCE_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_LOADER ON CACHE BOOL "" FORCE)
    set(DYNAMIC_LOADER ON CACHE BOOL "" FORCE)
    FetchContent_Declare(openxr_sdk
        URL https://github.com/KhronosGroup/OpenXR-SDK/archive/refs/tags/release-1.1.58.zip
        URL_HASH SHA256=43d75ce00e31be8f662b58e8c74af8c04156bcaa7acbfc19112aa2880f3a3669)
    FetchContent_MakeAvailable(openxr_sdk)
endif()
get_filename_component(LIBERTYVR_OVERLAY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(LIBRW_DIR "${LIBERTYVR_OVERLAY_DIR}/../librw" CACHE PATH "Bundled librw source")
if(NOT EXISTS "${LIBRW_DIR}/rw.h")
    # Also support a manager-assembled source tree.
    set(LIBRW_DIR "${LIBERTYVR_OVERLAY_DIR}/vendor/librw")
endif()
include("${LIBERTYVR_OVERLAY_DIR}/cmake/LibertyVulkan.cmake")
if(LIBERTYVR_RE3_ROOT)
    set(RE3_ROOT "${LIBERTYVR_RE3_ROOT}")
    set(LIBERTYVR_APP_DIR "${LIBERTYVR_OVERLAY_DIR}/android/app/src/main/cpp")
    set(LIBERTYVR_SOURCES game_main.cpp platform_windows.cpp WaveDecode.cpp renderdoc_windows.cpp streaming_windows.cpp
        "${LIBERTYVR_APP_DIR}/android_main.cpp" "${LIBERTYVR_APP_DIR}/xr_vulkan_session.cpp")
    include("${LIBERTYVR_OVERLAY_DIR}/cmake/LibertyGameSources.cmake")
    add_executable(libertyvr-game ${LIBERTYVR_SOURCES})
    add_dependencies(libertyvr-game libertyvr_shaders)
    target_include_directories(libertyvr-game PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/compat" "${LIBERTYVR_OVERLAY_DIR}/src/xr"
        "${LIBERTYVR_OVERLAY_DIR}/src/core" "${LIBERTYVR_APP_DIR}" "${SPIRV_OUT_DIR}" ${GAME_INCLUDES})
    target_compile_definitions(libertyvr-game PRIVATE LIBRW RW_VULKAN GTA_VR_WEAPONS
        AUDIO_ANDROID LIBERTYVR_WINDOWS WITHWINDOWS XR_USE_GRAPHICS_API_VULKAN
        LIBERTYVR_DEV_TOOLS=$<BOOL:${LIBERTYVR_DEV_TOOLS}>
        LIBERTYVR_VEHICLE_CAPTURE=0 CMAKE_NO_AUTOLINK NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
    target_compile_options(libertyvr-game PRIVATE /FI"${CMAKE_CURRENT_SOURCE_DIR}/compat/RuntimeCompat.h" /bigobj /MP8 /Zi)
    target_link_options(libertyvr-game PRIVATE /DEBUG)
    target_link_libraries(libertyvr-game PRIVATE librw OpenXR::openxr_loader
        avrt winmm msacm32 mfplay mfplat mfreadwrite mfuuid ole32 propsys shlwapi shell32 dbghelp)
    if(TARGET openxr_loader)
        add_custom_command(TARGET libertyvr-game POST_BUILD COMMAND ${CMAKE_COMMAND} -E
            copy_if_different $<TARGET_FILE:openxr_loader> $<TARGET_FILE_DIR:libertyvr-game>)
    endif()
endif()
add_executable(libertyvr-pcvr main.cpp)
target_compile_definitions(libertyvr-pcvr PRIVATE
    XR_USE_GRAPHICS_API_VULKAN NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
target_include_directories(libertyvr-pcvr PRIVATE "${LIBERTYVR_OVERLAY_DIR}/src/xr")
target_link_libraries(libertyvr-pcvr PRIVATE librw OpenXR::openxr_loader)
if(TARGET openxr_loader)
    add_custom_command(TARGET libertyvr-pcvr POST_BUILD COMMAND ${CMAKE_COMMAND} -E
        copy_if_different $<TARGET_FILE:openxr_loader> $<TARGET_FILE_DIR:libertyvr-pcvr>)
endif()
if(openxr_sdk_SOURCE_DIR)
    set(_notices "$<TARGET_FILE_DIR:libertyvr-pcvr>/openxr-notices")
    add_custom_command(TARGET libertyvr-pcvr POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_notices}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${openxr_sdk_SOURCE_DIR}/LICENSE" "${openxr_sdk_SOURCE_DIR}/COPYING.adoc" "${_notices}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${openxr_sdk_SOURCE_DIR}/.reuse/dep5" "${_notices}/copyright-dep5"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${openxr_sdk_SOURCE_DIR}/LICENSES" "${_notices}/LICENSES"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${openxr_sdk_SOURCE_DIR}/src/external/jsoncpp/LICENSE" "${_notices}/JsonCpp-LICENSE")
endif()
include(CTest)
if(BUILD_TESTING)
    if(LIBERTYVR_AUDIO_TESTS)
        add_executable(pcvr-platform-tests platform_tests.cpp platform_windows.cpp WaveDecode.cpp)
        target_include_directories(pcvr-platform-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/compat"
            "${LIBERTYVR_OVERLAY_DIR}/src/core" "${LIBERTYVR_OVERLAY_DIR}/src/audio"
            "${LIBERTYVR_OVERLAY_DIR}/android/app/src/main/cpp")
        target_compile_definitions(pcvr-platform-tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
        target_link_libraries(pcvr-platform-tests PRIVATE avrt winmm msacm32 mfplay mfplat mfreadwrite mfuuid ole32 propsys shlwapi shell32 dbghelp)
        add_test(NAME pcvr-platform COMMAND pcvr-platform-tests)
        set_tests_properties(pcvr-platform PROPERTIES PASS_REGULAR_EXPRESSION "IMA cache/playback passed")
    endif()
    add_executable(pcvr-core-tests tests.cpp)
    target_include_directories(pcvr-core-tests PRIVATE "${LIBERTYVR_OVERLAY_DIR}/src/xr"
        "${LIBRW_DIR}/src/vulkan")
    target_link_libraries(pcvr-core-tests PRIVATE Vulkan::Vulkan OpenXR::headers)
    add_test(NAME pcvr-core COMMAND pcvr-core-tests)
    add_test(NAME pcvr-cli-help COMMAND libertyvr-pcvr --help)
    add_test(NAME pcvr-cli-invalid COMMAND libertyvr-pcvr --invalid)
    set_tests_properties(pcvr-cli-invalid PROPERTIES WILL_FAIL TRUE)
    if(LIBERTYVR_GPU_TESTS)
        add_test(NAME pcvr-renderer COMMAND libertyvr-pcvr --renderer-probe)
        add_test(NAME pcvr-colour COMMAND libertyvr-pcvr --colour-probe)
    endif()
endif()
