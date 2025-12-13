set(program_SOURCES src/operation.h src/operation.cpp src/evaluation.cpp src/basic_operations.h
  src/compose_tag.h src/compose_tag.cpp src/rewrites.cpp src/main.cpp

  src/boxed_operations.h src/frontend.h src/frontend.cpp src/scheme_frontend.cpp

  src/test/fixtures.h src/test/rewrites.cpp

  db/common.h db/main.c db/windows.c db/viewports.c db/commands.c)

set(manual_SOURCES src/main.cpp)

foreach(WHAT program manual)
  set(WEAVE_DIR ${CMAKE_CURRENT_BINARY_DIR}/weave_${WHAT})

  add_custom_command(
    OUTPUT ${WEAVE_DIR}/.file
    COMMAND ${CMAKE_COMMAND} -E make_directory ${WEAVE_DIR}
    COMMAND ${CMAKE_COMMAND} -E touch ${WEAVE_DIR}/.file
  )

  foreach(SOURCE ${${WHAT}_SOURCES})
    string(REGEX REPLACE "[/.]" "_" NAME ${SOURCE})

    add_custom_command(
      OUTPUT ${WEAVE_DIR}/${NAME}.texi
      DEPENDS ${PROJECT_SOURCE_DIR}/${SOURCE}
              ${CMAKE_CURRENT_SOURCE_DIR}/${WHAT}.texi
              ${CMAKE_CURRENT_SOURCE_DIR}/weave.awk
              ${WEAVE_DIR}/.file
      WORKING_DIRECTORY ${WEAVE_DIR}
      COMMAND awk -f ${CMAKE_CURRENT_SOURCE_DIR}/weave.awk
                  -v target=${WHAT}
                  -v bindir=${PROJECT_BINARY_DIR}
                  -v prefix=${NAME}
                  -- ${PROJECT_SOURCE_DIR}/${SOURCE} > ${NAME}.texi
    )

    list(APPEND ${WHAT}_TEXINFOS ${WEAVE_DIR}/${NAME}.texi)
  endforeach()

  add_custom_command(
    OUTPUT ${WHAT}.pdf
    DEPENDS ${${WHAT}_TEXINFOS}
    WORKING_DIRECTORY ${WEAVE_DIR}
    COMMAND texi2any --latex ${CMAKE_CURRENT_SOURCE_DIR}/${WHAT}.texi -o ${WHAT}.tex
    COMMAND texi2pdf --build=local ${WHAT}.tex -o ${CMAKE_CURRENT_BINARY_DIR}/${WHAT}.pdf
  )

  add_custom_target(weave-${WHAT} ALL DEPENDS ${WHAT}.pdf)
  set_target_properties(weave-${WHAT} PROPERTIES EXCLUDE_FROM_ALL TRUE)
endforeach()
