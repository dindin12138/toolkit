cmake_minimum_required(VERSION 3.20)
project(toolkit C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

file(GLOB_RECURSE TOOLKIT_SOURCES "src/*.c")
add_library(tk STATIC ${TOOLKIT_SOURCES})

target_include_directories(tk PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/vendor")

target_include_directories(tk PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# option(TOOLKIT_BUILD_TESTS "Build the toolkit tests" ON)
#
# if(TOOLKIT_BUILD_TESTS) enable_testing() if(EXISTS
# "${CMAKE_SOURCE_DIR}/test/ds/test_vec.c") add_executable(test_vec
# test/ds/test_vec.c) target_link_libraries(test_vec PRIVATE tk) add_test(NAME
# VecTests COMMAND test_vec) endif() endif()

# --- Testing Configuration with Criterion and PkgConfig ---

# Define a user-configurable option to enable or disable the building of tests.
# option(TOOLKIT_BUILD_TESTS "Build the toolkit tests" ON)
#
# if(TOOLKIT_BUILD_TESTS) # Enable CTest, CMake's built-in testing framework.
# enable_testing()
#
# find_package(PkgConfig REQUIRED)
#
# pkg_check_modules(Criterion REQUIRED criterion)
#
# set(TEST_SOURCE_FILE "${CMAKE_SOURCE_DIR}/test/ds/test_vec.c")
#
# if(EXISTS "${TEST_SOURCE_FILE}") # Define an executable target for our test
# runner. add_executable(test_runner ${TEST_SOURCE_FILE})
#
# target_include_directories(test_runner PRIVATE ${Criterion_INCLUDE_DIRS})
#
# target_link_libraries(test_runner PRIVATE tk ${Criterion_LIBRARIES})
#
# # Register the executable as a test case with CTest. add_test(NAME
# ToolkitTests COMMAND test_runner) endif() endif()

# --- Testing Configuration (Scalable and Compatible Version) ---

option(TOOLKIT_BUILD_TESTS "Build the toolkit tests" ON)

if(TOOLKIT_BUILD_TESTS)
  enable_testing()

  find_package(PkgConfig REQUIRED)

  pkg_check_modules(Criterion REQUIRED QUIET criterion)

  file(GLOB_RECURSE TEST_SOURCE_FILES "test/**/test_*.c")

  foreach(test_source IN LISTS TEST_SOURCE_FILES)
    get_filename_component(module_name ${test_source} NAME_WE)

    set(test_target "criterion_${module_name}")

    add_executable(${test_target} ${test_source})

    target_include_directories(${test_target} PRIVATE ${Criterion_INCLUDE_DIRS})
    target_link_libraries(${test_target} PRIVATE tk ${Criterion_LIBRARIES})

    add_test(NAME ${test_target} COMMAND ${test_target})
  endforeach()
endif()
