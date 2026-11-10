cmake_minimum_required(VERSION 3.22)
project(depthprobe CXX)
set(CMAKE_CXX_STANDARD 17)
find_program(GLSLC glslc HINTS "${ANDROID_NDK}/shader-tools/windows-x86_64" "${ANDROID_NDK}/shader-tools/linux-x86_64" REQUIRED)
foreach(STAGE vert frag)
  add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/control_${STAGE}.h"
    COMMAND "${GLSLC}" --target-env=vulkan1.1 -O -mfmt=c -o "${CMAKE_CURRENT_BINARY_DIR}/control_${STAGE}.h" "${CMAKE_CURRENT_SOURCE_DIR}/control.${STAGE}"
    DEPENDS "control.${STAGE}" VERBATIM)
endforeach()
foreach(KILL 0 1)
  add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/transition_${KILL}.h"
    COMMAND "${GLSLC}" --target-env=vulkan1.1 -O -mfmt=c -DCONTROL_KILL=${KILL} -o "${CMAKE_CURRENT_BINARY_DIR}/transition_${KILL}.h" "${CMAKE_CURRENT_SOURCE_DIR}/transition.frag"
    DEPENDS transition.frag VERBATIM)
endforeach()
add_library(depthprobe SHARED probe.cpp "${CMAKE_CURRENT_BINARY_DIR}/control_vert.h" "${CMAKE_CURRENT_BINARY_DIR}/control_frag.h"
  "${CMAKE_CURRENT_BINARY_DIR}/transition_0.h" "${CMAKE_CURRENT_BINARY_DIR}/transition_1.h")
target_include_directories(depthprobe PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../../overlay/src/skel/android")
target_link_libraries(depthprobe vulkan log dl)
target_compile_options(depthprobe PRIVATE -Wall -Wextra -Werror)
