find_package(Boost COMPONENTS unit_test_framework)

if (NOT Boost_UNIT_TEST_FRAMEWORK_FOUND)
  return()
endif ()

find_package(CGAL OPTIONAL_COMPONENTS Qt5)

add_executable(
  test

  main.cpp sandbox.cpp

  polyhedron_tests.cpp polygon_tests.cpp circle_polygon_tests.cpp
  conic_polygon_tests.cpp store.cpp mesh_tests.cpp deform_tests.cpp

  frontend.cpp rewrites.cpp selection.cpp)

if (CGAL_Qt5_FOUND)
  target_compile_definitions(test PRIVATE -DCGAL_USE_BASIC_VIEWER -DQT_NO_KEYWORDS)
else ()
  message(STATUS "Qt5 CGAL component not found.")
endif ()

target_include_directories(test PRIVATE ../src)
target_link_libraries(test ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} objects
  CGAL::CGAL_Qt5 stdc++fs)
