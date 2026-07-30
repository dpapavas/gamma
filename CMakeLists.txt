set(
  SANITIZE_ADDRESS_OPTIONS
  "-fsanitize=address -fsanitize=leak -fno-omit-frame-pointer")

set(
  CMAKE_CXX_FLAGS_DEBUG "-O0 -g"
  CACHE STRING "Compilation C++ flags for debug build")

set(
  CMAKE_C_FLAGS_DEBUG "-O0 -g"
  CACHE STRING "Compilation C flags for debug build")

set(
  CMAKE_CXX_FLAGS_OPTIMIZE "-O3 -g"
  CACHE STRING "Compilation C++ flags for optimized build")

set(
  CMAKE_C_FLAGS_OPTIMIZE "-O3 -g"
  CACHE STRING "Compilation C flags for optimized build")

# CGAL_DEBUG keeps CGAL assertions enabled, even if NDEBUG is defined.

set(
  CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -DCGAL_DEBUG"
  CACHE STRING "Compilation C++ flags for release build")

set(
  CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG="
  CACHE STRING "Compilation C flags for release build")

set(
  CMAKE_CXX_FLAGS_SANITIZEADDRESS "-O0 -g ${SANITIZE_ADDRESS_OPTIONS}"
  CACHE STRING "Compilation C++ flags for AddressSanitizer build")

set(
  CMAKE_C_FLAGS_SANITIZEADDRESS "-O0 -g ${SANITIZE_ADDRESS_OPTIONS}"
  CACHE STRING "Compilation C flags for AddressSanitizer build")

set(
  LINK_FLAGS_SANITIZEADDRESS ${SANITIZE_ADDRESS_OPTIONS}
  CACHE STRING "Linking flags for AddressSanitizer build")

set(
  CMAKE_CXX_FLAGS_SANITIZETHREAD "-O2 -g -fsanitize=thread "
  CACHE STRING "Compilation C++ flags for ThreadSanitizer build")

set(
  CMAKE_C_FLAGS_SANITIZETHREAD "-O2 -g -fsanitize=thread "
  CACHE STRING "Compilation C flags for ThreadSanitizer build")

set(
  LINK_FLAGS_SANITIZETHREAD "-fsanitize=thread"
  CACHE STRING "Linking flags for ThreadSanitizer build")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

option(USE_GUILE "Try to build the Scheme backend" TRUE)
option(USE_LUA "Try to build the Lua backend" TRUE)

cmake_minimum_required(VERSION 3.21...3.29)
project(main)
Set(GAMMA_VERSION "0.6.0")
set(GAMMADB_VERSION "0.1.0")

if (POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif()

if (POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Wno-unused-parameter)

add_subdirectory(src)
add_subdirectory(db)
add_subdirectory(doc)

add_custom_target(
  dist
  DEPENDS weave-README weave-INSTALL
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND git ls-tree --full-tree -r --name-only HEAD
      | tar -zcf ${CMAKE_CURRENT_BINARY_DIR}/gamma-${GAMMA_VERSION}.tar.gz
      --exclude='.*' --transform 's/^/gamma-${GAMMA_VERSION}\\//' --files-from=-
      -C ${CMAKE_CURRENT_BINARY_DIR}/doc README INSTALL
)
