find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(Freetype REQUIRED)
find_package(Fontconfig REQUIRED)
find_package(GL2PS)

find_path(Readline_INCLUDE_DIR readline/readline.h)
find_library(Readline_LIBRARIES readline)

add_executable(embed embed.c)

set(SHADER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/glsl/*.c")
set(SHADER_STRINGS "${CMAKE_CURRENT_BINARY_DIR}/shaders.h")

add_custom_command(
  OUTPUT "${SHADER_STRINGS}"
  DEPENDS embed "${SHADER_SOURCES}"
  COMMAND embed "${SHADER_STRINGS}" "${SHADER_SOURCES}"
)

add_executable(gammadb glad.c commands.c main.c windows.c viewports.c "${SHADER_STRINGS}")
target_compile_definitions(
  gammadb PRIVATE
  "DEFAULT_PROGRAM=\"$<IF:$<CONFIG:Release>,$<TARGET_FILE_NAME:gamma>,$<TARGET_FILE:gamma>>\"")
target_compile_definitions(gammadb PRIVATE "SHADER_STRINGS=\"${SHADER_STRINGS}\"")
target_link_libraries(gammadb OpenGL::GL Freetype::Freetype Fontconfig::Fontconfig glfw m readline ${GL2PS_LIBRARY})
target_include_directories(gammadb PRIVATE . ${GL2PS_INCLUDE_DIR})

install(TARGETS gammadb)

add_subdirectory(test)
