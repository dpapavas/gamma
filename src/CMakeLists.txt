include(GNUInstallDirs)

# Find CGAL and its components and include helper file.

find_package(CGAL COMPONENTS REQUIRED)
include(${CGAL_USE_FILE})

find_package(Boost REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Eigen3 3.1.0 REQUIRED)
include(CGAL_Eigen3_support)

find_package(TBB)
include(CGAL_TBB_support)
if (NOT TARGET CGAL::TBB_support)
  message(STATUS "Intel TBB was not found.")
endif ()

# Build a set of object files, which can be used for both the main and
# test binaries.

add_library(
  objects OBJECT

  operation.cpp polyhedron_operations.cpp extrusion_operation.cpp
  polygon_operations.cpp circle_polygon_operations.cpp
  conic_polygon_operations.cpp misc_polygon_operations.cpp
  sink_operations.cpp mesh_operations.cpp deform_operations.cpp

  bounding_volumes.cpp compressed_stream.cpp compose_tag.cpp
  evaluation.cpp projection.cpp rewrites.cpp selection.cpp sandbox.cpp
  transformations.cpp options.cpp frontend.cpp)

target_include_directories(
  objects PUBLIC ${ZLIB_INCLUDE_DIR})
target_link_options(objects PUBLIC -pthread)

if (WIN32)
target_link_options(objects PUBLIC -static)
endif ()

target_link_libraries(objects PUBLIC
  ${CGAL_LIBRARIES} ${CGAL_3RD_PARTY_LIBRARIES} CGAL::Eigen3_support
  ${ZLIB_LIBRARIES})

# Scheme backend

if (USE_CHIBI)
  find_package(Chibi)

  if (Chibi_FOUND)
    target_sources(objects PRIVATE scheme_frontend.cpp)
    target_include_directories(objects PUBLIC ${CHIBI_INCLUDE_DIR})
    target_link_libraries(objects PUBLIC ${CHIBI_LIBRARIES} util)
    target_compile_definitions(
      objects PUBLIC
      HAVE_SCHEME
      "SCHEME_LIBRARY_DIR=\"${CMAKE_INSTALL_FULL_DATADIR}/gamma/scheme\"")
  else ()
    message(
      STATUS
      "Chibi-Scheme was not found.  The Scheme language backend won't "
      "be available.")
  endif ()
endif ()

# Lua backend

if (USE_LUA)
  find_package(Lua)

  if (Lua_FOUND)
    target_sources(objects PRIVATE lua_frontend.cpp)
    target_include_directories(objects PUBLIC ${LUA_INCLUDE_DIR})
    target_link_libraries(objects PUBLIC ${LUA_LIBRARIES})
    target_compile_definitions(
      objects PUBLIC
      HAVE_LUA
      "LUA_LIBRARY_DIR=\"${CMAKE_INSTALL_FULL_DATADIR}/gamma/lua\"")
  else ()
    message(
      STATUS
      "Lua was not found.  The Lua language backend won't be available.")
  endif ()
endif ()

# Create the main executable.

add_executable(gamma main.cpp)
target_link_libraries(gamma objects)

add_to_cached_list(CGAL_EXECUTABLE_TARGETS gamma)

install(TARGETS gamma)

if (Chibi_FOUND)
  install(
    DIRECTORY "${CHIBI_LIB_DIR}/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/gamma/scheme"
    FILES_MATCHING PATTERN "*.scm" PATTERN "*.sld")

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
