cmake_minimum_required(VERSION 3.20)
project(toolkit C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

file(GLOB_RECURSE TOOLKIT_SOURCES "src/*.c")
add_library(tk STATIC ${TOOLKIT_SOURCES})

target_include_directories(tk PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/vendor")

target_include_directories(tk PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

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
