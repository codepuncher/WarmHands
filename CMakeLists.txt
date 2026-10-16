cmake_minimum_required(VERSION 3.21)

project(
    WarmHands
    VERSION 0.0.1
    DESCRIPTION "An SKSE plugin for Skyrim SE/AE"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PLUGIN_TESTS_ONLY "Build unit tests instead of the plugin" OFF)
option(PLUGIN_BUILD_ESL "Build WarmHands.esl with Spriggit" ON)

if(PLUGIN_TESTS_ONLY)

    find_package(Catch2 3 CONFIG REQUIRED)

    include(CTest)
    include(Catch)

    add_executable(
        ${PROJECT_NAME}Tests
        test/SettingsTests.cpp
        test/WarmthTests.cpp
        test/IdleDecisionTests.cpp
        test/KeyNamesTests.cpp
        test/HoldTrackerTests.cpp
        test/HeatGeometryTests.cpp)

    target_include_directories(${PROJECT_NAME}Tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

    target_link_libraries(${PROJECT_NAME}Tests PRIVATE Catch2::Catch2WithMain)

    catch_discover_tests(${PROJECT_NAME}Tests)

else()

    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lib/commonlibsse-ng/CMakeLists.txt")
        message(FATAL_ERROR "lib/commonlibsse-ng is not checked out. Clone with --recurse-submodules, "
                            "or run: git submodule update --init --recursive")
    endif()
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lib/skse-mcp/include/SKSEMCP/SKSEMenuFramework.hpp")
        message(FATAL_ERROR "lib/skse-mcp is not checked out. Clone with --recurse-submodules, "
                            "or run: git submodule update --init --recursive")
    endif()

    find_package(spdlog CONFIG REQUIRED)
    # directxtk must be found before add_commonlibsse_plugin (CommonLibSSE-NG links
    # Microsoft::DirectXTK transitively).
    find_package(directxtk CONFIG REQUIRED)
    # rapidcsv must be in vcpkg.json (CommonLibSSE-NG unconditionally adds it as an include dir)

    set(ENABLE_SKYRIM_SE
        ON
        CACHE BOOL "" FORCE)
    set(ENABLE_SKYRIM_AE
        ON
        CACHE BOOL "" FORCE)
    set(ENABLE_SKYRIM_VR
        ON
        CACHE BOOL "" FORCE)
    set(BUILD_TESTS
        OFF
        CACHE BOOL "" FORCE)
    set(SKSE_SUPPORT_XBYAK
        OFF
        CACHE BOOL "" FORCE)

    # With SE, AE, and VR all enabled, Prebuilt.cmake fetches a prebuilt CommonLibSSE.lib on
    # GITHUB_ACTIONS. That artifact is MSVC-only and cannot link with the clang-cl cross toolchain.
    # COMMONLIB_PREBUILT=OFF does not stop the fetch, so the env var is unset for this call only.
    set(_github_actions "$ENV{GITHUB_ACTIONS}")
    unset(ENV{GITHUB_ACTIONS})
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/lib/commonlibsse-ng" "${CMAKE_CURRENT_BINARY_DIR}/commonlibsse-ng"
                     EXCLUDE_FROM_ALL)
    set(ENV{GITHUB_ACTIONS} "${_github_actions}")

    include("${CMAKE_CURRENT_SOURCE_DIR}/lib/commonlibsse-ng/cmake/CommonLibSSE.cmake")

    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/src")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/src/version.rc" @ONLY)

    # add_commonlibsse_plugin generates SKSEPlugin_Version, so Plugin.cpp only defines SKSEPlugin_Load.
    add_commonlibsse_plugin(
        ${PROJECT_NAME}
        NAME
        "WarmHands"
        AUTHOR
        "codepuncher"
        SOURCES
        src/Plugin.cpp
        src/Config.h
        src/Config.cpp
        src/FireMagic.h
        src/FireMagic.cpp
        src/GameEvents.h
        src/GameEvents.cpp
        src/HeatSource.h
        src/HeatSource.cpp
        src/HoldInput.h
        src/HoldInput.cpp
        src/HoldTracker.h
        src/IdleDecision.h
        src/KeyNames.h
        src/MenuUI.h
        src/MenuUI.cpp
        src/PlayerState.h
        src/PlayerState.cpp
        src/RangeOverlay.h
        src/RangeOverlay.cpp
        src/Settings.h
        src/Ticker.h
        src/Ticker.cpp
        src/WarmthSource.h
        "${CMAKE_CURRENT_BINARY_DIR}/src/version.rc"
        USE_ADDRESS_LIBRARY)

    source_group("Resources" FILES "${CMAKE_CURRENT_BINARY_DIR}/src/version.rc")

    # cmake_llvm_rc preprocesses RC files using the target's INCLUDE_DIRECTORIES, not CMAKE_C_FLAGS.
    # The xwin SDK paths are in CMAKE_C_FLAGS_INIT as /imsvc flags and are invisible to cmake_llvm_rc,
    # so we add them explicitly here for the RC compiler to find winres.h.
    if(DEFINED XWIN_DIR)
        target_include_directories(
            ${PROJECT_NAME} SYSTEM PRIVATE "$<$<COMPILE_LANGUAGE:RC>:${XWIN_DIR}/sdk/include/um>"
                                           "$<$<COMPILE_LANGUAGE:RC>:${XWIN_DIR}/sdk/include/shared>")
    endif()

    target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)

    target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
                                                       "${CMAKE_CURRENT_BINARY_DIR}/src")
    target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/lib/skse-mcp/include")
    # windows.h (via SimpleIni) otherwise defines max(), which breaks numeric_limits<>::max().
    # Its winspool AddForm macro also renames BGSListForm::AddForm.
    target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

    set_target_properties(
        ${PROJECT_NAME}
        PROPERTIES OUTPUT_NAME WarmHands
                   PREFIX ""
                   SUFFIX ".dll")

    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE /Zi "$<$<CONFIG:Release>:/Zc:inline;/JMC->")
        target_link_options(${PROJECT_NAME} PRIVATE "$<$<CONFIG:Debug>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF>"
                            "$<$<CONFIG:Release>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>")
    endif()

    # Setting IPO on a toolchain that doesn't support it is a fatal error.
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _ipo_supported OUTPUT _ipo_output)
    if(_ipo_supported)
        set_target_properties(${PROJECT_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    else()
        message(VERBOSE "IPO not supported: ${_ipo_output}")
    endif()

    if(DEFINED ENV{SKYRIM_MODS_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_MODS_FOLDER}")
        set(_default_deploy "$ENV{SKYRIM_MODS_FOLDER}/${PROJECT_NAME}")
    elseif(DEFINED ENV{SKYRIM_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_FOLDER}/Data")
        set(_default_deploy "$ENV{SKYRIM_FOLDER}/Data")
    else()
        set(_default_deploy "${CMAKE_BINARY_DIR}/deploy/${PROJECT_NAME}")
    endif()

    set(DEPLOY_DIR_OVERRIDE
        ""
        CACHE
            PATH
            "Override deploy destination (leave empty to derive from SKYRIM_MODS_FOLDER or SKYRIM_FOLDER in the environment, optionally sourced from .env)"
    )

    # A cached DEPLOY_DIR would shadow the env-derived path.
    if(DEFINED CACHE{DEPLOY_DIR})
        message(WARNING "DEPLOY_DIR is no longer a cache variable and has been removed. "
                        "To pin a fixed deploy path, set -DDEPLOY_DIR_OVERRIDE=... instead.")
        unset(DEPLOY_DIR CACHE)
    endif()

    # Recomputed on every configure so environment changes take effect.
    unset(DEPLOY_DIR CACHE)
    if(NOT DEPLOY_DIR_OVERRIDE STREQUAL "")
        set(DEPLOY_DIR "${DEPLOY_DIR_OVERRIDE}")
    else()
        set(DEPLOY_DIR "${_default_deploy}")
    endif()

    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "${DEPLOY_DIR}/SKSE/Plugins")

    install(
        FILES "$<TARGET_PDB_FILE:${PROJECT_NAME}>"
        DESTINATION "${DEPLOY_DIR}/SKSE/Plugins"
        OPTIONAL)

    install(FILES "WarmHands.ini" DESTINATION "${DEPLOY_DIR}/SKSE/Plugins")

    set(_deploy_deps ${PROJECT_NAME})
    if(PLUGIN_BUILD_ESL)
        # WarmHands.esl is generated from its Spriggit YAML in plugin/WarmHands (needs the .NET SDK).
        include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Spriggit.cmake")

        file(GLOB_RECURSE _plugin_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/plugin/WarmHands/*")
        set(_esl "${CMAKE_BINARY_DIR}/WarmHands.esl")
        add_custom_command(
            OUTPUT "${_esl}"
            COMMAND "${SPRIGGIT_EXECUTABLE}" deserialize --InputPath "${CMAKE_CURRENT_SOURCE_DIR}/plugin/WarmHands"
                    --OutputPath "${_esl}"
            DEPENDS ${_plugin_sources} "${SPRIGGIT_EXECUTABLE}"
            COMMENT "Building WarmHands.esl from plugin/WarmHands"
            VERBATIM)
        add_custom_target(WarmHandsPlugin ALL DEPENDS "${_esl}")
        install(FILES "${_esl}" DESTINATION "${DEPLOY_DIR}")
        list(APPEND _deploy_deps WarmHandsPlugin)
    endif()

    # `cmake --build --preset deploy`  (or  cmake --workflow --preset deploy)
    add_custom_target(
        deploy
        COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}"
        DEPENDS ${_deploy_deps}
        COMMENT "Deploying to ${DEPLOY_DIR}")

endif()
