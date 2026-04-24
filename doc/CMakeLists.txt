set(program_SOURCES src/operation.h src/operation.cpp src/evaluation.cpp
  src/basic_operations.h src/compose_tag.h src/compose_tag.cpp src/rewrites.cpp
  src/main.cpp src/extrusion_operation.cpp src/sink_operations.cpp

  src/boxed_operations.h src/frontend.h src/frontend.cpp src/scheme_frontend.cpp
  src/mesh_operations.cpp src/misc_polygon_operations.cpp src/selection.cpp
  src/chamfering_operations.cpp

  src/test/fixtures.h src/test/rewrites.cpp src/test/frontend.cpp
  src/test/selection.cpp src/test/circle_polygon_tests.cpp
  src/test/conic_polygon_tests.cpp src/test/polygon_tests.cpp
  src/test/mesh_tests.cpp src/test/chamfering_tests.cpp

  db/common.h db/main.c db/windows.c db/viewports.c db/commands.c

  doc/program_frontend_introduction)

set(manual_SOURCES doc/manual_introduction)

foreach(WHAT "program;program" "manual;scheme" "manual;lua")
  list(POP_FRONT WHAT DOCUMENT VARIANT)

  set(WEAVE_DIR ${CMAKE_CURRENT_BINARY_DIR}/weave_${DOCUMENT}_${VARIANT})

  add_custom_command(
    OUTPUT ${WEAVE_DIR}/.file
    COMMAND ${CMAKE_COMMAND} -E make_directory ${WEAVE_DIR}
    COMMAND ${CMAKE_COMMAND} -E touch ${WEAVE_DIR}/.file
  )

  foreach(SOURCE ${${DOCUMENT}_SOURCES})
    if(SOURCE MATCHES "^doc/.*$")
      set(PREFIX)
    else()
      set(PREFIX "//")
    endif()

    string(REGEX REPLACE "[/.]" "_" NAME ${SOURCE})

    add_custom_command(
      OUTPUT ${WEAVE_DIR}/${NAME}.texi
      DEPENDS ${PROJECT_SOURCE_DIR}/${SOURCE}
              ${CMAKE_CURRENT_SOURCE_DIR}/*.scm
              ${CMAKE_CURRENT_SOURCE_DIR}/weave.awk
              ${WEAVE_DIR}/.file
      WORKING_DIRECTORY ${WEAVE_DIR}
      COMMAND awk -f ${CMAKE_CURRENT_SOURCE_DIR}/weave.awk
                  -v prefix="${PREFIX}"
                  -v target=${DOCUMENT}/${VARIANT}
                  -v path=${NAME}
                  -v srcdir=${CMAKE_CURRENT_SOURCE_DIR}
                  -v bindir=${PROJECT_BINARY_DIR}
                  -- ${PROJECT_SOURCE_DIR}/${SOURCE} > ${NAME}.texi
    )

    list(APPEND ${DOCUMENT}_${VARIANT}_TEXINFOS ${WEAVE_DIR}/${NAME}.texi)
  endforeach()

  add_custom_command(
    OUTPUT ${VARIANT}.pdf
    DEPENDS ${${DOCUMENT}_${VARIANT}_TEXINFOS}
            ${CMAKE_CURRENT_SOURCE_DIR}/${DOCUMENT}.texi
    WORKING_DIRECTORY ${WEAVE_DIR}
    COMMAND texi2any --latex ${CMAKE_CURRENT_SOURCE_DIR}/${DOCUMENT}.texi -o ${DOCUMENT}.tex
    COMMAND texi2pdf --build=local ${DOCUMENT}.tex -o ${CMAKE_CURRENT_BINARY_DIR}/${VARIANT}.pdf
  )

  add_custom_target(weave-${VARIANT} ALL DEPENDS ${VARIANT}.pdf)
  set_target_properties(weave-${VARIANT} PROPERTIES EXCLUDE_FROM_ALL TRUE)
endforeach()
