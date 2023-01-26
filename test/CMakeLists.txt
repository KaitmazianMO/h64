cmake_minimum_required(VERSION 3.14)

project(h64Tests LANGUAGES C)

include(../cmake/project-is-top-level.cmake)
include(../cmake/folders.cmake)
include(../cmake/windows-set-path.cmake)

# ---- Dependencies ----

if(PROJECT_IS_TOP_LEVEL)
  find_package(h64 REQUIRED)
  enable_testing()
endif()

# ---- Tests ----

add_executable(h64_test source/h64_test.c)
target_link_libraries(h64_test PRIVATE h64::h64)
target_compile_features(h64_test PRIVATE c_std_99)

add_test(NAME h64_test COMMAND h64_test)
windows_set_path(h64_test h64::h64)

# ---- End-of-file commands ----

add_folders(Test)
