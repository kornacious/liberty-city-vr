# Shared re3/VR source selection; platform services are selected by the caller.
	set(GAME_DIRS
		animation audio buildings collision control core entities extras
		fakerw math modelinfo objects peds renderer rw save skel skel/android
		text vehicles weapons)

	set(GAME_SOURCES "")
	foreach(d ${GAME_DIRS})
		file(GLOB _dir_sources "${RE3_ROOT}/src/${d}/*.cpp")
		list(APPEND GAME_SOURCES ${_dir_sources})
	endforeach()
	set(QUEST_TRAVERSAL_SOURCE "${RE3_ROOT}/src/peds/VrPlayerTraversal.cpp")
	list(REMOVE_ITEM GAME_SOURCES "${QUEST_TRAVERSAL_SOURCE}")
	list(APPEND GAME_SOURCES "${QUEST_TRAVERSAL_SOURCE}")
	# Keep this explicit because file(GLOB) does not cause an already configured
	# Ninja graph to notice a newly added source file.
	set(QUEST_PROFILER_SOURCE
		"${RE3_ROOT}/src/skel/android/QuestProfiler.cpp")
	list(REMOVE_ITEM GAME_SOURCES "${QUEST_PROFILER_SOURCE}")
	list(APPEND GAME_SOURCES "${QUEST_PROFILER_SOURCE}")
    set(WORLD_SESSION_SOURCE "${RE3_ROOT}/src/core/WorldSession.cpp")
    list(REMOVE_ITEM GAME_SOURCES "${WORLD_SESSION_SOURCE}")
    list(APPEND GAME_SOURCES "${WORLD_SESSION_SOURCE}")
	set(QUEST_RENDERDOC_SOURCE "${RE3_ROOT}/src/skel/android/QuestRenderDoc.cpp")
	list(REMOVE_ITEM GAME_SOURCES "${QUEST_RENDERDOC_SOURCE}")
	list(APPEND GAME_SOURCES "${QUEST_RENDERDOC_SOURCE}")
	# Like QuestProfiler, keep the newly introduced source explicit so an
	# already-configured Ninja graph notices it immediately.
	set(PHYSICS_DIRECTOR_SOURCE
		"${RE3_ROOT}/src/vehicles/PhysicsDirector.cpp")
	list(REMOVE_ITEM GAME_SOURCES "${PHYSICS_DIRECTOR_SOURCE}")
	list(APPEND GAME_SOURCES "${PHYSICS_DIRECTOR_SOURCE}")
	set(VEHICLE_CAPTURE_SOURCE "${RE3_ROOT}/src/vehicles/VehicleCapture.cpp")
	list(REMOVE_ITEM GAME_SOURCES "${VEHICLE_CAPTURE_SOURCE}")
	if(LIBERTYVR_VEHICLE_CAPTURE)
		list(APPEND GAME_SOURCES "${VEHICLE_CAPTURE_SOURCE}")
	endif()
	set(VEHICLE_VISUAL_DIRECTOR_SOURCE
		"${RE3_ROOT}/src/vehicles/VehicleVisualDirector.cpp")
	list(REMOVE_ITEM GAME_SOURCES "${VEHICLE_VISUAL_DIRECTOR_SOURCE}")
	list(APPEND GAME_SOURCES "${VEHICLE_VISUAL_DIRECTOR_SOURCE}")
	set(VEHICLE_DEFORMATION_SOURCE
		"${RE3_ROOT}/src/vehicles/VehicleDeformation.cpp")
	list(REMOVE_ITEM GAME_SOURCES "${VEHICLE_DEFORMATION_SOURCE}")
	list(APPEND GAME_SOURCES "${VEHICLE_DEFORMATION_SOURCE}")
	# Exclude the tool translation unit entirely in player builds. The header
	# still provides constant-false/no-op entry points for defensive callers.
	set(QUEST_VISUALIZATION_SOURCE "${RE3_ROOT}/src/renderer/QuestVisualization.cpp")
    list(REMOVE_ITEM GAME_SOURCES "${QUEST_VISUALIZATION_SOURCE}")
    list(APPEND GAME_SOURCES "${QUEST_VISUALIZATION_SOURCE}")
    set(VR_CULL_VIZ_SOURCE "${RE3_ROOT}/src/renderer/VrCullViz.cpp")
	list(REMOVE_ITEM GAME_SOURCES "${VR_CULL_VIZ_SOURCE}")
	if(LIBERTYVR_DEV_TOOLS)
		list(APPEND GAME_SOURCES "${VR_CULL_VIZ_SOURCE}")
	endif()

	# Quest audio uses a native AAudio SFX mixer and the Android platform
	# decoder for long radio/cutscene streams.  The desktop OpenAL/Miles
	# backends carry host-only libraries and are deliberately not linked.
	list(REMOVE_ITEM GAME_SOURCES
		"${RE3_ROOT}/src/audio/sampman_oal.cpp"
		"${RE3_ROOT}/src/audio/sampman_miles.cpp"
		"${RE3_ROOT}/src/audio/sampman_null.cpp"
		"${RE3_ROOT}/src/extras/GitSHA1.cpp")

	# GitSHA1.cpp is generated, not committed; the desktop build makes it with
	# printHash.bat as a prebuild step.
	execute_process(
		COMMAND git rev-parse HEAD
		WORKING_DIRECTORY "${RE3_ROOT}"
		OUTPUT_VARIABLE GIT_SHA1
		OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_QUIET)
	if(NOT GIT_SHA1)
		set(GIT_SHA1 "unknown")
	endif()
	configure_file("${RE3_ROOT}/src/extras/GitSHA1.cpp.in"
	               "${CMAKE_CURRENT_BINARY_DIR}/GitSHA1.cpp" @ONLY)
	list(APPEND GAME_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/GitSHA1.cpp")
	# Native Quest hands share the authored UltimateXR mesh loader with the
	# desktop VR build. QuestWeaponVR is the Android input/interaction adapter;
	# it deliberately does not own another OpenXR session or stereo renderer.
	list(APPEND GAME_SOURCES
		"${RE3_ROOT}/src/vr/VRHandModel.cpp"
		"${RE3_ROOT}/src/vr/QuestDrivingVR.cpp"
		"${RE3_ROOT}/src/vr/QuestRadio.cpp"
		"${RE3_ROOT}/src/vr/QuestSpeedometer.cpp"
		"${RE3_ROOT}/src/vr/QuestWeaponVR.cpp")
	# Player full-body presentation IK is intentionally isolated from the
	# weapon module. Keep it explicit so incremental Ninja graphs notice the
	# newly introduced translation unit after CMake reruns.
	set(VR_BODY_IK_SOURCE "${RE3_ROOT}/src/peds/VrBodyIK.cpp")
	list(REMOVE_ITEM GAME_SOURCES "${VR_BODY_IK_SOURCE}")
	list(APPEND GAME_SOURCES "${VR_BODY_IK_SOURCE}")

	list(APPEND LIBERTYVR_SOURCES ${GAME_SOURCES})

	foreach(d ${GAME_DIRS} "")
		list(APPEND GAME_INCLUDES "${RE3_ROOT}/src/${d}")
	endforeach()
	list(APPEND GAME_INCLUDES "${RE3_ROOT}/src" "${RE3_ROOT}/src/audio/oal"
	                         "${RE3_ROOT}/src/vr")
	# The skeleton reaches into the app layer for the storage roots.
	list(APPEND GAME_INCLUDES "${LIBERTYVR_APP_DIR}")
