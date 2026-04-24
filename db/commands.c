#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#include <gl2ps.h>

// Document: program

// # Commands

// Here we deal with parsing and executing commands.  These can arrive
// from another process through an IPC channel, or from the terminal.
// But first we need to get out of the way some supporting routines
// and definitions.

// ## Growing Buffers

// The macros below implement exponentially growing buffers, with a
// base of $1.5$ (i.e. the buffer grows by 50% each time).

#define BUFFER_TYPE(T)  struct {T *p; size_t n_0, n;}

#define START_WITH(B, N_0)                              \
    {                                                   \
        B.n_0 = N_0;                                    \
        B.n = 0;                                        \
        B.p = realloc(B.p, B.n_0 * sizeof(B.p[0]));     \
    }

#define MAYBE_GROW_TO(B, N)                                     \
    {                                                           \
        size_t n_ = N;                                          \
        while (B.n_0 + B.n < n_) {                              \
            if (B.n > 0) {                                      \
                B.n += B.n / 2;                                 \
            } else {                                            \
                B.n = 64;                                       \
            }                                                   \
                                                                \
            B.p = realloc(B.p, (B.n_0 + B.n) * sizeof(B.p[0])); \
            assert(B.p);                                        \
        }                                                       \
    }

// ## Triangulating polygons

// The GL only operates on triangles, so we need a way to triangulate
// arbitrary polygons.  We use a simple implementation of ear clipping
// that should be sufficient (even efficient) for small polygons.

// In the course of triangulation, we'll need to determine whether a
// point is inside a triangle.

static bool point_in_triangle(
    const double *p, const double *a, const double *b, const double *c)
{
    // We accomplish that by first calculating $p$'s barycentric
    // coordinates $u$ and $v$.

    const double v_0[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    const double v_1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const double v_2[3] = {p[0] - a[0], p[1] - a[1], p[2] - a[2]};

    const double vv_00 = v_0[0] * v_0[0] + v_0[1] * v_0[1] + v_0[2] * v_0[2];
    const double vv_01 = v_0[0] * v_1[0] + v_0[1] * v_1[1] + v_0[2] * v_1[2];
    const double vv_02 = v_0[0] * v_2[0] + v_0[1] * v_2[1] + v_0[2] * v_2[2];
    const double vv_11 = v_1[0] * v_1[0] + v_1[1] * v_1[1] + v_1[2] * v_1[2];
    const double vv_12 = v_1[0] * v_2[0] + v_1[1] * v_2[1] + v_1[2] * v_2[2];

    const double d = (vv_00 * vv_11 - vv_01 * vv_01);

    // If the denominator `d` is zero, the triangle $\triangle abc$
    // under consideration is degenerate.  We return `true` to discard
    // it.

    if (d == 0.0f) {
        return true;
    }

    const double u = (vv_11 * vv_02 - vv_01 * vv_12) / d;
    const double v = (vv_00 * vv_12 - vv_01 * vv_02) / d;

    // "Point $p$ is inside triangle $abc$" is then equivalent to the
    // following:

    return (u > 0.0f) && (v > 0.0f) && (u + v < 1.0f);
}

// This is the triangulation routine.  It accepts a polygon of `n`
// vertices, indexed by `s` and outputs new indices for each triangle
// in the triangulation of the polygon in `t`.

static void triangulate(
    size_t n, unsigned int *s, double *vertices, unsigned int *t)
{
    assert(n >= 3);

    // In the general case we iterate the vertices looking for an ear
    // tip.  Vertex $b$ is an ear tip if the segment $ac$ formed by
    // connecting its neighbor vertices intersects the polygon only at
    // $a$ and $c$ and if it is convex, i.e the $\angle abc$ is less
    // than $\pi$.

    // To determine convexity, we need to know the surface normal for
    // the polygon.  We calculate it by Newell's method below,
    // skipping normalization.

    double w[3] = {};

    if (n > 3) {
        for (size_t i = 0; i < n; i++) {
            const unsigned int l = s[i];
            const unsigned int m = s[(i + 1) % n];

            const double *a = &vertices[7 * l];
            const double *b = &vertices[7 * m];

            w[0] += (a[1] - b[1]) * (a[2] + b[2]);
            w[1] += (a[2] - b[2]) * (a[0] + b[0]);
            w[2] += (a[0] - b[0]) * (a[1] + b[1]);
        }
    }

    // We then proceed by:

    for (size_t i = 0; i < n;) {
        //   1. First handling the trivial case of a triangle, which
        //   we just copy.

        if (n == 3) {
            memcpy(t, s, 3 * sizeof(unsigned int));
            return;
        }

        //   2. looking up the vertex indices and vertices,
        //   for the candidate tip and its neighbors,

        const unsigned int k = s[(i + n - 1) % n];
        const unsigned int l = s[i];
        const unsigned int m = s[(i + 1) % n];

        const double *a = &vertices[7 * k];
        const double *b = &vertices[7 * l];
        const double *c = &vertices[7 * m];

        //   3. calculating the vectors forming the angle $\angle
        //   abc$,

        const double u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        const double v[3] = {c[0] - b[0], c[1] - b[1], c[2] - b[2]};

        //   4. determining whether the angle is convex by comparing
        //   the directions of the cross product $u \times v$ and the
        //   polygon's normal $w$, using the scalar triple product $w
        //   \cdot (u \times v)$, skipping over the vertex if that is
        //   the case,

        const double uxv[3] = {
            u[1] * v[2] - u[2] * v[1],
            u[2] * v[0] - u[0] * v[2],
            u[0] * v[1] - u[1] * v[0]
        };

        //     We may need to be careful about floating point
        //     comparisons here, but no geometry has caused issues
        //     yet.

        if (uxv[0] * w[0] + uxv[1] * w[1] + uxv[2] * w[2] < 0.0f) {
            goto next;
        }

        //   5. skipping over vertices, which aren't principal
        //   vertices to begin with,

        for (size_t j = 0; j < n - 3 ; j++) {
            if (point_in_triangle(
                    &vertices[7 * s[(i + j + n + 2) % n]], a, b, c)) {
                goto next;
            }
        }

        //   6. at which point we have an ear tip.  We output the
        //   corresponding triangle,

        t[0] = k;
        t[1] = l;
        t[2] = m;

        t += 3;

        //   7. then clip the tip vertex by removing it from the list
        //   of vertices, after which we can finally,

        n--;
        for (size_t j = i; j < n ; s[j] = s[j + 1], j++);

        //   8. start anew with the reduced polygon, unless it's
        //   already down to a triangle, which we output immediately.

        i = 0;
        continue;

      next:
        i++;
    }

    assert(false);
}

// ## Extracting Edges

// We want to be able to draw the edges of the object as it is
// received, not those of its triangulation.  Here we extract them as
// line segmenets, by going over the vertices in pairs.  The order of
// the vertices making up each edge is immaterial, so we're free to
// output them in order of increasing index.  This facilitates sorting
// and deduplication of the edges later on.

static void extract_edges(size_t n, const unsigned int *s, unsigned int *t)
{

    for (size_t i = 0; i < n; i++) {
        const unsigned int a = s[i], b = s[(i + 1) % n];

        t[2 * i + (a > b)] = a;
        t[2 * i + (a <= b)] = b;
    }
}

// This is the comparison function for edge sorting.

static int compare_edges(const void *a, const void *b)
{
    const unsigned int *p = a, *q = b;

    if (p[0] == q[0]) {
        return p[1] - q[1];
    }

    return p[0] - q[0];
}

// ## Settings

// Settings are constants, that affect various aspects of operation,
// such as drawing of objects, execution of the "inferior", etc.  They
// can be changed with the `set` command and their current value can
// be displayed with the `show` command.

// Below, we define the structure for global settings and set default
// values.

struct settings settings = {
    .present_on_reload = true,
    .recenter_on_reload = true,
    .default_zoom = 0.7f,
    .default_view = 50.0f,
    .default_vertex_color = {0.78, 0.78, 0.78, 1},
    .vertex_point_size = 3.0f,
    .edge_line_width = 2.0f,
    .mouse_sensitivity = 0.01
};

// ## Definitions

// Definitions are arbitrary key-value pairs that are supplied to the
// inferior as `-Dkey=value` options.  They can be set with the
// `define` command and cleared with the `undefine` command.

struct definition {
    char *name, *value;
};

static BUFFER_TYPE(struct definition) definitions;

// ## Key Bindings

// Key bindings map keystrokes in any of the windows to commands,
// which are executed as if entered in the terminal.

// The tables included below list the names that can be used to bind
// function keys, i.e. keys that don't simply enter a character and
// specify modifiers.

#include "keys.h"

static struct {
    BUFFER_TYPE(struct key_binding) buffer;
    size_t count;
} key_bindings;

struct key_binding *find_key_binding(int mods, int key)
{
    for (size_t i = 0; i < key_bindings.count; i++) {
        struct key_binding *p = key_bindings.buffer.p + i;
        if (p->mods == mods && p->key == key) {
            return p;
        }
    }

    return nullptr;
}

// ## Parsing Commands

// We read commands from a `FILE *`, although it's not a real file in
// most cases.

// We use the following function to skip whitespace, except newline
// characters^[With the exception of some commands that can be spread
// out on multiple lines, for which we skip newlines as well.].  It
// also skips comments, which start with `#` and continue to the end
// of the line and returns the newlines it consumed as whitespace.

static int skip(FILE *fp)
{
    bool p = false;
    for (char c = fgetc(fp); c != EOF; c = fgetc(fp)) {
        if (c == '\n') {
            return 1;
        } else if (c == '#') {
            p = true;
        } else if (!(p || isspace(c))) {
            ungetc(c, fp);
            break;
        }
    }

    return 0;
}

// This function scans tokens that are not required to be on the same
// line.  This is generally the first token, i.e. the command, or
// required tokens of multi-line commands, such as `load`. We skip any
// whitespace (possibly spanning multiple lines) before the token.

static int do_scan(FILE *fp, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    while(skip(fp));
    const int n = vfscanf(fp, fmt, ap);
    va_end(ap);

    return n;
}

// When reading the last argument of a command, or optional arguments,
// we want to look for them in the current line.  We therefore skip
// whitespace and if we cross into the next line, we return instead of
// attempting to read a token, as we would have read the next
// command's token.

static int try_scan(FILE *fp, const char *fmt, ...)
{
    va_list ap;

    // In that case we also put back the newline character.  This
    // allows us to check that there weren't any extraneous, or
    // otherwise invalid arguments at the end (see below).

    if (skip(fp) == 1) {
        ungetc('\n', fp);
        return 0;
    }

    va_start(ap, fmt);
    const int n = vfscanf(fp, fmt, ap);
    va_end(ap);

    return n;
}

// When we're done parsing arguments, we should normally be at the end
// of the line (potentially after white space, or a comment).  If the
// call to `skip` below returns zero and we're not at EOF, then we're
// at the beginning of invalid input that hasn't been consumed yet, so
// we compain about it.

// This may happen when there are extraneous arguments or with
// commands that accept optional arguments, where the argument is not
// of the correct type, so that it wasn't scanned.

#define PARSING_FINISHED                                \
    do {                                                \
        if (skip(fp) < 1 && !feof(fp)) {                \
            print_error("error: invalid syntax");       \
                                                        \
            if (try_scan(fp, "%63[^\n]", s) == 1) {     \
                print_error(", near \"%s\"\n", s);      \
            } else {                                    \
                print_error("\n");                      \
            }                                           \
                                                        \
            goto error;                                 \
        }                                               \
    } while(false)

// Many commands require a current window to have been selected.  We
// handle error checking with a macro.

#define NEEDS_WINDOW                                                    \
    do {                                                                \
        if (!w) {                                                       \
            print_error("error: no window selected\n");                 \
            goto error;                                                 \
        }                                                               \
    } while(false)

int read_commands(FILE *fp)
{
    // We keep reading commands as long as there are non-whitespace
    // characters to read, then branch accordingly.

    char s[64];
    for (int n = 0; ; n++) {
        if (do_scan(fp, "%63s", s) < 1) {
            return n;
        }

        static struct window *w;

        if (!w) {
            w = windows;
        }

        if (!strcmp(s, "quit") || !strcmp(s, "exit")) {
            PARSING_FINISHED;
            exit(EXIT_SUCCESS);
        }

        // ## Binding Commands

        //   `bind key command` := Bind the action of pressing a key
        //   inside one of the windows to a command.  The key can be
        //   any printable character, or the name of a function key,
        //   potentially prefixed by one or more of `C-`, `M-`, `S-`,
        //   or `s-` to specify that the Control, Meta (Alt), Shift,
        //   or Super modifiers should by present.  Use the completion
        //   feature when entering this command in the terminal for a
        //   list of function key names.

        //   The command is entered exactly as it would be entered in
        //   the terminal.

        //   A list of established bindings can be displayed with the
        //   `info bindings` command.

        //   `unbind key` := Delete the binding previously established
        //   for the key.  A list of established bindings can be
        //   displayed with the `info bindings` command.

        else if (!strcmp(s, "bind") || !strcmp(s, "unbind")) {
            // First we make a note of whether we're binding or
            // unbinding, then scan the key, which consists of:

            const bool q = (s[0] == 'b');

            if (try_scan(fp, "%63s", s) != 1) {
                print_error("error: no key specified\n");
                goto error;
            }

            int k = 0, m = 0;

            for (char *c = s; c;) {
                size_t i;

                //   1. one or more potential modifiers, followed by
                //   the key, which can be

                if (c[1] == '-') {
                    for (i = 0;
                         i < sizeof(modifier_keys) / sizeof(modifier_keys[0]);
                         i++) {
                        if (!strncmp(modifier_keys[i].name, c, 2)) {
                            m |= modifier_keys[i].i;
                            break;
                        }
                    }

                    if (i == sizeof(modifier_keys) / sizeof(modifier_keys[0])) {
                        print_error(
                            "error: invalid modifier '%c' specified\n", c[0]);
                        goto error;
                    }

                    c += 2;
                    continue;
                }

                //   2. an ordinary key, i.e. one that corresponds to
                //   a character, which GLFW conveniently represents
                //   with their ASCII codes^[The capitalized version
                //   is used for characters of the alphabet.], or

                if (c[1] == '\0') {
                    if (!isgraph(c[0])) {
                        print_error("error: invalid key specified\n");
                        goto error;
                    }

                    if (islower(c[0])) {
                        k = toupper(c[0]);
                    } else {
                        k = c[0];
                        m |= GLFW_MOD_SHIFT;
                    }

                    break;
                }

                //   3. a function key, which can be referred to by
                //   its name, as listed in the table defined in ref:
                //   Key Bindings.

                for (i = 0;
                     i < sizeof(function_keys) / sizeof(function_keys[0]);
                     i++) {
                    if (!strcmp(function_keys[i].name, c)) {
                        k = function_keys[i].i;
                        break;
                    }
                }

                if (i == sizeof(function_keys) / sizeof(function_keys[0])) {
                    print_error("error: invalid key '%s' specified\n", c);
                    goto error;
                }

                break;
            }

            if (q) {
                // We're binding, so we need to scan the command to
                // bind to.  We skip any initial whitespace and scan
                // to the end of the line.

                char *t;

                if (try_scan(fp, " %m[^\n]", &t) != 1) {
                    print_error("error: no command specified\n");
                    goto error;
                }

                PARSING_FINISHED;

                struct key_binding *p;

                // To establish the binding, we either:

                if ((p = find_key_binding(m, k))) {
                    //   1. look for a pre-existing binding to update,
                    //   or

                    free((char *)p->command);
                    p->command = t;
                } else if ((p = find_key_binding(0, 0))) {
                    //   2. look for a previously unbound binding to
                    //   reuse, or

                    p->mods = m;
                    p->key = k;
                    p->command = t;
                } else {
                    //   3. add a new biding.

                    MAYBE_GROW_TO(key_bindings.buffer, ++key_bindings.count);

                    p = key_bindings.buffer.p + key_bindings.count - 1;
                    p->mods = m;
                    p->key = k;
                    p->command = t;
                }
            } else {
                // Here we're unbindind; we just need to look up the
                // binding and mark it as deleted.

                PARSING_FINISHED;

                struct key_binding *p = find_key_binding(m, k);

                if (!p) {
                    print_error("error: no such binding\n");
                    goto error;
                }

                p->mods = p->key = 0;
                free((char *)p->command);
            }
        }

        // ### Window Commands

        //   `window name` := Create or select a window with the given
        //   name.  The new window is initially hidden, until it
        //   receives geometry for one of its viewports, or unitl it's
        //   explicitly shown.

        else if (!strcmp(s, "window")) {

            // We read in the name and look through the window list.

            if (try_scan(fp, "%63s", s) != 1) {
                print_error("error: no window name specified\n");
                goto error;
            }

            PARSING_FINISHED;

            w = find_window(s);
        }

        //   `hide` := Hide the currently selected window.

        else if (!strcmp(s, "hide")) {
            PARSING_FINISHED;
            NEEDS_WINDOW;

            // If the window is full screen, GLFW will ignore the hide
            // request.  We need to restore it first.

            if (w->saved_geometry[2] != 0) {
                resize_window(w, 0, 0);
            }

            glfwHideWindow(w->window);
        }

        //   `present` := Present, that is unhide if hidden and focus
        //   the window.

        else if (!strcmp(s, "present")) {
            PARSING_FINISHED;
            NEEDS_WINDOW;

            glfwShowWindow(w->window);
            glfwFocusWindow(w->window);
        }

        //   `resize width height`, `resize fullscreen` := Resize the
        //   currently selected window.  If `fullscreen` is specified,
        //   the window is made full screen, if not already so.  If
        //   already full screen, its old size and position is
        //   restored.  Viewport sizes are adjusted accordingly.

        else if (!strcmp(s, "resize")) {
            int a, b = -1;

            if (try_scan(fp, "%63[a-z]", &s) == 1) {
                if (!strcmp(s, "fullscreen")) {
                    a = b = 0;
                }
            } else {
                try_scan(fp, "%d", &a);
                try_scan(fp, "%d", &b);
            }

            if (b == -1) {
                print_error("error: new size not specified\n");
                goto error;
            }

            PARSING_FINISHED;
            NEEDS_WINDOW;

            resize_window(w, a, b);
        }

        //   `focus index` := Focus the viewport with the given index.
        //   Further operations of viewport-related commands will
        //   affect this viewport, until another is focused, either by
        //   a command or with the mouse.

        else if (!strcmp(s, "focus")) {
            size_t i;

            if (try_scan(fp, "%zu", &i) != 1) {
                print_error("error: no viewport index specified\n");
                goto error;
            }

            PARSING_FINISHED;
            NEEDS_WINDOW;

            struct viewport *v = w->viewports;
            while (v && --i > 0) {
                v = v->next;
            }

            if (!v) {
                print_error("error: no such viewport\n");
                goto error;
            }

            w->focus = v;
            glfwPostEmptyEvent();
        }

        //   `print file` := Print the contents of the viewports
        //   in the current window to a file.  The file format is
        //   chosen based on the file extension, which can be either
        //   `ps`, `eps`, `pdf`, or `svg`.

        else if (!strcmp(s, "print")) {
            char *c;

            // We proceed by:

            //   1. scanning the file name^[We go through the stack
            //   allocation and copying process below to make sure
            //   that there are no memory leaks, no matter where we
            //   exit.],

            if (try_scan(fp, "%ms", &c) != 1) {
                print_error("error: no output file name specified\n");
                goto error;
            }

            char t[strlen(c) + 1];
            strcpy(t, c);
            free(c);

            //   2. choosing the output format and

            GLint i;

            {
                const char *c = strrchr(t, '.');

                if (!strcasecmp(c, ".ps")) {
                    i = GL2PS_PS;
                } else if (!strcasecmp(c, ".eps")) {
                    i = GL2PS_EPS;
                } else if (!strcasecmp(c, ".pdf")) {
                    i = GL2PS_PDF;
                } else if (!strcasecmp(c, ".svg")) {
                    i = GL2PS_SVG;
                } else {
                    print_error("error: output file has unknown extension\n");
                    goto error;
                }
            }

            PARSING_FINISHED;
            NEEDS_WINDOW;

            //   3. writing the document.

            FILE *fp = fopen(t, "wb");

            if (!fp) {
                print_error(
                    "error: could not open output file (%s)\n",
                    strerror(errno));
                goto error;
            }

            print_window(w, i, fp);
            fclose(fp);
        }

        // ### Viewport Commands

        //   `split [direction] [parts] [splits]` := Split the focused
        //   viewport horizontally or vertically in equal parts.

        //   The direction may be either `horizontally` or `vertically`.
        //   The horizontal direction is assumed if none is explictly
        //   specified.

        //   If the number of parts is not specified the viewport is
        //   split along its middle into two equal parts.

        //   If a number of splits is specified, no more than the
        //   specified number of splits will be carried out.  For
        //   instance if `parts` is 3 and `splits` is 1, the viewport
        //   will be split in two, with 1/3 of the width (or height)
        //   allocated to one viewport and 2/3 to the other.

        else if (!strcmp(s, "split")) {
            enum direction dir = HORIZONTALLY;
            unsigned int q = 2;
            size_t n = 0;

            if (try_scan(fp, "%63[a-z]", s) == 1) {
                if (!strcmp(s, "horizontally")) {
                    dir = HORIZONTALLY;
                } else if (!strcmp(s, "vertically")) {
                    dir = VERTICALLY;
                } else {
                    print_error("error: invalid split direction specified\n");
                    goto error;
                }

                size_t m;
                if (try_scan(fp, "%u", &q) == 1
                    && try_scan(fp, "%zu", &m) == 1
                    && m < q) {
                    n = q - m - 1;
                }
            }

            PARSING_FINISHED;
            NEEDS_WINDOW;

            struct viewport *v = w->focus;

            // If enabled, we resize the whole window, so that the
            // split viewport retains its current size.

            if (settings.resize_on_split) {
                int a, b;

                glfwGetFramebufferSize(w->window, &a, &b);

                if (dir == HORIZONTALLY) {
                    resize_window(w, a + (q - 1) * (v->right - v->left), b);
                } else {
                    resize_window(w, a, b + (q - 1) * (v->top - v->bottom));
                }
            }

            // We then perform the specified number of splits,
            // creating and initializing new viewports accordingly.

            for (size_t i = q; i > n + 1; i--) {
                struct viewport *u =
                    (struct viewport *)malloc(sizeof(struct viewport));

                *u = *v;
                u->annotation = nullptr;
                u->object = nullptr;
                u->vao = 0;

                switch (dir) {
                case HORIZONTALLY:
                {
                    const int m = v->left + (v->right - v->left) / i;
                    u->left = m;
                    v->right = m;
                }

                break;

                case VERTICALLY:
                {
                    const int m = v->bottom + (v->top - v->bottom) / i;
                    v->top = m;
                    u->bottom = m;
                }

                break;
                }

                v->stale.projection = true;
                u->stale.projection = true;

                u->flags.maximized = false;

                assert(w->viewports);

                // We append new viewports at the end, so as not to
                // upset the index numbers (and implicit names) of
                // current viewports.

                size_t j = 1;
                for (v = w->viewports; v->next; v = v->next) {
                    j++;
                }

                u->name = (const char *)malloc(4);
                snprintf((char *)u->name, 3, "%zu", j + 1);
                u->stale.annotation = true;

                v->next = u;
                u->next = nullptr;
                v = u;
            }

            glfwPostEmptyEvent();
        }

        //   `target name` := Set or change the target of the focused
        //   viewport.  After this all loaded geometry with the same
        //   name as the the one specified, will be displayed in the
        //   viewport.

        else if (!strcmp(s, "target")) {
            if (try_scan(fp, "%63s", s) != 1) {
                s[0] = '\0';
            }

            PARSING_FINISHED;
            NEEDS_WINDOW;

            free((char *)w->focus->name);
            w->focus->name = strdup(s);
            w->focus->stale.annotation = true;

            glfwPostEmptyEvent();
        }

        //   `rotate [alpha] [beta] [gamma]` := Rotate the focused
        //   viewport by the given euler angles, in degrees.  If no
        //   angles are given, reset the orientation.  If the first
        //   angle is given, but the second or third angles are not
        //   specified, they are assumed to be zero.

        //   The current rotation, in Euler angles, can be shown with
        //   the `info viewports` command.

        else if (!strcmp(s, "rotate")) {
            float v[3] = {};
            size_t i;

            for (i = 0; i < 3 && try_scan(fp, "%f", &v[i]) == 1; i++);

            PARSING_FINISHED;
            NEEDS_WINDOW;

            if (i == 0) {
                rotate_viewport(w->focus, NAN, NAN, NAN);
            } else {
                rotate_viewport(
                    w->focus,
                    v[0] / 180.0f * M_PI,
                    v[1] / 180.0f * M_PI,
                    v[2] / 180.0f * M_PI);
            }

            glfwPostEmptyEvent();
        }

        //   `translate x [y] [z]` := Translate the focused viewport
        //   by the given displacements.  If no displacements are
        //   given, reset the translation.  If the first displacement
        //   is given, but the second, or third displacements are not
        //   specified, they are assumed to be zero.

        //   The current translation can be shown with the `info
        //   viewports` command.

        else if (!strcmp(s, "translate")) {
            float v[3] = {};
            size_t i;

            for (i = 0; i < 3 && try_scan(fp, "%f", &v[i]) == 1; i++);

            PARSING_FINISHED;
            NEEDS_WINDOW;

            if (i == 0) {
                translate_viewport(w->focus, NAN, NAN, NAN);
            } else {
                translate_viewport(w->focus, v[0], v[1], v[2]);
            }

            glfwPostEmptyEvent();
        }

        //   `track x [y]` := Track the focused viewport by the given
        //   displacements perpendicularly and along the camera
        //   viewing direction.  If no displacements are given, reset
        //   the viewport's translation.  If less than three
        //   displacements are specified, the rest are assumed to be
        //   zero.

        else if (!strcmp(s, "track")) {
            float v[3] = {};
            size_t i;

            for (i = 0; i < 3 && try_scan(fp, "%f", &v[i]) == 1; i++);

            PARSING_FINISHED;
            NEEDS_WINDOW;

            if (i == 0) {
                translate_viewport(w->focus, NAN, NAN, NAN);
            } else {
                track_viewport(w->focus, v[0], v[1], v[2]);
            }

            glfwPostEmptyEvent();
        }

        //   `zoom [incr]` := Adjust the focused viewport's zoom by
        //   the given increment.  If no increment is specified, reset
        //   the zoom.

        else if (!strcmp(s, "zoom")) {
            float zeta = NAN;

            try_scan(fp, "%f", &zeta);

            PARSING_FINISHED;
            NEEDS_WINDOW;

            zoom_viewport(w->focus, zeta);

            glfwPostEmptyEvent();
        }

        //   `view proj` := Change the projection of the focused
        //   viewport.  The argument can be either one of
        //   `orthographic` and `perspective`, or a field of view angle
        //   in degrees.  In the first case orthographic projection is
        //   selected.  In the latter two cases, perspecitve
        //   projection is selected, either retaining the current, or
        //   updating the field of view angle.

        else if (!strcmp(s, "view")) {
            float f = 0;
            enum projection mode;
            bool p = false;

            if (try_scan(fp, "%f", &f) == 1 && f > 0.0f) {
                mode = PERSPECTIVE;
            } else if (try_scan(fp, "%63[a-z]", s) == 1) {
                if (!strcmp(s, "orthographic")) {
                    mode = ORTHOGRAPHIC;
                } else if (!strcmp(s, "perspective")) {
                    mode = PERSPECTIVE;
                } else if (!strcmp(s, "toggle")) {
                    p = true;
                } else {
                    print_error("error: invalid projection specified\n");
                    goto error;
                }
            } else {
                print_error("error: no projection specified\n");
                goto error;
            }

            PARSING_FINISHED;
            NEEDS_WINDOW;

            struct viewport *v = w->focus;

            v->stale.projection = true;

            if (p) {
                v->projection = (
                    v->projection == ORTHOGRAPHIC ? PERSPECTIVE : ORTHOGRAPHIC);
            } else {
                v->projection = mode;
            }

            if (f > 0.0f) {
                v->angle = f / 2.0f / 180.0f * M_PI;
            }

            glfwPostEmptyEvent();
        }

        // ### Loading Object Geometry

        //   `load [name] [< file]` := Load geometry for an object.
        //   The geometry can either follow the command or be loaded
        //   from a local file.

        //   If no name is specified the dafault target is loaded.

        else if (!strcmp(s, "load")) {

            FILE *old_fp = nullptr;

            {
                s[0] = '\0';

                bool p = true;

                while (true) {
                    // Look for the "redirection" character `<`.  If
                    // one is found, no name has been specified, so we
                    // leave `s` at its default value.

                    char c;
                    if (try_scan(fp, "%1[<]", &c) == 1) {
                        char *t;

                        // Scan the file name, open it swap it with
                        // the file we're currently reading from.

                        if (try_scan(fp, "%ms", &t) != 1) {
                            print_error("error: no file name specified\n");
                            goto error;
                        }

                        PARSING_FINISHED;

                        old_fp = fp;
                        fp = fopen(t, "r");
                        free(t);

                        if (!fp) {
                            print_error("Could not open file (%s)\n", strerror(errno));
                            goto done;
                        }

                    } else if (p) {
                        // If we haven't already scanned the name, do
                        // so now and loop to read a potential input
                        // file.

                        try_scan(fp, "%63s", s);
                        p = false;

                        continue;
                    }

                    break;
                }
            }

            // Here, we read geometry in the OFF format.  We start
            // with an optional header.

            {
                char t[4] = "OFF";

                if (do_scan(fp, "%4[STCN4nOF]", t) < 0
                    || (strcmp(t, "OFF") && strcmp(t, "COFF"))) {
                    print_error("error: found unsupported or invalid data\n");
                    goto error;
                }

                // Next come three integer counts of vertices, faces
                // and edges respectively.

                uint32_t a = 0, b = 0, c = 0;

                if (do_scan(fp, "%u", &a) != 1
                    || do_scan(fp, "%u", &b) != 1
                    || do_scan(fp, "%u", &c) != 1) {
                    print_error(
                        "could not read vertex, face, or edge counts\n");

                    goto error;
                }

                // We allocate a suitable buffer and read in all
                // vertices.

                static BUFFER_TYPE(double) vertices;
                MAYBE_GROW_TO(vertices, 7 * a);

                for (size_t i = 0; i < a; i++) {
                    double * const p = &vertices.p[7 * i];

                    // Each vertex is made up of:

                    //   1. 3 spatial coordinates, which are always required,

                    for (size_t j = 0; j < 3; j++) {
                        if (do_scan(fp, "%lf", &p[j]) != 1) {
                            print_error(
                                "could not read coordinate %zu of vertex %zu\n",
                                j, i);

                            goto error;
                        }
                    }

                    //   2. plus 4 required color coordinates if we're
                    //   reading the COFF variant.  If not, or if we
                    //   read in the special color with zero
                    //   components, we set the vertex to the default
                    //   color.

                    memset(&p[3], 0, 4 * sizeof(double));

                    if (t[0] == 'C') {
                        bool q = false;

                        for (size_t j = 3; j < 7; j++) {
                            if (do_scan(fp, "%lf", &p[j]) != 1) {
                                print_error(
                                    "could not read coordinate %zu of "
                                    "vertex %zu\n",
                                    j, i);

                                goto error;
                            }

                            q = q || p[j] > 1.0f;
                        }

                        if (q) {
                            for (size_t j = 3; j < 7; j++) {
                                p[j] /= 255.0;
                            }
                        }
                    }

                    if (p[3] == 0 && p[4] == 0 && p[5] == 0 && p[6] == 0) {
                        p[3] = (double)settings.default_vertex_color[0];
                        p[4] = (double)settings.default_vertex_color[1];
                        p[5] = (double)settings.default_vertex_color[2];
                        p[6] = (double)settings.default_vertex_color[3];
                    }
                }

                // Indices are more involved.  For one we don't know
                // how many we'll end up with as it depends, not only
                // on the number of faces `b`, but also on what kind
                // of polygon each face is.

                // We also need to triangulate the polygons before
                // passing them to the GL.  We also need to extract
                // edges, as we want to be able to draw those too.

                // We allocate growing buffers for the purpose.

                static BUFFER_TYPE(unsigned int) triangles;
                static BUFFER_TYPE(unsigned int) edges;

                // We keep track of the elements written to the
                // triangle and edge indices buffers with `n` and `m`
                // respectively.

                size_t n = 0, m = 0;

                // Now for each face we:

                for (size_t i = 0; i < b; i++) {
                    size_t l;

                    //   1. read the number of vertices into `l`,

                    if (do_scan(fp, "%zu", &l) != 1) {
                        print_error(
                            "could not read number of vertices for face %zu\n",
                            i);

                        goto error;
                    }

                    //   2. read the face polygon indices indices into `s`,

                    unsigned int s[l];
                    for (size_t j = 0; j < l; j++) {
                        if (do_scan(fp, "%u", &s[j]) != 1) {
                            print_error(
                                "could not read index %zu for face %zu\n",
                                j, i);

                            goto error;
                        }
                    }

                    //   3. write $l$ edges, corresponding to the $l$
                    //   vertices in our buffer^[We do it at this
                    //   point when the vertices are still the
                    //   original vertices loaded from file, not
                    //   potentially duplicated vertices created when
                    //   reading face colors below.  This allows
                    //   duplicate edges to be reliably detected and
                    //   discarded, since otherwise the same edge
                    //   could have shown up with different indices,
                    //   corresponding to vertices with the same
                    //   position but different colors.], potentially
                    //   growing it to make space if needed,

                    MAYBE_GROW_TO(edges, m += 2 * l);
                    extract_edges(l, s, &edges.p[m - 2 * l]);

                    //   4. read the face color if present.  This is a
                    //   bit complicated.

                    {
                        double v[4] = {};
                        size_t j;
                        bool p = false;

                        //   We assume a color is either 3 (RGB), or 4
                        //   (RGBA) values.  If less are available, we
                        //   ignore them.

                        for (j = 0; j < 4; j++) {
                            const int k = try_scan(fp, "%lf", &v[j]);

                            if (k <= 0) {
                                break;
                            }

                            //   Values can be given either as
                            //   integers in [0, 255] or floats in [0,
                            //   1].  If we find any number higher
                            //   than 1, we assume the former.

                            if (v[j] > 1.0f) {
                                p = true;
                            }
                        }

                        if (j >= 3) {
                            //   We normalize to [0, 1] if integers were
                            //   given.

                            if (p) {
                                for (size_t k = 0; k < j; k++) {
                                    v[k] /= 255.0f;
                                }
                            }

                            //   We also fill in a default alpha value
                            //   if necessary.

                            if (j == 3) {
                                v[3] = 1.0f;
                            }

                            //   The real problem is that the GL
                            //   doesn't know anything about "per-face
                            //   attributes".  All attributes are
                            //   per-vertex, so we need to duplicate
                            //   the vertices of the face and replace
                            //   their original vertex colors with the
                            //   constant face color.

                            MAYBE_GROW_TO(vertices, 7 * (a += l));

                            for (size_t k = 0; k < l; k++) {
                                const size_t alk = a - l + k;

                                memcpy(
                                    &vertices.p[7 * alk], &vertices.p[7 * s[k]],
                                    3 * sizeof(double));

                                memcpy(
                                    &vertices.p[7 * alk + 3], v,
                                    4 * sizeof(double));

                                s[k] = alk;
                            }
                        }
                    }

                    //   5. write the $l - 2$ triangles resulting from
                    //   the triangulation of the face polygon into
                    //   our buffer and finally

                    MAYBE_GROW_TO(triangles, n += 3 * (l - 2));
                    triangulate(l, s, vertices.p, &triangles.p[n - 3 * (l - 2)]);
                }

                //   For closed, manifold meshes, each edge will be
                //   shared by two faces and hence will show up twice
                //   in our buffer.  For non-manifold meshes there
                //   might be even more repetitions of the same edge.

                //   We don't want to draw the same edge multiple
                //   times, so we sort and deduplicate the edges
                //   before copying them to the GL buffers.

                qsort(edges.p, m / 2, 2 * sizeof(edges.p[0]), compare_edges);

                for (unsigned int *p = edges.p,
                         *q = edges.p + 2,
                         *end = edges.p + m; q < end; q += 2) {
                    if (!memcmp(p, q, 2 * sizeof(unsigned int))) {
                        m -= 2;
                        continue;
                    }

                    if ((p += 2) != q) {
                        assert((q - p) % 2 == 0 && q - p >= 2);
                        memcpy(p, q, 2 * sizeof(unsigned int));
                    }
                }

                //   6. load the resulting triangles and edges^[Unless
                //   there are none, which can happen if we try to
                //   load an OFF file with no polygons] to the
                //   specified object.

                if (n > 0) {
                    assert(a > 0 && m > 0);
                    refresh_object(s, a, vertices.p, n, triangles.p, m, edges.p);
                }
            }

            // We also remember to undo the file "redirection", if
            // there was one.

          done:
            if (old_fp) {
                if (fp) {
                    fclose(fp);
                }

                fp = old_fp;
            }
        }

        // ## The Run Command

        // In order to run the inferior, we assemble a command line
        // from the pieces defined below:

        //   1. the executable,

#define RUN                                                             \
        do {                                                            \
            static BUFFER_TYPE(char) buffer;                            \
            size_t n = 1;                                               \
                                                                        \
            if (!settings.program) {                                    \
                settings.program = strdup(DEFAULT_PROGRAM);             \
            }                                                           \
                                                                        \
            {                                                           \
                MAYBE_GROW_TO(buffer, (n += strlen(settings.program))); \
                stpcpy(buffer.p, settings.program);                     \
            }

            //   2. the IPC address option,

#define WITH_ADDRESS                                                    \
            {                                                           \
                const size_t n_0 = n - 1;                               \
                                                                        \
                MAYBE_GROW_TO(                                          \
                    buffer,                                             \
                    (n += snprintf(                                     \
                        nullptr, 0,                                     \
                        " --debugger-address=gammadb-%d", getpid())));  \
                sprintf(                                                \
                    buffer.p + n_0, " --debugger-address=gammadb-%d",   \
                    getpid());                                          \
            }

            //   3. parameter definitions,

#define WITH_DEFINITIONS                                                \
            for (size_t i = 0; i < definitions.n; i++) {                \
                const struct definition *q = definitions.p + i;         \
                                                                        \
                if (!q->name) {                                         \
                    continue;                                           \
                }                                                       \
                                                                        \
                const size_t n_0 = n - 1;                               \
                char *p;                                                \
                                                                        \
                if (q->value) {                                         \
                    MAYBE_GROW_TO(                                      \
                        buffer, (n += (strlen(q->name)                  \
                                       + strlen(q->value) + 4)));       \
                                                                        \
                    p = stpcpy(buffer.p + n_0, " -D");                  \
                    p = stpcpy(p, q->name);                             \
                    p = stpcpy(p, "=");                                 \
                    p = stpcpy(p, q->value);                            \
                } else {                                                \
                    MAYBE_GROW_TO(buffer, (n += (strlen(q->name) + 3))); \
                                                                        \
                    p = stpcpy(buffer.p + n_0, " -D");                  \
                    p = stpcpy(p, q->name);                             \
                }                                                       \
            }

            //   4. potentially more than one inspection outputs,

#define WITH_RUN_OUTPUTS                                                \
            if (w) {                                                    \
                for (struct viewport *v = w->viewports; v; v = v->next) { \
                    if (mode == SINGLE && v != w->focus) {              \
                        continue;                                       \
                    }                                                   \
                                                                        \
                    const size_t n_0 = n - 1;                           \
                    char *p;                                            \
                                                                        \
                    MAYBE_GROW_TO(buffer, (n += 2 * strlen(v->name) + 5)); \
                    p = stpcpy(buffer.p + n_0, " -o ");                 \
                    p = stpcpy(p, v->name);                             \
                    p = stpcpy(p, ":");                                 \
                    p = stpcpy(p, v->name);                             \
                }                                                       \
            }

            //   5. a single output, written to disk,

#define WITH_OUTPUT                                                     \
            if (w) {                                                    \
                const struct viewport *v = w->focus;                    \
                const size_t n_0 = n - 1;                               \
                char *p;                                                \
                                                                        \
                MAYBE_GROW_TO(buffer, (n += strlen(v->name) + strlen(s) + 5)); \
                p = stpcpy(buffer.p + n_0, " -o ");                     \
                p = stpcpy(p, s);                                       \
                p = stpcpy(p, ":");                                     \
                p = stpcpy(p, v->name);                                 \
            }

            //   6. any arguments specified by the user.

#define AND_ARGS                                                        \
            if (settings.args) {                                        \
                const size_t n_0 = n - 1;                               \
                char *p;                                                \
                                                                        \
                MAYBE_GROW_TO(buffer, (n += strlen(settings.args) + 1)); \
                p = stpcpy(buffer.p + n_0, " ");                        \
                p = stpcpy(p, settings.args);                           \
            }                                                           \
                                                                        \
            run_inferior(buffer.p);                                     \
        } while(false)

        //   `run [mode]` := Run the "inferior" process to update the
        //   contents of the viewports.  The program specified by the
        //   `program` setting is run in a shell, with the arguments
        //   specified in the `args` setting, augmented with arguments
        //   to enable outputs bound to viewports in the current
        //   window.

        //   The mode can be either `all`, in which case all viewports
        //   in the current window will be updated, or `single`, where
        //   only the focused viewport will be updated.

        else if (!strcmp(s, "run")) {
            enum {
                SINGLE, ALL
            } mode = ALL;

            // First, we try to scan the mode and assume `ALL` if we
            // can't.

            if (try_scan(fp, "%63[a-z]", s) == 1) {
                if (!strcmp(s, "single")) {
                    mode = SINGLE;
                } else if (!strcmp(s, "all")) {
                    mode = ALL;
                } else {
                    print_error("error: invalid mode specified\n");
                    goto error;
                }
            }

            PARSING_FINISHED;
            RUN WITH_ADDRESS WITH_DEFINITIONS WITH_RUN_OUTPUTS AND_ARGS;
        }

        //   `output file` := Run the "inferior" process to output the
        //   geometry of the focused window to the specified file.
        //   The file format is determined by the file extension,
        //   which must correspond to a supported output format.

        else if (!strcmp(s, "output")) {

            if (try_scan(fp, "%63s", s) != 1) {
                s[0] = '\0';
            }

            PARSING_FINISHED;

            RUN WITH_DEFINITIONS WITH_OUTPUT AND_ARGS;
        }

#undef RUN
#undef WITH_ADDRESS
#undef WITH_DEFINITIONS
#undef WITH_RUN_OUTPUTS
#undef WITH_OUTPUT
#undef END_RUN

        //   `kill` := Terminate an ongoing run.

        else if (!strcmp(s, "kill")) {
            PARSING_FINISHED;

            if (kill_inferior() == -1) {
                print_error(
                    "error: could not kill ongoing run (%s)\n",
                    strerror(errno));

                goto error;
            }
        }

        // ### The Information Command

        // The information displayed with the `info` command below is
        // often laid out in a tabular format, so we need a way to
        // print neatly aligned columns of data.  We handle this with
        // the following macros.

#define COLUMN(FMT, ...)                                                \
        do {                                                            \
            if (phase_ == 0) {                                          \
                if (i_ < n_ - 1) {                                      \
                    const int m_ = snprintf(nullptr, 0, FMT, __VA_ARGS__); \
                                                                        \
                    if (m_ > widths_[i_]) {                             \
                        widths_[i_] = m_;                               \
                    }                                                   \
                }                                                       \
            } else {                                                    \
                const int m_ = printf(FMT, __VA_ARGS__);                \
                if (i_ < n_ - 1) {                                      \
                    for (int j_ = 0;                                    \
                         j_ < widths_[i_] - m_ + 2;                     \
                         putchar(' '), j_++);                           \
                } else {                                                \
                    putchar('\n');                                      \
                }                                                       \
            }                                                           \
                                                                        \
            i_ = (i_ + 1) % n_;                                         \
        } while (false)

#define PRINT_TABLE(N, ...)                                             \
        do {                                                            \
            if (!settings.quiet) {                                      \
                begin_print();                                          \
                                                                        \
                const size_t n_ = N;                                    \
                int widths_[n_ - 1] = {};                               \
                for (size_t phase_ = 0, i_ = 0; phase_ < 2; i_ = 0, phase_++) \
                    __VA_ARGS__                                         \
                                                                        \
                end_print();                                            \
            }                                                           \
        } while (false)

        //   `info subject` := Display information on a particular
        //   subject.  The subject can be any of the following:

        else if (!strcmp(s, "info")) {
            if (try_scan(fp, "%63s", s) != 1) {
                print_error("error: no subject specified\n");
                goto error;
            }

            //   `windows` := Describe existing windows.

            if (!strcmp(s, "windows")) {
                PARSING_FINISHED;

                if (!windows) {
                    print_output("No existing windows.\n");
                } else {
                    PRINT_TABLE(
                        4, {
                            COLUMN("%s", "#");
                            COLUMN("%s", "Size");
                            COLUMN("%s", "Vis.");
                            COLUMN("%s", "Name");

                            size_t i = 0;
                            for (struct window *w = windows; w; w = w->next) {
                                int a, b;

                                glfwGetFramebufferSize(w->window, &a, &b);

                                COLUMN("%zu", ++i);
                                COLUMN("%d, %d", a, b);
                                COLUMN("%s", (
                                           glfwGetWindowAttrib(
                                               w->window, GLFW_VISIBLE)
                                           == GL_TRUE ? "Yes" : "No"));
                                COLUMN("%s", w->name);
                            }
                        });
                }
            }

            //   `viewports` := Describe viewports in the current
            //   window.

            else if (!strcmp(s, "viewports")) {
                PARSING_FINISHED;
                NEEDS_WINDOW;

                PRINT_TABLE(
                    10, {
                        COLUMN("%s", "#");
                        COLUMN("%s", "Orig.");
                        COLUMN("%s", "Size");
                        COLUMN("%s", "Trans.");
                        COLUMN("%s", "Rotation");
                        COLUMN("%s", "Zoom");
                        COLUMN("%s", "Pr.");
                        COLUMN("%s", "Name");
                        COLUMN("%s", "Flags");
                        COLUMN("%s", "");

                        size_t i = 0;
                        for (struct viewport *v = w->viewports; v; v = v->next) {
                            const GLfloat *R = v->rotation;

                            // The rotation of the viewport is given
                            // in y-x-z Tait-Bryan angles.  We need to
                            // extract those from its matrix.

                            float alpha, beta, gamma;

                            if (R[6] < 1.0f) {
                                if (R[6] > -1.0f) {
                                    // If $\alpha \in (-{\pi\over
                                    // 2},{\pi\over 2})$, i.e. if $R_6
                                    // = -sin(\alpha) \in (-1, 1)$,
                                    // then $cos(\alpha) \neq 0$, so:

                                    alpha = atan2f(R[2], R[10]);
                                    beta = asinf(-R[6]);
                                    gamma = atan2f(R[4], R[5]);
                                } else {
                                    // If $\alpha = {\pi\over 2}$,
                                    // i.e. $R_6 = -1$, then
                                    // $cos(\alpha) = 0$, then only
                                    // $\gamma - \alpha$ is uniquely
                                    // defined.  We choose:

                                    alpha = -atan2f(R[1], R[0]);
                                    beta = M_PI_2;
                                    gamma = 0.0f;
                                }
                            } else {
                                // If $\alpha = -{\pi\over 2}$,
                                // i.e. $R_6 = 1$, then similarly only
                                // $\gamma + \alpha$ is uniquely
                                // defined.  We choose:

                                alpha = atan2f(R[1], R[0]);
                                beta = -M_PI_2;
                                gamma = 0.0f;
                            }

                            // The `%g` format switches to scientific
                            // notation for numbers smaller than 1e-4.
                            // We don't want that, so we truncate to 5
                            // decimal places.

                            const float x = roundf(v->translation[0] * 1e4);
                            const float y = roundf(v->translation[1] * 1e4);
                            const float z = roundf(v->translation[2] * 1e4);

                            COLUMN("%zu", ++i);
                            COLUMN("%d, %d", v->left, v->bottom);
                            COLUMN(
                                "%d, %d",
                                (v->right - v->left),
                                (v->top - v->bottom));

                            // The reason we use the tertiary operator
                            // below, instead of just diviing the
                            // constants below by `1e4` straight away,
                            // is to avoid having small negative
                            // number rounded to negative zero and
                            // showing up as `-0`.

                            COLUMN(
                                "%.4g, %.4g, %.4g",
                                x == 0.0f ? 0.0f : x / 1e4,
                                y == 0.0f ? 0.0f : y / 1e4,
                                z == 0.0f ? 0.0f : z / 1e4);

                            COLUMN(
                                "%d, %d, %d",
                                (int)roundf(alpha / M_PI * 180.0f),
                                (int)roundf(beta / M_PI * 180.0f),
                                (int)roundf(gamma / M_PI * 180.0f));

                            COLUMN("%g", v->zoom);

                            // We show the field of view angle and
                            // projection mode in one column.

                            {
                                char s[] = "Or.";

                                if (v->projection) {
                                    snprintf(
                                        s, 4, "%d",
                                        (int)roundf(v->angle / M_PI * 180.0f * 2.0f));
                                }

                                COLUMN("%s", s);
                            }

                            COLUMN("%s", v->name);

                            // Viewport flags are shown as a set of letters.

                            {
                                const struct {
                                    bool p;
                                    char c;
                                } flags[] = {
                                    {v->flags.maximized, 'm'},
                                    {v->flags.vertices, 'v'},
                                    {v->flags.edges, 'e'},
                                    {v->flags.faces, 'f'}
                                };

                                const int n = sizeof(flags) / sizeof(flags[0]);
                                char s[n + 3], *c = s;

                                *c++ = '[';

                                for (int i = 0; i < n; i++) {
                                    *c++ = flags[i].p ? flags[i].c : ' ';
                                }

                                *c++ = ']';
                                *c++ = '\0';

                                COLUMN("%s", s);
                            }

                            // We also mark the focused window with an
                            // asterisk.

                            COLUMN("%s", v == w->focus ? "*" : "");
                        }
                    });
            }

            //   `objects` := Describe loaded objects.  All loaded
            //   objects are listed, including currently or previously
            //   displayed in any window.

            else if (!strcmp(s, "objects")) {
                PARSING_FINISHED;

                PRINT_TABLE(
                    6, {
                        COLUMN("%s", "#");
                        COLUMN("%s", "Vert.");
                        COLUMN("%s", "Tri.");
                        COLUMN("%s", "Edges");
                        COLUMN("%s", "AABB");
                        COLUMN("%s", "Name");

                        size_t i = 0;
                        for (struct object *o = objects; o; o = o->next) {

                            COLUMN("%zu", ++i);
                            COLUMN("%d", o->counts[0]);
                            COLUMN("%d", o->counts[1] / 3);
                            COLUMN("%d", o->counts[2] / 2);
                            COLUMN(
                                "%g, %g, %g, %g, %g, %g",
                                o->bounds[0], o->bounds[1],
                                o->bounds[2], o->bounds[3],
                                o->bounds[4], o->bounds[5]);
                            COLUMN("%s", o->name);
                        }
                    });
            }

            //   `bindings` := Describe existing bindings.

            else if (!strcmp(s, "bindings")) {
                PARSING_FINISHED;

                if (key_bindings.count == 0) {
                    print_output("No existing bindings.\n");
                } else {
                    PRINT_TABLE(
                        2, {
                            COLUMN("%s", "Key");
                            COLUMN("%s", "Command");

                            // For each binding we need to:

                            for (size_t i = 0; i < key_bindings.count; i++) {
                                struct key_binding *p
                                    = key_bindings.buffer.p + i;

                                //   1. Skip it if it is deleted, otherwise

                                if (!p->mods && !p->key) {
                                    continue;
                                }

                                //   2. Synthesize the key name, which
                                //   is basically the reverse of
                                //   parsing it, then

                                char s[32] = {}, *q = s;

                                for (size_t j = 0;
                                     j < sizeof(modifier_keys)
                                         / sizeof(modifier_keys[0]);
                                     j++) {
                                    if (modifier_keys[j].i == GLFW_MOD_SHIFT
                                        && isupper(p->key)) {
                                        continue;
                                    }

                                    if (p->mods & modifier_keys[j].i) {
                                        assert(
                                            q - s
                                            + strlen(modifier_keys[j].name) < 32);
                                        q = stpcpy(q, modifier_keys[j].name);
                                    }
                                }

                                if (isgraph(p->key)) {
                                    if (isupper(p->key)
                                        && !(p->mods & GLFW_MOD_SHIFT)) {
                                        *q++ = tolower(p->key);
                                    } else {
                                        *q++ = p->key;
                                    }

                                    *q = '\0';
                                } else {
                                    for (size_t j = 0;
                                         j < sizeof(function_keys)
                                             / sizeof(function_keys[0]);
                                         j++) {
                                        if (p->key == function_keys[j].i) {
                                            q = stpcpy(q, function_keys[j].name);
                                            break;
                                        }
                                    }
                                }

                                //   3. print the key along with the command.

                                COLUMN("%s", s);
                                COLUMN("%s", p->command);
                            }
                        });
                }
            }

            //   `definitions` := List defined parameters.

            else if (!strcmp(s, "definitions")) {
                PARSING_FINISHED;

                size_t i;
                for (i = 0;
                     i < definitions.n && !definitions.p[i].name;
                     i++);

                if (i == definitions.n) {
                    print_output("No existing definitions.\n");
                } else {
                    PRINT_TABLE(
                        2, {
                            COLUMN("%s", "Name");
                            COLUMN("%s", "Value");

                            for (size_t j = i; j < definitions.n; j++) {
                                if (!definitions.p[j].name) {
                                    continue;
                                }

                                COLUMN("%s", definitions.p[j].name);
                                if (definitions.p[j].value) {
                                    COLUMN("%s", definitions.p[j].value);
                                } else {
                                    COLUMN("%s", "");
                                }
                            }
                        });
                }
            }

            else {
                PARSING_FINISHED;

                print_error("error: no such subject\n");
                goto error;
            }
        }

#undef COLUMN
#undef PRINT_TABLE

        // ### Toggling Commands

        // `toggle flag`

        // Toggle a flag on the currently focused viewport.  The
        // available flags are:

        else if (!strcmp(s, "toggle")) {
            if (try_scan(fp, "%63s", s) != 1) {
                print_error("error: no flag specified\n");
                goto error;
            }

            PARSING_FINISHED;
            NEEDS_WINDOW;

            struct viewport *v = w->focus;

#define TOGGLE(FLAG) v->flags.FLAG = !w->focus->flags.FLAG

            //   `maximized` := Allow the viewport to termporarily
            //   occupy the entire window.

            if (!strcmp(s, "maximized")) {
                TOGGLE(maximized);
                v->stale.projection = true;
            }

            //   `vertices` := Draw points to show geometry vertices.

            else if (!strcmp(s, "vertices")) {
                TOGGLE(vertices);
            }

            //   `edges` := Draw lines to show geometry edges.

            else if (!strcmp(s, "edges")) {
                TOGGLE(edges);
            }

            //   `faces` := Draw the geometry faces.

            else if (!strcmp(s, "faces")) {
                TOGGLE(faces);
            }

#undef TOGGLE

            else {
                print_error("error: no such flag\n");
                goto error;
            }

            glfwPostEmptyEvent();
        }

        // ### Setting Commands

        // We handle changing and showing settings with the following
        // macros.

        // Here we set a string setting whose value is the portion of
        // the `set` command line after from the first non-whitespace
        // character after the name of the setting is scanned and up
        // to the newline character, exactly as typed, including
        // whitespace.

#define SET_LINE(FP, SETTING)                   \
        do {                                    \
            if (SETTING) {                      \
                free(SETTING);                  \
                SETTING = nullptr;              \
            }                                   \
                                                \
            try_scan(fp, " %m[^\n]", &SETTING); \
                                                \
            PARSING_FINISHED;                   \
        } while(false);                         \

#define SET_BOOLEAN(FP, SETTING)                                        \
        do {                                                            \
            char s_[6];                                                 \
            try_scan(fp, " %3s", s_);                                   \
                                                                        \
            PARSING_FINISHED;                                           \
                                                                        \
            if (!strcmp(s_, "yes")                                      \
                || !strcmp(s_, "on")                                    \
                || !strcmp(s_, "true")) {                               \
                SETTING = true;                                         \
            } else if (!strcmp(s_, "no")                                \
                       || !strcmp(s_, "off")                            \
                       || !strcmp(s_, "false")) {                       \
                SETTING = false;                                        \
            } else {                                                    \
                print_error("error: \"yes\", or \"no\" expected\n");    \
                goto error;                                             \
            }                                                           \
        } while(false);

        // Here we set up to `N` values of the type specified by the
        // `scanf` specifer `SPEC`.

#define SET_VALUES(FP, SETTING, SPEC, N)                        \
        do {                                                    \
            size_t i_, n_ = N;                                  \
            double d_[N];                                       \
                                                                \
            for (i_ = 0;                                        \
                 i_ < n_ && try_scan(fp, SPEC, &d_[i_]) == 1;   \
                 i_++);                                         \
                                                                \
            PARSING_FINISHED;                                   \
            while (i_-- > 0) {                                  \
                (SETTING)[i_] = d_[i_];                         \
            }                                                   \
        } while(false);

        // The show macros, are analogous to the set macros above.

#define SHOW_STRING(SETTING)                    \
        do {                                    \
            PARSING_FINISHED;                   \
                                                \
            if (SETTING) {                      \
                print_output("%s\n", SETTING);  \
            }                                   \
        } while(false);

#define SHOW_BOOLEAN(SETTING)                   \
        do {                                    \
            PARSING_FINISHED;                   \
                                                \
            if (SETTING) {                      \
                print_output("yes\n");          \
            } else {                            \
                print_output("no\n");           \
            }                                   \
        } while(false);

#define SHOW_VALUES(SETTING, SPEC, N)                   \
        do {                                            \
            PARSING_FINISHED;                           \
                                                        \
            size_t n_ = N;                              \
            for (size_t i = 0; i < n_ - 1; i++) {       \
                print_output(SPEC " ", (SETTING)[i]);   \
            }                                           \
                                                        \
            print_output(SPEC "\n", (SETTING)[n_ - 1]); \
        } while(false);

        // `set setting value`

        // Change the value of a setting.  The available settings are:

        else if (!strcmp(s, "set")) {
            if (try_scan(fp, "%63s", s) != 1) {
                print_error("error: no setting specified\n");
                goto error;
            }

            //   `program` := The executable that should be run in
            //   order to refresh the displayed geometry.

            if (!strcmp(s, "program")) {
                SET_LINE(fp, settings.program);
            }

            //   `args` := The command line arguments to pass to the
            //   program.

            else if (!strcmp(s, "args")) {
                SET_LINE(fp, settings.args);
            }

            //   `quiet` := Do not print any messages to the standard output.

            else if (!strcmp(s, "quiet")) {
                SET_BOOLEAN(fp, settings.quiet);
            }

            //   `present-on-reload` := Present the window to the user
            //   when the contents of one or more of its viewports are
            //   reloaded.

            else if (!strcmp(s, "present-on-reload")) {
                SET_BOOLEAN(fp, settings.present_on_reload);
            }

            //   `recenter-on-reload` := Recenter the view when the
            //   contents of a viewport are reloaded.

            else if (!strcmp(s, "recenter-on-reload")) {
                SET_BOOLEAN(fp, settings.recenter_on_reload);
            }

            //   `resize-on-split` := Resize the window accordingly
            //   when splitting viewports.

            else if (!strcmp(s, "resize-on-split")) {
                SET_BOOLEAN(fp, settings.resize_on_split);
            }

            //   `print-frames` := Resize the window accordingly
            //   when splitting viewports.

            else if (!strcmp(s, "print-frames")) {
                SET_BOOLEAN(fp, settings.print_frames);
            }

            //   `default-view` := The default view angle of newly
            //   created viewports.  Set this to zero to select
            //   orthographic projection.

            else if (!strcmp(s, "default-view")) {
                SET_VALUES(fp, &settings.default_view, "%lf", 1);
            }

            //   `default-zoom` := The default zoom of newly created
            //   viewports.

            else if (!strcmp(s, "default-zoom")) {
                SET_VALUES(fp, &settings.default_zoom, "%lf", 1);
            }

            //   `default-rotation` := The default rotation of newly
            //   created viewports.

            else if (!strcmp(s, "default-rotation")) {
                SET_VALUES(fp, settings.default_rotation, "%lf", 3);
            }

            //   `default-translation` := The default translation of
            //   newly created viewports.

            else if (!strcmp(s, "default-translation")) {
                SET_VALUES(fp, settings.default_translation, "%lf", 3);
            }

            //   `default-vertex-color` := The color assigned to vertices
            //   that do not have a color associated with them.  It is
            //   given as four RGBA floating point values.

            else if (!strcmp(s, "default-vertex-color")) {
                SET_VALUES(fp, settings.default_vertex_color, "%lf", 4);
            }

            //   `vertex-point-size` := The size of the points showing
            //   the locations of the vertices.

            else if (!strcmp(s, "vertex-point-size")) {
                SET_VALUES(fp, &settings.vertex_point_size, "%lf", 1);
            }

            //   `edge-line-width` := The width of the lines used to
            //   draw geometry edges.

            else if (!strcmp(s, "edge-line-width")) {
                SET_VALUES(fp, &settings.edge_line_width, "%lf", 1);
            }

            //   `mouse-sensitivity` := A number that controls how fast
            //   the viewport is rotated, zoomed, etc. with the mouse.

            else if (!strcmp(s, "mouse-sensitivity")) {
                SET_VALUES(fp, &settings.mouse_sensitivity, "%lf", 1);
            }

            else {
                print_error("error: no such setting\n");
                goto error;
            }

            glfwPostEmptyEvent();
        }

        // `show setting`

        // Show the value of a setting.  See the `set` command for
        // possible settings to show.

        else if (!strcmp(s, "show")) {
            if (try_scan(fp, "%63s", s) != 1) {
                print_error("error: no setting specified\n");
                goto error;
            }

            if (!strcmp(s, "program")) {
                SHOW_STRING(settings.program);
            } else if (!strcmp(s, "args")) {
                SHOW_STRING(settings.args);
            } else if (!strcmp(s, "quiet")) {
                SHOW_BOOLEAN(settings.quiet);
            } else if (!strcmp(s, "present-on-reload")) {
                SHOW_BOOLEAN(settings.present_on_reload);
            } else if (!strcmp(s, "recenter-on-reload")) {
                SHOW_BOOLEAN(settings.recenter_on_reload);
            } else if (!strcmp(s, "resize-on-split")) {
                SHOW_BOOLEAN(settings.resize_on_split);
            } else if (!strcmp(s, "print-frames")) {
                SHOW_BOOLEAN(settings.print_frames);
            } else if (!strcmp(s, "default-view")) {
                SHOW_VALUES(&settings.default_view, "%lg", 1);
            } else if (!strcmp(s, "default-zoom")) {
                SHOW_VALUES(&settings.default_zoom, "%lg", 1);
            } else if (!strcmp(s, "default-rotation")) {
                SHOW_VALUES(settings.default_rotation, "%lg", 3);
            } else if (!strcmp(s, "default-translation")) {
                SHOW_VALUES(settings.default_translation, "%lg", 3);
            } else if (!strcmp(s, "default-vertex-color")) {
                SHOW_VALUES(settings.default_vertex_color, "%lg", 4);
            } else if (!strcmp(s, "vertex-point-size")) {
                SHOW_VALUES(&settings.vertex_point_size, "%lg", 1);
            } else if (!strcmp(s, "edge-line-width")) {
                SHOW_VALUES(&settings.edge_line_width, "%lg", 1);
            } else if (!strcmp(s, "mouse-sensitivity")) {
                SHOW_VALUES(&settings.mouse_sensitivity, "%lg", 1);
            } else {
                print_error("error: no such setting\n");
                goto error;
            }

#undef SET_LINE
#undef SET_VALUES
#undef SHOW_STRING
#undef SHOW_VALUES
        }

        // `define name [value]`

        // Change the value of a parameter.  When no value is
        // specified, the parameter is passed to the inferior as a
        // boolean, via the command line option o`-Dname`.  Otherwise
        // the parameter is defined with the option o`-Dname=value`.

        // In the latter case the value should be quoted and escaped
        // as necessary.

        else if (!strcmp(s, "define") || !strcmp(s, "undefine")) {
            const bool p = (s[0] == 'u');

            if (try_scan(fp, "%63s", s) != 1) {
                print_error("error: no parameter specified\n");
                goto error;
            }

            size_t j = definitions.n;
            for (size_t i = 0; i < definitions.n; i++) {
                if (!definitions.p[i].name) {
                    j = i;
                } else if (!strcmp(definitions.p[i].name, s)) {
                    j = i;
                    goto defined;
                }
            }

            if (p) {
                print_error("error: parameter %s has not been defined\n", s);
                goto error;
            }

            if (j == definitions.n) {
                MAYBE_GROW_TO(definitions, j + 1);
                memset(
                    definitions.p + j,
                    0,
                    (definitions.n - j) * sizeof(definitions.p[0]));
            }

            definitions.p[j].name = strdup(s);

          defined:
            if (p) {
                free(definitions.p[j].name);
                definitions.p[j].name = nullptr;
            } else {
                if (definitions.p[j].value) {
                    free(definitions.p[j].value);
                    definitions.p[j].value = nullptr;
                }

                try_scan(fp, " %m[^\n]", &definitions.p[j].value);
            }
        }

        else {
            print_error("error: invalid command \"%s\"\n", s);
            goto error;
        }

        continue;

      error:
        // If we found an error while parsing, we need to discard the
        // rest of the input.

        return -1;
    }
}

#undef SCAN_VIEWPORT
