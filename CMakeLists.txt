set(
  SANITIZE_ADDRESS_OPTIONS
  "-fsanitize=address -fsanitize=leak -fno-omit-frame-pointer")

set(
  CMAKE_CXX_FLAGS_DEBUG "-O0 -g"
  CACHE STRING "Compilation flags for debug build")

set(
  CMAKE_CXX_FLAGS_OPTIMIZE "-O3"
  CACHE STRING "Compilation flags for optimized build")

# CGAL_DEBUG keeps CGAL assertions enabled, even if NDEBUG is defined.

set(
  CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -DCGAL_DEBUG"
  CACHE STRING "Compilation flags for release build")

set(
  CMAKE_CXX_FLAGS_SANITIZEADDRESS "-O0 -g ${SANITIZE_ADDRESS_OPTIONS}"
  CACHE STRING "Compilation flags for AddressSanitizer build")

set(
  LINK_FLAGS_SANITIZEADDRESS ${SANITIZE_ADDRESS_OPTIONS}
  CACHE STRING "Linking flags for AddressSanitizer build")

set(
  CMAKE_CXX_FLAGS_SANITIZETHREAD "-O2 -g -fsanitize=thread "
  CACHE STRING "Compilation flags for ThreadSanitizer build")

set(
  LINK_FLAGS_SANITIZETHREAD "-fsanitize=thread"
  CACHE STRING "Linking flags for ThreadSanitizer build")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

option(USE_CHIBI "Try to build the Scheme backend" TRUE)
option(USE_LUA "Try to build the Lua backend" TRUE)

project(main)
cmake_minimum_required(VERSION 3.8)

if (POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)

add_definitions(-DCGAL_DISABLE_ROUNDING_MATH_CHECK=ON -DCGAL_HAS_THREADS)
add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)

add_subdirectory(src)
add_subdirectory(test)
