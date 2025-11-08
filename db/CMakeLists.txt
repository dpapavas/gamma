find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)

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
target_compile_definitions(gammadb PRIVATE "SHADER_STRINGS=\"${SHADER_STRINGS}\"")
target_link_libraries(gammadb OpenGL::GL glfw m readline)
target_include_directories(gammadb PRIVATE .)

install(TARGETS gammadb)

add_subdirectory(test)
