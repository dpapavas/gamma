set(program_SOURCES src/operation.h src/operation.cpp src/evaluation.cpp src/basic_operations.h
  src/compose_tag.h src/compose_tag.cpp src/rewrites.cpp src/main.cpp

  src/boxed_operations.h src/frontend.h src/frontend.cpp src/scheme_frontend.cpp

  src/test/fixtures.h src/test/rewrites.cpp

  db/common.h db/main.c db/windows.c db/viewports.c db/commands.c)

set(manual_SOURCES src/main.cpp)

foreach(WHAT program manual)
  foreach(X ${${WHAT}_SOURCES})
    add_custom_command(
      OUTPUT ${PROJECT_BINARY_DIR}/${X}.${WHAT}.texi
      DEPENDS ${PROJECT_SOURCE_DIR}/${X}
      ${CMAKE_CURRENT_SOURCE_DIR}/${WHAT}.texi
      ${CMAKE_CURRENT_SOURCE_DIR}/weave.awk
      COMMAND awk -f ${CMAKE_CURRENT_SOURCE_DIR}/weave.awk
      -v target=${WHAT}
      -v bindir=${PROJECT_BINARY_DIR}
      -v prefix=${PROJECT_BINARY_DIR}/${X}.${WHAT}
      -- ${PROJECT_SOURCE_DIR}/${X} > ${PROJECT_BINARY_DIR}/${X}.${WHAT}.texi
    )

    list(APPEND ${WHAT}_TEXINFOS ${PROJECT_BINARY_DIR}/${X}.${WHAT}.texi)
  endforeach()

  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${WHAT}.tex
    DEPENDS ${${WHAT}_TEXINFOS}
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    COMMAND texi2any --latex ${CMAKE_CURRENT_SOURCE_DIR}/${WHAT}.texi
                     -o ${CMAKE_CURRENT_BINARY_DIR}/${WHAT}.tex
  )

  add_custom_command(
    OUTPUT ${WHAT}.pdf
    DEPENDS ${WHAT}.tex
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND texi2pdf ${WHAT}.tex
  )

  add_custom_target(weave-${WHAT} ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${WHAT}.pdf)
  set_target_properties(weave-${WHAT} PROPERTIES EXCLUDE_FROM_ALL TRUE)
endforeach()
