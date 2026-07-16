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

void begin_print(void);
void end_print(void);
void print_output(const char *format, ...);
void print_error(const char *format, ...);

/* This macro ensures that the asserted expression does get executed,
 * no matter the build type, along with any side-effects. */

#ifdef NDEBUG
#define safely_assert(...) (void)(__VA_ARGS__)
#else
#define safely_assert(...) assert(__VA_ARGS__)
#endif

#define assert_not_reached() {                  \
        assert(false);                          \
        __builtin_unreachable();                \
    }

// Document: program

// The following definitions are used through the sources and are
// thefore bunched together in this single header.

// ## Settings Definitions

// Ref: Settings.

struct settings {
    char *args;
    char *program;

    bool quiet, batch;
    bool present_on_reload, recenter_on_reload, resize_on_split, print_frames;
    bool save_history;
    size_t history_size;

    double default_vertex_color[4], vertex_point_size, edge_line_width;
    double default_view, default_zoom;
    double default_rotation[3], default_translation[3];
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
    size_t n, double *vertices,
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

// ## Windows Definitions

// Ref: Windows.

struct window {
    const char *name;

    int saved_geometry[4];

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

// ## Viewports Definitions

// Ref: Viewports.

enum projection {ORTHOGRAPHIC, PERSPECTIVE};

struct viewport {
    const char *name;

    struct {
        bool projection: 1;
        bool annotation: 1;
    } stale;

    struct {
        bool maximized:1;
        bool vertices:1;
        bool edges:1;
        bool faces:1;
    } flags;

    int left, right, bottom, top;

    enum projection projection;
    GLfloat near, far;
    GLfloat angle;
    GLfloat zoom, parameter, translation[3], rotation[16], matrix[16];

    GLuint vao;

    struct text *annotation;
    struct object *object;
    struct viewport *next;
};

enum direction {HORIZONTALLY, VERTICALLY};
void track_viewport(struct viewport *v, float x, float y, float z);
void translate_viewport(struct viewport *v, float x, float y, float z);
void rotate_viewport(struct viewport *v, float alpha, float beta, float gamma);
void zoom_viewport(struct viewport *v, float zeta);
void refresh_viewport(struct viewport *v, struct window *w);

// ## Running Definitions

// Ref: Running the Inferior.

void run_inferior(const char *s);
int kill_inferior();

// Document: none

int read_commands(FILE *fp);

#endif
