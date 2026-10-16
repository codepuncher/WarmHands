# Fetches the Spriggit CLI and sets SPRIGGIT_EXECUTABLE.
set(SPRIGGIT_VERSION 0.41.0)
if(CMAKE_HOST_WIN32)
    set(_spriggit_zip SpriggitCLI.zip)
    set(_spriggit_exe Spriggit.CLI.exe)
    set(_spriggit_sha512
        c30ddf94ffe80f3a5db749bc656381d8f2a8114de6c991385b12509d35723464061f4b0b5e1df7d7958d2430c12ad8d4a00090f759662eb1ac2606f1d3fdd860
    )
else()
    set(_spriggit_zip SpriggitLinuxCLI.zip)
    set(_spriggit_exe Spriggit.CLI)
    set(_spriggit_sha512
        b9369ad90b3ad2e46b7e426576e0424ae2a4b8a580c410d21a803e9c85a366294b131b0565239f819f0d538c7826858ed3163c8f86ae85dff32af6400e6ac4ea
    )
endif()

# Shared by every preset so CI caches one copy.
set(SPRIGGIT_TOOLS_DIR
    "${PROJECT_SOURCE_DIR}/build/tools"
    CACHE PATH "Where the Spriggit CLI is unpacked")
set(_spriggit_dir "${SPRIGGIT_TOOLS_DIR}/spriggit-${SPRIGGIT_VERSION}")
set(SPRIGGIT_EXECUTABLE "${_spriggit_dir}/${_spriggit_exe}")

# Presets share the directory, so concurrent configures take turns unpacking.
file(LOCK "${SPRIGGIT_TOOLS_DIR}/spriggit.lock" GUARD FILE)
if(NOT EXISTS "${SPRIGGIT_EXECUTABLE}")
    set(_spriggit_archive "${CMAKE_BINARY_DIR}/download/${_spriggit_zip}")
    file(
        DOWNLOAD "https://github.com/Mutagen-Modding/Spriggit/releases/download/${SPRIGGIT_VERSION}/${_spriggit_zip}"
        "${_spriggit_archive}"
        EXPECTED_HASH SHA512=${_spriggit_sha512}
        STATUS _spriggit_status)
    list(GET _spriggit_status 0 _spriggit_code)
    if(NOT _spriggit_code EQUAL 0)
        message(FATAL_ERROR "Spriggit download failed: ${_spriggit_status}")
    endif()
    # Unpacked beside the target and renamed into place, so an interrupted extract never looks installed.
    set(_spriggit_partial "${_spriggit_dir}.partial")
    file(REMOVE_RECURSE "${_spriggit_partial}")
    file(MAKE_DIRECTORY "${_spriggit_partial}")
    # file(ARCHIVE_EXTRACT) reports failure without stopping configure.
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E tar xf "${_spriggit_archive}"
        WORKING_DIRECTORY "${_spriggit_partial}"
        RESULT_VARIABLE _spriggit_result)
    if(NOT _spriggit_result EQUAL 0)
        message(FATAL_ERROR "Spriggit extract failed: ${_spriggit_result}")
    endif()
    file(
        CHMOD
        "${_spriggit_partial}/${_spriggit_exe}"
        PERMISSIONS
        OWNER_READ
        OWNER_WRITE
        OWNER_EXECUTE
        GROUP_READ
        GROUP_EXECUTE
        WORLD_READ
        WORLD_EXECUTE)
    file(REMOVE_RECURSE "${_spriggit_dir}")
    file(RENAME "${_spriggit_partial}" "${_spriggit_dir}")
    file(REMOVE "${_spriggit_archive}")
endif()
file(LOCK "${SPRIGGIT_TOOLS_DIR}/spriggit.lock" RELEASE)
