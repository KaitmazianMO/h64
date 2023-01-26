cmake_minimum_required(VERSION 3.14)

include(cmake/prelude.cmake)

project(
    h64
    VERSION 0.1.0
    DESCRIPTION "Flat hash table in C"
    HOMEPAGE_URL "https://example.com/"
    LANGUAGES C
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

# ---- Declare library ----

add_library(
    h64_h64
    source/h64.c
)
add_library(h64::h64 ALIAS h64_h64)

include(GenerateExportHeader)
generate_export_header(
    h64_h64
    BASE_NAME h64
    EXPORT_FILE_NAME export/h64/h64_export.h
)

if(NOT BUILD_SHARED_LIBS)
  target_compile_definitions(h64_h64 PUBLIC H64_STATIC_DEFINE)
endif()

set_target_properties(
    h64_h64 PROPERTIES
    C_VISIBILITY_PRESET hidden
    VERSION "${PROJECT_VERSION}"
    SOVERSION "${PROJECT_VERSION_MAJOR}"
    EXPORT_NAME h64
    OUTPUT_NAME h64
)

target_include_directories(
    h64_h64 ${warning_guard}
    PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)

target_include_directories(
    h64_h64 SYSTEM
    PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/export>"
)

target_compile_features(h64_h64 PUBLIC c_std_99)

include(CheckCCompilerFlag)
CHECK_C_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
if(COMPILER_SUPPORTS_MARCH_NATIVE)
    target_compile_options(h64_h64 PRIVATE -march=native)
else()
    message(
        FATAL_ERROR
        "h64 lib requires march=natvie flag."
    )
endif()

# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

# ---- Developer mode ----

if(NOT h64_DEVELOPER_MODE)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  message(
      AUTHOR_WARNING
      "Developer mode is intended for developers of h64"
  )
endif()

include(cmake/dev-mode.cmake)
