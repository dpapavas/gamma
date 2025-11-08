#ifndef COMMON_H
#define COMMON_H

#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>

#include <pthread.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#define DEBUG

struct settings {
    const char *args;
    const char *program;

    double default_color[4];
    double mouse_sensitivity;
};

struct object {
    const char *name;

    GLuint vbo, ebo;
    GLsizei counts[3];
    GLfloat bounds[6];

    struct object *next;
};

#define DEFAULT_VIEWPORT_NAME ""
#define DEFAULT_VIEWPORT_ANGLE 90.0f
#define DEFAULT_VIEWPORT_ZOOM 0.7f

enum projection {ORTHOGRAPHIC, PERSPECTIVE};

struct viewport {
    size_t index;
    const char *name;
    bool stale;

    int left, right, bottom, top;

    enum projection projection;
    GLfloat near, far;
    GLfloat angle;
    GLfloat zoom, translation[3], rotation[16], matrix[16];

    GLuint vao;

    struct object *object;
    struct viewport *next;
};

struct window {
    const char *name;

    pthread_mutex_t mutex;

    GLFWwindow *window;
    GLuint vao, vbo;

    struct viewport *viewports, *focus;
    struct window *next;
};

extern struct window *windows;
extern struct settings settings;
extern struct object *objects;

void refresh_object(
    const char *name,
    size_t n, float *vertices,
    size_t m, unsigned int *triangles,
    size_t l, unsigned int *edges);

struct window *find_window(const char *name);
void resize_window(struct window *w, int width, int height);
bool refresh_windows(void);

enum direction {HORIZONTALLY, VERTICALLY};
struct viewport *split_viewport(
    struct viewport *v, enum direction direction, unsigned int parts);
void pan_viewport(struct viewport *v, float x, float y);
void translate_viewport(struct viewport *v, float x, float y, float z);
void rotate_viewport(struct viewport *v, float alpha, float beta, float gamma);
void zoom_viewport(struct viewport *v, float zeta);
void refresh_viewport(struct viewport *v);

GLuint compile_shader(GLenum type, const char *source);
GLuint create_program(GLuint vertex, GLuint fragment);

int read_commands(FILE *fp);

#endif
