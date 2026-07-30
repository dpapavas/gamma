find_program(TEXI2ANY texi2any)
find_program(TEXI2PDF texi2pdf)
find_program(AWK NAMES awk gawk)

if(NOT (AWK AND TEXI2ANY AND TEXI2PDF))
  return()
endif()

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
   doc/manual_using_the_debugger src/options.cpp db/commands.c db/main.c
   doc/manual_installing_gamma)

string(TIMESTAMP UPDATED "%B %Y")

set(
  TEXI2ANY_WITH_DEFS
  ${TEXI2ANY} -D "gamma_version ${GAMMA_VERSION}"
  -D "gammadb_version ${GAMMADB_VERSION}"
  -D "updated ${UPDATED}"
)

foreach(WHAT "program;program" "manual;scheme" "manual;lua")
  list(POP_FRONT WHAT DOCUMENT VARIANT)

  set(
    WEAVE_DIR_PREFIX
    ${CMAKE_CURRENT_BINARY_DIR}/weave_${DOCUMENT}_${VARIANT})

  # Invoke Awk to weave input files into .texi files.

  foreach(FORMAT "pdf" "html")
    set(WEAVE_DIR ${WEAVE_DIR_PREFIX}_${FORMAT})

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
        COMMAND ${AWK} -f ${CMAKE_CURRENT_SOURCE_DIR}/weave.awk
                       -v prefix="${PREFIX}"
                       -v document=${DOCUMENT}
                       -v variant=${VARIANT}
                       -v format=${FORMAT}
                       -v path=${NAME}
                       -v srcdir=${CMAKE_CURRENT_SOURCE_DIR}
                       -v bindir=${PROJECT_BINARY_DIR}
                       -v gamma_version=${GAMMA_VERSION}
                       -- ${PROJECT_SOURCE_DIR}/${SOURCE} > ${NAME}.texi
      )

      list(APPEND ${DOCUMENT}_${VARIANT}_${FORMAT}_TEXINFOS ${WEAVE_DIR}/${NAME}.texi)
    endforeach()
  endforeach()

  # Produce the manual in PDF form.

  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${VARIANT}.pdf
    DEPENDS ${${DOCUMENT}_${VARIANT}_pdf_TEXINFOS}
            ${CMAKE_CURRENT_SOURCE_DIR}/${DOCUMENT}*.texi
            gamma
            gammadb
    WORKING_DIRECTORY ${WEAVE_DIR_PREFIX}_pdf

    COMMAND ${TEXI2ANY_WITH_DEFS}
            --latex -o ${DOCUMENT}.tex
            ${CMAKE_CURRENT_SOURCE_DIR}/${DOCUMENT}.texi

    COMMAND ${TEXI2PDF} --build=local ${DOCUMENT}.tex
            -o ${CMAKE_CURRENT_BINARY_DIR}/${VARIANT}.pdf
  )

  add_custom_target(weave-${VARIANT}-pdf DEPENDS ${VARIANT}.pdf)

  if(NOT ${DOCUMENT} STREQUAL "manual")
    continue()
  endif()

  # Produce the manual in HTML form.

  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${VARIANT}.html/index.html
    DEPENDS ${${DOCUMENT}_${VARIANT}_html_TEXINFOS}
            ${CMAKE_CURRENT_SOURCE_DIR}/${DOCUMENT}*.texi
            gamma
            gammadb
    WORKING_DIRECTORY ${WEAVE_DIR_PREFIX}_html

    COMMAND ${TEXI2ANY_WITH_DEFS}
            -c "HTML_MATH mathjax"
            -c "HIGHLIGHT_SYNTAX source-highlight"
            --html --split chapter
            --css-ref=https://www.gnu.org/style.css
            -o ${CMAKE_CURRENT_BINARY_DIR}/${VARIANT}.html/
            ${CMAKE_CURRENT_SOURCE_DIR}/${DOCUMENT}.texi

    COMMAND ${CMAKE_COMMAND} -E copy
            ${WEAVE_DIR_PREFIX}_html/*.jpg
            ${CMAKE_CURRENT_BINARY_DIR}/${VARIANT}.html/
  )

  if(NOT ${DOCUMENT} STREQUAL "manual")
    continue()
  endif()

  add_custom_target(
    weave-${VARIANT}-html
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${VARIANT}.html/index.html)
endforeach()

# Produce a README file.

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/README
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/README*.texi

  COMMAND ${TEXI2ANY_WITH_DEFS}
          --plaintext --paragraph-indent=none
          -o ${CMAKE_CURRENT_BINARY_DIR}/README
          ${CMAKE_CURRENT_SOURCE_DIR}/README.texi
)

add_custom_target(weave-README DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/README)

# Produce an INSTALL file.

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/INSTALL
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/manual_installing_gamma

  COMMAND ${AWK} -f ${CMAKE_CURRENT_SOURCE_DIR}/weave.awk
                 -v prefix=""
                 -v document=manual
                 -v gamma_version=${GAMMA_VERSION}
                 -- ${CMAKE_CURRENT_SOURCE_DIR}/manual_installing_gamma
          | ${TEXI2ANY}
                 --plaintext --no-number-sections --paragraph-indent=none
                 -o ${CMAKE_CURRENT_BINARY_DIR}/INSTALL -
)

add_custom_target(weave-INSTALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/INSTALL)
