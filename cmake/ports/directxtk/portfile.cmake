vcpkg_from_github(
    OUT_SOURCE_PATH
    SOURCE_PATH
    REPO
    Microsoft/DirectXTK
    REF
    may2026
    SHA512
    9306774b06f52b4c37938fe4b3a10df8c7a85652188a25dc25e60ae9ff6fbf9d2cf920b114de3fc3945c564054cbb166cc45fca073021129e75ce282a51636e7
    HEAD_REF
    main)

# Headers only (shader compilation requires fxc.exe, Windows-only)
file(INSTALL "${SOURCE_PATH}/Inc/" DESTINATION "${CURRENT_PACKAGES_DIR}/include")

# Create header-only CMake target for find_package(directxtk CONFIG REQUIRED)
file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/share/directxtk")

file(
    WRITE "${CURRENT_PACKAGES_DIR}/share/directxtk/directxtkTargets.cmake"
    [[
if(NOT TARGET Microsoft::DirectXTK)
    add_library(Microsoft::DirectXTK INTERFACE IMPORTED GLOBAL)
    set_target_properties(Microsoft::DirectXTK PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include"
    )
endif()
]])

file(WRITE "${CURRENT_PACKAGES_DIR}/share/directxtk/directxtkConfig.cmake"
     "include(\"\${CMAKE_CURRENT_LIST_DIR}/directxtkTargets.cmake\")\n")

file(
    INSTALL "${SOURCE_PATH}/LICENSE"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
    RENAME copyright)
