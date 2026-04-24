include(GNUInstallDirs)

# Find CGAL and its components and include helper file.

find_package(CGAL COMPONENTS REQUIRED)

find_package(Boost REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Eigen3 3.1.0 REQUIRED)
include(CGAL_Eigen3_support)

find_package(TBB)
include(CGAL_TBB_support)
if (NOT TARGET CGAL::TBB_support)
  message(STATUS "Intel TBB was not found.")
endif ()

add_definitions(-DCGAL_HAS_THREADS)

# Build a set of object files, which can be used for both the main and
# test binaries.

add_library(
  objects OBJECT

  operation.cpp polyhedron_operations.cpp extrusion_operation.cpp
  polygon_operations.cpp circle_polygon_operations.cpp
  conic_polygon_operations.cpp misc_polygon_operations.cpp
  sink_operations.cpp mesh_operations.cpp deform_operations.cpp
  chamfering_operations.cpp

  bounding_volumes.cpp compressed_stream.cpp compose_tag.cpp
  evaluation.cpp projection.cpp rewrites.cpp selection.cpp sandbox.cpp
  transformations.cpp options.cpp frontend.cpp)

add_executable(gamma main.cpp)

# Valgrind doesn't cooperate with CGAL's messing with the FPU's
# rounding modes.  Setting this for debug builds

target_compile_definitions(objects PRIVATE
  $<$<CONFIG:Debug>:CGAL_DISABLE_ROUNDING_MATH_CHECK>
)

target_compile_options(objects PRIVATE
    $<$<OR:$<CONFIG:Release>,$<CONFIG:Optimize>>:-Wno-maybe-uninitialized -Wno-array-bounds>
)

target_include_directories(
  objects PUBLIC ${ZLIB_INCLUDE_DIR})
target_link_options(objects PUBLIC -pthread)

if (WIN32)
target_link_options(objects PUBLIC -static)
endif ()

target_link_libraries(objects PUBLIC CGAL::CGAL CGAL::Eigen3_support
  ${ZLIB_LIBRARIES})

# Scheme backend

if (USE_GUILE)
   # Find Guile using pkg-config
   find_package(PkgConfig REQUIRED)
   pkg_check_modules(GUILE REQUIRED guile-3.0)

   message(STATUS "Guile include dirs: ${GUILE_INCLUDE_DIRS}")
   message(STATUS "Guile libs: ${GUILE_LIBRARIES}")

  if (GUILE_FOUND)
    target_sources(objects PRIVATE scheme_frontend.cpp)
    target_include_directories(objects PUBLIC ${GUILE_INCLUDE_DIRS})
    target_link_directories(objects PUBLIC ${GUILE_LIBRARY_DIRS})
    target_link_libraries(objects PUBLIC ${GUILE_LIBRARIES})
    target_compile_definitions(objects PUBLIC HAVE_SCHEME)
    target_compile_definitions(gamma PUBLIC HAVE_SCHEME)
  else ()
    message(
      STATUS
      "Guile was not found.  The Scheme language backend won't be "
      "available.")
  endif ()
endif ()

# Lua backend

if (USE_LUA)
  find_package(Lua)

  if (Lua_FOUND)
    target_sources(objects PRIVATE lua_frontend.cpp)
    target_include_directories(objects PUBLIC ${LUA_INCLUDE_DIR})
    target_link_libraries(objects PUBLIC ${LUA_LIBRARIES})
    target_compile_definitions(objects PUBLIC HAVE_LUA)
    target_compile_definitions(gamma PUBLIC HAVE_LUA)
  else ()
    message(
      STATUS
      "Lua was not found.  The Lua language backend won't be available.")
  endif ()
endif ()

# Create the main executable.

target_compile_definitions(gamma PUBLIC "PROJECT_SOURCE_DIR=\"${PROJECT_SOURCE_DIR}\"")
target_compile_definitions(gamma PUBLIC "INSTALL_DATADIR=\"${CMAKE_INSTALL_FULL_DATADIR}\"")
target_compile_definitions(gamma PUBLIC "INSTALL_PREFIX=\"${CMAKE_INSTALL_PREFIX}\"")
target_link_libraries(gamma objects)

install(TARGETS gamma)

if (GUILE_FOUND)
  install(
    DIRECTORY ../scheme/
    DESTINATION "${CMAKE_INSTALL_DATADIR}/gamma/scheme"
    FILES_MATCHING PATTERN "*.sld")
endif ()

if (Lua_FOUND)
  install(
    DIRECTORY ../lua
    DESTINATION "${CMAKE_INSTALL_DATADIR}/gamma"
    FILES_MATCHING PATTERN "*.lua")
endif ()

if(NOT CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT CMAKE_BUILD_TYPE STREQUAL "Optimize")
    add_subdirectory(test)
endif()
