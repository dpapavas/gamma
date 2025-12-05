set(PATCH_SOURCES src/operation.h src/operation.cpp src/evaluation.cpp src/basic_operations.h
  src/compose_tag.h src/compose_tag.cpp src/rewrites.cpp

  src/boxed_operations.h src/frontend.h src/frontend.cpp src/scheme_frontend.cpp

  src/test/fixtures.h src/test/rewrites.cpp

  db/common.h db/main.c db/windows.c db/viewports.c db/commands.c)

foreach(X ${PATCH_SOURCES})
  add_custom_command(
    OUTPUT ${PROJECT_BINARY_DIR}/${X}.texi
    DEPENDS ${PROJECT_SOURCE_DIR}/${X}
            ${CMAKE_CURRENT_SOURCE_DIR}/weave.texi
            ${CMAKE_CURRENT_SOURCE_DIR}/weave.awk
    COMMAND awk -f ${CMAKE_CURRENT_SOURCE_DIR}/weave.awk
                -v bindir=${PROJECT_BINARY_DIR}
                -v prefix=${PROJECT_BINARY_DIR}/${X}
                -- ${PROJECT_SOURCE_DIR}/${X} > ${PROJECT_BINARY_DIR}/${X}.texi
  )

  list(APPEND PATCHES ${PROJECT_BINARY_DIR}/${X}.texi)
endforeach()

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/weave.tex
  DEPENDS ${PATCHES}
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
  COMMAND texi2any --latex ${CMAKE_CURRENT_SOURCE_DIR}/weave.texi
                   -o ${CMAKE_CURRENT_BINARY_DIR}/weave.tex
)

add_custom_command(
  OUTPUT weave.pdf
  DEPENDS weave.tex
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND texi2pdf weave.tex
)

add_custom_target(weave ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/weave.pdf)
set_target_properties(weave PROPERTIES EXCLUDE_FROM_ALL TRUE)
