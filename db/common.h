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

#define print_error(...) fprintf(stderr, __VA_ARGS__)
#define print_output(...)                       \
    do {                                        \
        if (!settings.quiet) {                  \
            printf(__VA_ARGS__);                \
        }                                       \
    } while (false)

// Document: program

// The following definitions are used through the sources and are
// thefore bunched together in this single header.

// ## Settings Definitions

// Ref: Settings.

struct settings {
    char *args;
    char *program;

    bool quiet;
    bool present_on_reload;

    double default_color[4], edge_color[4];
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

// ## Text Definitions

// Ref: Text Rendering.

// The dimensions of the texture tightly fit the rendered glyphs, but
// the true origin of the text, the "pen position" at the start of the
// initial glyph, is generally not at the lower left corner of the
// texture, because:

//   1. Some glyphs in the text (e.g. gs or ys) may have so-called
//   descenders that descend below the baseline.  The largest such
//   descent is recorded in the `descent` field below and is the
//   vertical offset we need to apply to the texture when rendering
//   it.

//   2. Parts of the first glyph may extend to the left of its origin
//   (i.e. the origin of its EM box), or the glyph may not extend to
//   the origin at all.  This horizontal offset is recorded in the
//   `offset` field below.

struct text {
    GLuint texture;
    int width, height, descent, offset;
};

struct text *make_text(void);
void printf_text(struct text *t, size_t size, const char *fmt, ...);

// ## Viewports Definitions

// Ref: Viewports.

#define DEFAULT_VIEWPORT_NAME ""
#define DEFAULT_VIEWPORT_ANGLE 50.0f
#define DEFAULT_VIEWPORT_ZOOM 0.7f

enum projection {ORTHOGRAPHIC, PERSPECTIVE};

struct viewport {
    size_t index;
    const char *name;

    struct {
        bool projection: 1;
        bool annotation: 1;
    } stale;

    int left, right, bottom, top;

    enum projection projection;
    GLfloat near, far;
    GLfloat angle;
    GLfloat zoom, translation[3], rotation[16], matrix[16];

    GLuint vao;

    struct text *annotation;
    struct object *object;
    struct viewport *next;
};

enum direction {HORIZONTALLY, VERTICALLY};
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
void print_window(struct window *w, GLint format, FILE *fp);
bool refresh_windows(void);

// Document: none

int read_commands(FILE *fp);

#endif
