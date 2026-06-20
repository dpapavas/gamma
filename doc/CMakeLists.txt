set(program_SOURCES src/operation.h src/operation.cpp src/evaluation.cpp
  src/basic_operations.h src/compose_tag.h src/compose_tag.cpp src/rewrites.cpp
  src/main.cpp src/extrusion_operation.cpp src/sink_operations.cpp
  src/projection.cpp src/options.cpp

  src/boxed_operations.h src/frontend.h src/frontend.cpp src/scheme_frontend.cpp
  src/mesh_operations.cpp src/misc_polygon_operations.cpp src/selection.cpp
  src/chamfering_operations.cpp

  src/test/main.cpp src/test/rewrites.cpp src/test/fixtures.h
  src/test/frontend.cpp src/test/selection.cpp src/test/circle_polygon_tests.cpp
  src/test/conic_polygon_tests.cpp src/test/polygon_tests.cpp
  src/test/mesh_tests.cpp src/test/chamfering_tests.cpp

  db/common.h db/main.c db/windows.c db/viewports.c db/commands.c

  doc/program_frontend_introduction)

set(manual_SOURCES doc/manual_introduction doc/manual_getting_started
  src/options.cpp)

string(TIMESTAMP UPDATED "%B %Y")

foreach(WHAT "program;program" "manual;scheme" "manual;lua")
  list(POP_FRONT WHAT DOCUMENT VARIANT)

  foreach(FORMAT "pdf" "html")
    set(
      WEAVE_DIR
      ${CMAKE_CURRENT_BINARY_DIR}/weave_${DOCUMENT}_${VARIANT}_${FORMAT})

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
                    -v document=${DOCUMENT}
                    -v variant=${VARIANT}
                    -v format=${FORMAT}
                    -v path=${NAME}
                    -v srcdir=${CMAKE_CURRENT_SOURCE_DIR}
                    -v bindir=${PROJECT_BINARY_DIR}
                    -- ${PROJECT_SOURCE_DIR}/${SOURCE} > ${NAME}.texi
      )

      list(APPEND ${DOCUMENT}_${VARIANT}_${FORMAT}_TEXINFOS ${WEAVE_DIR}/${NAME}.texi)
    endforeach()

    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${VARIANT}.pdf
      DEPENDS ${${DOCUMENT}_${VARIANT}_${FORMAT}_TEXINFOS}
      ${CMAKE_CURRENT_SOURCE_DIR}/${DOCUMENT}.texi
      WORKING_DIRECTORY ${WEAVE_DIR}
      COMMAND texi2any -D "gamma_version ${GAMMA_VERSION}"
                       -D "gammadb_version ${GAMMADB_VERSION}"
                       -D "updated ${UPDATED}"
                       --latex -o ${DOCUMENT}.tex
                       ${CMAKE_CURRENT_SOURCE_DIR}/${DOCUMENT}.texi
      COMMAND texi2pdf --build=local ${DOCUMENT}.tex
                       -o ${CMAKE_CURRENT_BINARY_DIR}/${VARIANT}.pdf
    )

    if(NOT ${DOCUMENT} STREQUAL "manual")
      continue()
    endif()

    add_custom_command(
      OUTPUT ${WEAVE_DIR}/index.html
      DEPENDS ${${DOCUMENT}_${VARIANT}_${FORMAT}_TEXINFOS}
      ${CMAKE_CURRENT_SOURCE_DIR}/${DOCUMENT}.texi
      WORKING_DIRECTORY ${WEAVE_DIR}
      COMMAND texi2any -D "gamma_version ${GAMMA_VERSION}"
                       -D "gammadb_version ${GAMMADB_VERSION}"
                       -D "updated ${UPDATED}"
                       -c "HTML_MATH mathjax"
                       -c "HIGHLIGHT_SYNTAX source-highlight"
                       --html --split chapter
                       --css-ref=https://www.gnu.org/style.css
                       -o ${WEAVE_DIR}/
                       ${CMAKE_CURRENT_SOURCE_DIR}/${DOCUMENT}.texi
    )
  endforeach()

  add_custom_target(weave-${VARIANT}-pdf ALL DEPENDS ${VARIANT}.pdf)
  set_target_properties(weave-${VARIANT}-pdf PROPERTIES EXCLUDE_FROM_ALL TRUE)

  add_custom_target(
    weave-${VARIANT}-html ALL DEPENDS ${WEAVE_DIR}/index.html)
  set_target_properties(weave-${VARIANT}-html PROPERTIES EXCLUDE_FROM_ALL TRUE)

endforeach()
