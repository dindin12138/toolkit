cmake_minimum_required(VERSION 3.20)
project(toolkit C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- Explicit source list (replaces file(GLOB_RECURSE ...)) ---
# A glob would silently miss newly added .c files until the next configure run;
# an explicit list keeps the build deterministic and reviewable.
add_library(tk STATIC src/core/error.c src/ds/vec.c src/ds/list.c)

target_include_directories(tk PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/vendor")
target_include_directories(tk PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# --- Warning flags (shared by the library and every test target) ---
# These are added to *both* the library and the tests so that warnings cannot
# hide in the test sources. We deliberately do NOT add -Werror: warnings must
# remain visible but must not break the build.
set(TOOLKIT_WARNING_FLAGS "")
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  set(TOOLKIT_WARNING_FLAGS -Wall -Wextra -Wpedantic)
endif()

target_compile_options(tk PRIVATE ${TOOLKIT_WARNING_FLAGS})

# --- Narrow, test-only hardening: -Werror=incompatible-function-pointer-types ---
# test/ds/test_vec.c hard-pins the *exact* function type of the six P1-2
# accessors by assigning them to explicitly typed function pointers. Function
# pointer assignment demands an exact type match, so any signature drift
# (e.g. tk_vec_at regressing from `const void *` to `void *`) becomes an
# incompatible-function-pointer-types diagnostic. Promoting just that one
# diagnostic to an error turns the pins into a hard, compiler-independent
# guarantee, without reintroducing a blanket -Werror.
#
# Scope decisions:
#   * TEST TARGETS ONLY -- the tk library keeps its -Werror-free warning policy.
#   * Clang only -- GCC does not implement this exact diagnostic name
#     (GCC folds the case into -Wincompatible-pointer-types); we keep the flag
#     conservative so the build stays warning-clean on both toolchains.
set(TOOLKIT_TEST_ONLY_WERROR_FLAGS "")
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
  set(TOOLKIT_TEST_ONLY_WERROR_FLAGS
      -Werror=incompatible-function-pointer-types)
endif()

# --- Optional sanitizer switch ---
# Note: enabling this only guarantees that the project configures, compiles and
# links with ASan/UBSan. Running the sanitized binaries is the caller's choice.
option(TOOLKIT_ENABLE_SANITIZERS "Enable ASan/UBSan" OFF)
if(TOOLKIT_ENABLE_SANITIZERS)
  target_compile_options(tk PUBLIC -fsanitize=address,undefined
                                   -fno-omit-frame-pointer -g)
  target_link_options(tk PUBLIC -fsanitize=address,undefined)
endif()

option(TOOLKIT_BUILD_TESTS "Build the toolkit tests" ON)

if(TOOLKIT_BUILD_TESTS)
  enable_testing()

  find_package(PkgConfig REQUIRED)
  pkg_check_modules(Criterion REQUIRED QUIET criterion)

  # --- Explicit test source list (replaces file(GLOB_RECURSE ...)) ---
  # New test files MUST be registered here or they will not be compiled.
  set(TOOLKIT_TEST_SOURCES
      test/core/test_headers.c test/core/test_iterator.c
      test/core/test_iterator_random_access.c test/ds/test_vec.c
      test/ds/test_list.c test/algo/test_vec_sequence.c
      test/algo/test_list_sequence.c test/algo/test_vec_algorithms.c
      test/algo/test_list_algorithms.c test/algo/test_algo_numeric.c
      test/algo/test_algo_cross_container.c)

  foreach(test_source IN LISTS TOOLKIT_TEST_SOURCES)
    get_filename_component(module_name ${test_source} NAME_WE)

    set(test_target "criterion_${module_name}")

    add_executable(${test_target} ${test_source})

    target_include_directories(${test_target} PRIVATE ${Criterion_INCLUDE_DIRS})
    target_link_libraries(${test_target} PRIVATE tk ${Criterion_LIBRARIES})
    target_compile_options(${test_target} PRIVATE ${TOOLKIT_WARNING_FLAGS}
                                                  ${TOOLKIT_TEST_ONLY_WERROR_FLAGS})

    add_test(NAME ${test_target} COMMAND ${test_target})
  endforeach()
endif()
