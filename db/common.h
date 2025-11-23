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

// ---

// The following definitions are used through the sources and are
// thefore bunched together in this single header.

// ## Settings Definitions

// Ref: Settings.

struct settings {
    const char *args;
    const char *program;

    double default_color[4];
    double mouse_sensitivity;
};

extern struct settings settings;

// ## Key Bindings Definitions

// Ref: Key Bindings.

struct key_binding {
    int key, mods;
    const char *command;
};

struct key_name {
    int i;
    char *name;
};

struct key_binding *find_key_binding(int mods, int key);

// ## Objects Definitions

// Ref: Refreshing Object Geometry.

struct object {
    const char *name;

    GLuint vbo, ebo;
    GLsizei counts[3];
    GLfloat bounds[6];

    struct object *next;
};

extern struct object *objects;

void refresh_object(
    const char *name,
    size_t n, float *vertices,
    size_t m, unsigned int *triangles,
    size_t l, unsigned int *edges);

// ## Viewports Definitions

// Ref: Viewports.

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

enum direction {HORIZONTALLY, VERTICALLY};
struct viewport *split_viewport(
    struct viewport *v, enum direction direction, unsigned int parts);
void pan_viewport(struct viewport *v, float x, float y);
void translate_viewport(struct viewport *v, float x, float y, float z);
void rotate_viewport(struct viewport *v, float alpha, float beta, float gamma);
void zoom_viewport(struct viewport *v, float zeta);
void refresh_viewport(struct viewport *v);

// ## Windows Definitions

// Ref: Windows.

struct window {
    const char *name;

    pthread_mutex_t mutex;

    GLFWwindow *window;
    GLuint vao, vbo;

    struct viewport *viewports, *focus;
    struct window *next;
};

extern struct window *windows;

struct window *find_window(const char *name);
void resize_window(struct window *w, int width, int height);
bool refresh_windows(void);

// ---

int read_commands(FILE *fp);

#endif
