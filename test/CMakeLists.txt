find_package(Boost COMPONENTS unit_test_framework)

if (NOT Boost_UNIT_TEST_FRAMEWORK_FOUND)
  return()
endif ()

find_package(CGAL OPTIONAL_COMPONENTS Qt6)

add_executable(
  test

  main.cpp sandbox.cpp

  polyhedron_tests.cpp polygon_tests.cpp circle_polygon_tests.cpp
  conic_polygon_tests.cpp store.cpp mesh_tests.cpp deform_tests.cpp

  frontend.cpp rewrites.cpp selection.cpp)

if (NOT CGAL_Qt6_FOUND)
  message(STATUS "Qt6 CGAL component not found.")
endif ()

if (USE_CHIBI)
  target_compile_definitions(test PRIVATE "CHIBI_LIB_DIR=\"${CHIBI_LIB_DIR}\"")
endif ()

target_compile_definitions(test PRIVATE "SOURCE_DIR=\"${PROJECT_SOURCE_DIR}\"")
target_include_directories(test PRIVATE ../src)
target_link_libraries(test ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} objects
  CGAL::CGAL_Basic_viewer stdc++fs)
