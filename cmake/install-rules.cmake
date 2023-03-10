if(PROJECT_IS_TOP_LEVEL)
  set(
      CMAKE_INSTALL_INCLUDEDIR "include/h64-${PROJECT_VERSION}"
      CACHE PATH ""
  )
endif()

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# find_package(<package>) call for consumers to find this project
set(package h64)

install(
    DIRECTORY
    include/
    "${PROJECT_BINARY_DIR}/export/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT h64_Development
)

install(
    TARGETS h64_h64
    EXPORT h64Targets
    RUNTIME #
    COMPONENT h64_Runtime
    LIBRARY #
    COMPONENT h64_Runtime
    NAMELINK_COMPONENT h64_Development
    ARCHIVE #
    COMPONENT h64_Development
    INCLUDES #
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

write_basic_package_version_file(
    "${package}ConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion
)

# Allow package maintainers to freely override the path for the configs
set(
    h64_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/${package}"
    CACHE PATH "CMake package config location relative to the install prefix"
)
mark_as_advanced(h64_INSTALL_CMAKEDIR)

install(
    FILES cmake/install-config.cmake
    DESTINATION "${h64_INSTALL_CMAKEDIR}"
    RENAME "${package}Config.cmake"
    COMPONENT h64_Development
)

install(
    FILES "${PROJECT_BINARY_DIR}/${package}ConfigVersion.cmake"
    DESTINATION "${h64_INSTALL_CMAKEDIR}"
    COMPONENT h64_Development
)

install(
    EXPORT h64Targets
    NAMESPACE h64::
    DESTINATION "${h64_INSTALL_CMAKEDIR}"
    COMPONENT h64_Development
)

if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()
