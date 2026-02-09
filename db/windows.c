#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "common.h"
#include SHADER_STRINGS

#include <gl2ps.h>

// Document: program

// # Windows

// We create one or more windows in which geometry can be drawn for
// inspection.  Each window can be further subdivided into multiple
// viewports, each of which is dedicated to a particular piece of
// geometry.

struct window *windows;

// One viewport is said to be "focused".  This is the viewport in
// which the mouse cursor is located and which can be manipulated
// through the mouse and keyboard.

// Depending on the user's actions, we're always in one of several
// modes of manipulation.

static enum: int {
    IDLE = 0,
    ROTATING = GLFW_MOUSE_BUTTON_LEFT + 1,
    PANNING = GLFW_MOUSE_BUTTON_RIGHT + 1,
    ZOOMING = GLFW_MOUSE_BUTTON_MIDDLE + 1
} mode;

// ## Input Callbacks

// The following functions implement viewport manipulations for the
// windows, starting with keyboard input.

static void key_callback(
    GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS) {
        const struct key_binding *p = find_key_binding(mods, key);

        if (p) {
            FILE *fp = fmemopen((char *)p->command, strlen(p->command), "r");
            read_commands(fp);
            fclose(fp);
        }
    }
}

// The following callback handles mouse motion.  Modes such as
// rotation, zooming and panning are concerned with relative motion,
// so we need to calculate the position relative to the previous
// callback.  We keep the cursor location in a couple of variables.

static double previous_x = NAN, previous_y = NAN;

static void cursor_position_callback(GLFWwindow *window, double x, double y)
{
    struct window *w = (struct window *)glfwGetWindowUserPointer(window);

    switch (mode) {
    // When no mouse buttons are held down, the user is just browsing
    // the window.  We need to detect when the cursor transitions
    // between viewports and update the focus accordingly.

    case IDLE:
    {
        int a, b;
        glfwGetFramebufferSize(window, &a, &b);

        const double y_1 = b - y;
        struct viewport *v = w->viewports;

        while (
            v && (v->left > x || v->right < x
                  || v->bottom > y_1 || v->top < y_1)) {
            v = v->next;
        }

        // We can move the pointer out of all viewports, even if they
        // seem to span the whole window.  Moving the pointer on the
        // decorations still fires this callback.

        if (v) {
            w->focus = v;
        }
    }

        break;

    // Camera manipulation modes are handled in their own functions.

    case ROTATING:
    case PANNING:
    case ZOOMING:
        {
            const GLfloat c = settings.mouse_sensitivity;

            if (mode == ZOOMING) {
                zoom_viewport(w->focus, c * (GLfloat)(y - previous_y));
            } else if (mode == ROTATING) {
                rotate_viewport(
                    w->focus,
                    c * (GLfloat)(x - previous_x),
                    c * (GLfloat)(y - previous_y),
                    0);
            } else if (mode == PANNING) {
                // The appropriate panning sensitivity depends on the
                // current viewport projecton.  If we're viewing a
                // large object, zoomed out to fill the viewport, we
                // want to pan faster (in terms of world units per
                // mouse motion pixels) than when viewing a small
                // object, or when zoomed into a detail of a large
                // object.

                // We therefore scale the sensitivity by the current
                // viewing volume extents.

                struct viewport *v = w->focus;
                const GLfloat a =
                    (GLfloat)(v->top - v->bottom) / (v->right - v->left);
                const GLfloat w = v->object->bounds[3] - v->object->bounds[0];
                const GLfloat h = v->object->bounds[4] - v->object->bounds[1];
                const GLfloat dim = fmaxf(w, h / a);
                const float d = c * dim / v->zoom / 2.0f;

                pan_viewport(
                    v,
                    -d * (GLfloat)(x - previous_x),
                    d * (GLfloat)(y - previous_y));
            }
        }

        break;
    }

    previous_x = x;
    previous_y = y;
}

// Finally this callback handles mouse button presses.

static void mouse_button_callback(
    GLFWwindow *window, int button, int action, int mods)
{
    glfwGetCursorPos(window, &previous_x, &previous_y);

    // When manipulating the camera we hide the mouse cursor switch
    // GLFW to relative motion mode.

    if (action == GLFW_PRESS && mode == IDLE) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        mode = button + 1;
    } else if (action == GLFW_RELEASE && mode == button + 1) {
        mode = IDLE;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

// The following callback is used with the GL_KHR_debug extension to
// automatically print out any GL errors as they are generated.
// Useful to ensure everything is spick and span GL-wise.

#ifndef NDEBUG
static void APIENTRY debug_message_callback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar *message,
    const void *userParam)
{
    print_error("GL ");

    switch (severity) {
    case GL_DEBUG_SEVERITY_NOTIFICATION:
        print_error("note");
        break;
    case GL_DEBUG_SEVERITY_LOW:
        print_error("warning");
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        print_error("severe warning");
        break;
    case GL_DEBUG_SEVERITY_HIGH:
        print_error("error");
        break;
    }

    print_error(": %s\n", message);
}
#endif

// ## GLSL Programs

// We use a structure to keep track of the GL name and uniform
// locations for each program we create.  These are the following:

//   1. The uniform program fills each pixel with a uniform color.
//   It's mostly useful for drawing the UI elements.

struct {
    GLuint name;

    GLuint matrix, color;
} uniform;

//   2. The `flat` program is for flat shading with per-vertex colors.

struct {
    GLuint name;

    GLuint matrix, intensity;
} flat;

//   3. The `sprite` program is for sprite-like textured quads,
//   e.g. text.

struct {
    GLuint name;

    GLuint matrix, color;
} sprite;

// The following functions compile and link shaders and programs.

static GLuint compile_shader(GLenum type, const char *source)
{
    const GLuint i = glCreateShader(type);

    glShaderSource(i, 1, &source, nullptr);
    glCompileShader(i);

    GLint p;
    glGetShaderiv(i, GL_COMPILE_STATUS, &p);

    if (!p) {
        GLchar s[512];
        glGetShaderInfoLog(i, 512, nullptr, s);
        print_error("Shader compilation error:\n%s\n", s);
    }

    return i;
}

static GLuint create_program(GLuint vertex, GLuint fragment)
{
    GLuint i = glCreateProgram();

    glAttachShader(i, vertex);
    glAttachShader(i, fragment);
    glLinkProgram(i);

    GLint p;
    glGetProgramiv(i, GL_LINK_STATUS, &p);

    if (!p) {
        GLchar s[512];
        glGetProgramInfoLog(i, 512, nullptr, s);
        print_error("Program linking error:\n%s\n", s);
    }

    return i;
}

// ## Text Rendering

// We need to draw annotations inside the viewports, so we need a way
// to render text.  We use Freetype to load and render the glyphs for
// the text we need to display into a texture.  Ref: Text Definitions.

// Before rendering text, we need to create a texture to hold the
// rendered image.

struct text *make_text(void)
{
    struct text *t = (struct text *)malloc(sizeof(struct text));

    glGenTextures(1, &t->texture);
    glBindTexture(GL_TEXTURE_2D, t->texture);

    // The texture will be rendered without minification or
    // magnification, so that mipmapping is not necessary.
    // Furthermore, if we are careful to translate it into position so
    // that texels and screen pixels align, we should be able to
    // dispense with linear filtering as well.

    // We do linear filtering nevertheless, just to be on the safe
    // side, in case we come across some GL implementation that
    // misbehaves.

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Again, although this should not be necessary, we do clamp
    // texture coordinates, in case any them end up slightly outside
    // $[0, 1]$ after interpolation.

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return t;
}

// Here we render a piece of text into the texture.  This may happen
// more than once with the same texture, if the text needs to be
// updated.

void printf_text(struct text *t, size_t size, const char *fmt, ...)
{
    // The first time we attempt to render text, we initialize
    // Freetype and try to load a face.  Until we determine whether
    // this can fail in practice and under what circumastances, we
    // simply return (leaving the texture untouched) if it does.

    static FT_Library ft;
    static FT_Face face;

    if (!ft) {
        if (FT_Init_FreeType(&ft) != FT_Err_Ok) {
            return;
        }
    }

    if (!face) {
        // To load a font, we first need to find it.  We use
        // Fontconfig to locate a suitable system font.  What we want
        // is a "good quality sans-serif font"; we request that and
        // use an environment variable to allow finetuning in case the
        // default match proves inadequate.

        FcPattern *p, *m = nullptr;
        FcChar8 *s = (FcChar8 *)getenv("GAMMADB_FONT");

        p = FcNameParse(s ? s : (FcChar8 *)"sans-serif");
        if (!p) {
            print_error("Could not parse the font pattern\n");
            goto error;
        }

        FcConfigSubstitute(0, p, FcMatchPattern);
        FcDefaultSubstitute(p);

        FcResult r;
        m = FcFontMatch (0, p, &r);

        if (r != FcResultMatch
            || FcPatternGetString(m, FC_FILE, 0, &s) != FcResultMatch) {
            print_error("Could not match the font pattern\n");
            goto error;
        }

        if (FT_New_Face(ft, (char *)s, 0, &face) != FT_Err_Ok) {
            goto error;
        }

      error:
        // One or more of `p`, `m` can be null here, but
        // `FcPatternDestroy` obligingly ignores `nullptr` arguments.

        FcPatternDestroy(p);
        FcPatternDestroy(m);
        FcFini();

        if (!face) {
            return;
        }
    }

    if (FT_Set_Pixel_Sizes(face, 0, size) != FT_Err_Ok) {
        return;
    }

    // Next, we construct the string that is to be rendered.

    char *s;
    va_list ap;

    va_start(ap, fmt);
    vasprintf(&s, fmt, ap);
    va_end(ap);

    // Rendering proceeds in two phases: first we measure the bounding
    // box dimensions the resulting text image will have and then we
    // allocate a buffer and go again, rendering the glyphs into it.

    // We proceed according to the "subpixel positioning" algorithm
    // laid out in Freetye's documentation^[See
    // https://freetype.org/freetype2/docs/glyphs/glyphs-5.html#section-2].
    // Some parts of the process are the same, so we use the macros
    // below to:

    //   1. Load the current glyph and transform it by the fractional
    //   part of the pen position and

    const size_t m = strlen(s);

#define LOAD_CHAR(FLAGS)                                        \
    do {                                                        \
        FT_Vector delta = {x_0 & 63, 0};                        \
        FT_Set_Transform(face, nullptr, &delta);                \
                                                                \
        if (FT_Load_Char(face, s[n], FLAGS) != FT_Err_Ok) {     \
            continue;                                           \
        }                                                       \
    } while (false)

    //   2. advance the pen position before going to the next glyph.

#define ADVANCE                                         \
    do {                                                \
        if (FT_HAS_KERNING(face)) {                     \
            FT_Vector delta;                            \
                                                        \
            FT_Get_Kerning(                             \
                face,                                   \
                FT_Get_Char_Index(face, s[n]),          \
                FT_Get_Char_Index(face, s[n + 1]),      \
                FT_KERNING_DEFAULT,                     \
                &delta);                                \
                                                        \
            x_0 += delta.x;                             \
        }                                               \
                                                        \
        x_0 += g->advance.x;                            \
    } while (false)

    // This is the measuring phase.  We update the vertical extents of
    // the bounding box for each glyph.  The horizontal extents can be
    // calculated at once from the metrics of the first and last
    // glyphs.

    int x_min, x_max, y_min, y_max;
    for (size_t i = 0; i < 2; i++) {
        for (size_t n = 0, x_0 = 0; ; n++) {
            LOAD_CHAR(FT_LOAD_DEFAULT);

            auto g = face->glyph;

            if (n == 0) {
                x_min = g->bitmap_left;
                y_max = g->bitmap_top;
                y_min = y_max - g->bitmap.rows;
            } else {
                if (y_max < g->bitmap_top) {
                    y_max = g->bitmap_top;
                }

                if (y_min > (int)g->bitmap_top - (int)g->bitmap.rows) {
                    y_min = g->bitmap_top - (int)g->bitmap.rows;
                }
            }

            if (n == m - 1) {
                x_max = (x_0 >> 6) + g->bitmap_left + g->bitmap.width + 1;
                break;
            }

            ADVANCE;
        }
    }

    // We can now calculate the dimensions of the texture and allocate
    // a buffer for its contents, then go again rendering each glyph
    // and copying it into the buffer

    t->width = (x_max - x_min);
    t->height = (y_max - y_min);
    t->offset = x_min;
    t->descent = y_min;

    GLubyte texels[t->width * t->height] = {};

    for (size_t n = 0, x_0 = 0; n < m ; n++) {
        LOAD_CHAR(FT_LOAD_RENDER);

        auto g = face->glyph;

        if (g->bitmap.buffer) {
            const int j_0 = g->bitmap_top - y_min - 1;
            const int i_0 = (x_0 >> 6) + g->bitmap_left - x_min;

            assert(j_0 >= (int)g->bitmap.rows - 1);
            assert(j_0 < t->height);
            assert(i_0 >= 0);
            assert(i_0 + (int)g->bitmap.width < t->width);

            for (size_t j = 0; j < g->bitmap.rows; j++) {
                for (size_t i = 0; i < g->bitmap.width; i++) {
                    GLubyte *p = &texels[(j_0 - j) * t->width + i_0 + i];
                    const unsigned int a =
                        *p + g->bitmap.buffer[j * g->bitmap.pitch + i];

                    *p = a > 255 ? 255 : a;
                }
            }
        }

        ADVANCE;
    }

    // Finally we upload the texture.  Since the image only has one
    // component per pixel, each row may start on an arbitrary
    // alignment, depending on the horizontal dimension of the
    // texture.  We therefore need to set the unpack alignment to 1
    // (from the default which is 4).

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, t->texture);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RED, t->width, t->height, 0, GL_RED,
        GL_UNSIGNED_BYTE, texels);

    free(s);
}

// ## Protecting Access to Window Contexts

// A window's context can potentially be used by threads other than
// the main, for instance when the IPC thread loads new geometry.  We
// need to ensure that only one thread is accessing the context at any
// one time.

// We use mutexes wrapped in the functions below.  Context switch must
// only be carried out through them.

static void lock_window(struct window *w)
{
    pthread_mutex_lock(&w->mutex);

    assert(!glfwGetCurrentContext());
    glfwMakeContextCurrent(w->window);
}

static void unlock_window(struct window *w)
{
    assert(glfwGetCurrentContext() == w->window);
    glfwMakeContextCurrent(nullptr);

    pthread_mutex_unlock(&w->mutex);
}

// ## Window Creation

// We can now see about creating windows.  This happens in the
// function below, assuming we don't find a window by the specified
// name has already been created.

struct window *find_window(const char *name)
{
    const int width = 500, height = 500;
    struct window *w = nullptr;

    for (w = windows; w && strcmp(w->name, name); w = w->next);

    if (w) {
        return w;
    }

    // First, we allocate and link in the new window and fill in some
    // members.

    w = (struct window *)malloc(sizeof(struct window));

    w->name = strdup(name);
    w->next = windows;

    pthread_mutex_init(&w->mutex, nullptr);

    // The window is created with a single viewport spanning it, which
    // we initialize below.

    w->focus = w->viewports =
        (struct viewport *)malloc(sizeof(struct viewport));

#define EYE {                                   \
    1.0f, 0.0f, 0.0f, 0.0f,                     \
    0.0f, 1.0f, 0.0f, 0.0f,                     \
    0.0f, 0.0f, 1.0f, 0.0f,                     \
    0.0f, 0.0f, 0.0f, 1.0f                      \
}

    *w->viewports = (struct viewport){
        strdup("1"),
        {true, true},
        0, width - 1, 0, height - 1,
        (settings.default_view > 0.0f ? PERSPECTIVE : ORTHOGRAPHIC),
        0.1f, 1.0f,
        ((settings.default_view > 0.0f ? settings.default_view : 50.0f) / 2.0f
         / 180.0f * M_PI),
        settings.default_zoom,
        {0.0f, 0.0f, 0.0f},
        EYE, EYE,
        0,
        nullptr,
        nullptr,
        nullptr
    };

    translate_viewport(
        w->viewports,
        settings.default_translation[0],
        settings.default_translation[1],
        settings.default_translation[2]);

    rotate_viewport(
        w->viewports,
        settings.default_rotation[0],
        settings.default_rotation[1],
        settings.default_rotation[2]);

#undef EYE

    // Now, we create the actual window.  If this is not the first
    // window we share the previously created window's context.  This
    // establishes a chain of shared contexts so that any window can
    // use buffer objects created in any other.

    w->window = glfwCreateWindow(
        width, height, name, nullptr, w->next ? w->next->window : nullptr);

    if (!w->window) {
        print_error("Could not create GLFW window\n");
        exit(EXIT_FAILURE);
    }

    // We register our callbacks and proceed to make the context
    // current and initialize some state.  This needs to be done for
    // each window's context as only data are geneally shared, not the
    // state.

    glfwSetWindowUserPointer(w->window, (void *)w);
    glfwSetKeyCallback(w->window, key_callback);
    glfwSetMouseButtonCallback(w->window, mouse_button_callback);
    glfwSetCursorPosCallback(w->window, cursor_position_callback);

    lock_window(w);

    // The contexts are created with the same APIs so the function
    // pointers should be re-usable between them.  We therefore only
    // initialize GLAD once.

    if (!w->next && !gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        print_error("Could not initialize GLAD\n");
        exit(EXIT_FAILURE);
    }

    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);

    glEnable(GL_MULTISAMPLE);

    glPolygonOffset(1.0f, 1.0f);

    // If enabled, we set up a debug message callback, to be notified
    // about any GL errors.

#ifndef NDEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(debug_message_callback, nullptr);
    glDebugMessageControl(
        GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
#endif

    // We need to create array and buffer objects for the GUI.  This
    // consists of geometry for a baground quad for each viewport and
    // a rectangle for its border.  Both use the same four vertices.

    // VAOs are not shared between contexts, so each window gets its
    // own.  On the other hand, VBOs are shared so we only create the
    // object for the first window.

    glGenVertexArrays(1, &w->vao);
    glBindVertexArray(w->vao);

    if (w->next) {
        w->vbo = w->next->vbo;
        glBindBuffer(GL_ARRAY_BUFFER, w->vbo);
    } else {
        glGenBuffers(1, &w->vbo);

        float vertices[] = {
            -1.0f, -1.0f,
            1.0f, -1.0f,
            1.0f, 1.0f,
            -1.0f, 1.0f
        };

        glBindBuffer(GL_ARRAY_BUFFER, w->vbo);
        glBufferData(
            GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    }

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0,(void*)0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Program objects are also shared between contexts.  We compile
    // the program when creating the first window only.

    if (!w->next) {
#define BUILD_PROGRAM(NAME)                                     \
        do {                                                    \
            assert(!NAME.name);                                 \
                                                                \
            const GLuint i = compile_shader(                    \
                GL_VERTEX_SHADER, NAME ##_vertex_c);            \
            const GLuint j = compile_shader(                    \
                GL_FRAGMENT_SHADER, NAME ##_fragment_c);        \
                                                                \
            NAME.name = create_program(i, j);                   \
                                                                \
            glDeleteShader(i);                                  \
            glDeleteShader(j);                                  \
        } while (false)

#define WITH_UNIFORM(NAME, VAR)                                 \
        do {                                                    \
            NAME.VAR = glGetUniformLocation(NAME.name, #VAR);   \
        } while (false)

        BUILD_PROGRAM(uniform);
        WITH_UNIFORM(uniform, matrix);
        WITH_UNIFORM(uniform, color);

        BUILD_PROGRAM(flat);
        WITH_UNIFORM(flat, matrix);
        WITH_UNIFORM(flat, intensity);

        BUILD_PROGRAM(sprite);
        WITH_UNIFORM(sprite, matrix);
        WITH_UNIFORM(sprite, color);

#undef BUILD_PROGRAM
#undef WITH_UNIFORM
    }

    unlock_window(w);

    // We add the window to the list now that it's fully configured,
    // so as to avoid having the main loop to pick it up prematurely.

    windows = w;
    return w;
}

// ## Window Manipulation

// The following functions allow manipulation of a window.  They're
// generally invoked via bindings, or commands on the terminal.

void resize_window(struct window *w, int width, int height)
{
    int w_0, h_0;

    lock_window(w);
    glfwGetFramebufferSize(w->window, &w_0, &h_0);
    glfwSetWindowSize(w->window, width, height);

    for (struct viewport *v = w->viewports; v; v = v->next) {
        v->left = v->left * (width - 1) / (w_0 - 1);
        v->right = v->right * (width - 1) / (w_0 - 1);
        v->bottom = v->bottom * (height - 1) / (h_0 - 1);
        v->top = v->top * (height - 1) / (h_0 - 1);
        v->stale.projection = true;
    }

    unlock_window(w);

    // There can be a delay until the size is actually set on the
    // window.  Interactively, this shouldn't be a problem, but it can
    // be when executing command files or in tests.  (For instance, a
    // `resize` command, followed by an `info windows` command can
    // show the old size in one run and the new size in another.)

    do {
        glfwGetFramebufferSize(w->window, &w_0, &h_0);
    } while (w_0 != width || h_0 != height);

    glfwPostEmptyEvent();
}

// ## Refreshing Window Contents

// Each time the GLFW detects a mouse or keyboard event, we redraw the
// windows.  Most viewports in most windows will not have changed, but
// redrawing happens rarely enough that being too fussy about what
// parts of what windows to redraw is probably not worth the trouble.

static bool refresh_window(struct window *w)
{
    // Once we've determined the window isn't hidden, or about to be,
    // we lock it and begin.

    if (glfwWindowShouldClose(w->window)) {
        glfwHideWindow(w->window);
        glfwSetWindowShouldClose(w->window, GLFW_FALSE);
        return false;
    }

    if (glfwGetWindowAttrib(w->window, GLFW_VISIBLE) == GLFW_FALSE) {
        return false;
    }

    lock_window(w);

    // We clear the window and for each viewport, we:

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    size_t i = 1;
    for (struct viewport *v = w->viewports; v; v = v->next, i++) {
        // Redraw the background and frame of the viewport.  This
        // consists in drawing a unit quad, first filled and then as a
        // line loop, scaled so that its edges run through the middle
        // of the pixels on the edge of the viewport.

        // This is done with the matrix below, along with the
        // following viewport specification.

        const GLfloat a = v->right - v->left;
        const GLfloat b = v->top - v->bottom;

        glViewport(v->left, v->bottom, (GLsizei)a, (GLsizei)b);

        {
            const GLfloat S[16] = {
                (a - 1.0f) / a, 0.0f, 0.0f, 0.0f,
                0.0f, (b - 1.0f) / b, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
            };

            // Now we bind the uniform color program and draw the quads.

            glUseProgram(uniform.name);

            // This is a workaround around a Mesa driver bug.^[See:
            // https://gitlab.freedesktop.org/mesa/mesa/-/issues/14129]

            {
                const GLfloat M[16] = {};
                glUniformMatrix4fv(uniform.matrix, 1, GL_TRUE, M);
                glUniform4f(uniform.color, 0.0f, 0.0f, 0.0f, 0.0f);
            }

            glUniformMatrix4fv(uniform.matrix, 1, GL_TRUE, S);

            if (v == w->focus) {
                glUniform4f(uniform.color, 0.7f, 0.7f, 0.7f, 1.0f);
            } else {
                glUniform4f(uniform.color, 0.5f, 0.5f, 0.5f, 1.0f);
            }

            glBindVertexArray(w->vao);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

            if (v == w->focus) {
                glUniform4f(uniform.color, 1.0f, 0.85f, 0.24f, 1.0f);
            } else {
                glUniform4f(uniform.color, 0.2f, 0.2f, 0.2f, 1.0f);
            }

            glDrawArrays(GL_LINE_LOOP, 0, 4);
        }

        // We can now draw the viewport's geometry, assuming it has
        // some.

        if (v->vao) {
            assert(v->object);

            // First, we recalculate the viewport's projection if
            // needed.

            if (v->stale.projection) {
                refresh_viewport(v);
                v->stale.projection = false;
            }

            // Now we can draw the:

            const GLsizei *counts = v->object->counts;

            glEnable(GL_DEPTH_TEST);
            glBindVertexArray(v->vao);

            glUseProgram(flat.name);
            glUniformMatrix4fv(flat.matrix, 1, GL_TRUE, v->matrix);

            //   1. geometry faces, offsetting it away from the camera
            //   to prevent z-fighting with the following passes and
            //   dimming the color intensity, if the viewport's not
            //   focues, then

            if (v == w->focus) {
                glUniform1f(flat.intensity, 1.0f);
            } else {
                glUniform1f(flat.intensity, 0.75f);
            }

            glEnable(GL_POLYGON_OFFSET_FILL);
            glDrawElements(GL_TRIANGLES, counts[1], GL_UNSIGNED_INT, 0);
            glDisable(GL_POLYGON_OFFSET_FILL);

            //   2. the edges, drawn as a sequence of line segments,

            glUseProgram(uniform.name);
            glUniformMatrix4fv(uniform.matrix, 1, GL_TRUE, v->matrix);
            glUniform4f(
                uniform.color,
                (GLfloat)settings.edge_color[0],
                (GLfloat)settings.edge_color[1],
                (GLfloat)settings.edge_color[2],
                (GLfloat)settings.edge_color[3]);

            glDrawElements(
                GL_LINES, counts[2], GL_UNSIGNED_INT,
                (void *)(counts[1] * sizeof(GLuint)));

            //   3. the vertices, drawn as points and finally,

            glUniform4f(uniform.color, 0.0f, 0.0f, 1.0f, 1.0f);
            glPointSize(5);
            glDrawArrays(GL_POINTS, 0, counts[0]);

            glDisable(GL_DEPTH_TEST);
        }

        //   4. text annotations, which is currently just the viewport
        //   index and target^[We only display the target if it has
        //   been changed from the default, which is the viewport
        //   index].

        {
            if (!v->annotation) {
                v->annotation = make_text();
            }

            struct text *t = v->annotation;

            if (v->stale.annotation) {
                size_t j = 0;
                sscanf(v->name, "%zu", &j);
                printf_text(t, 18, (j != i) ? "%d: %s" : "%d", i, v->name);

                v->stale.annotation = false;
            }

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glBindTexture(GL_TEXTURE_2D, t->texture);

            for (int i = (v != w->focus); i < 2 ; i++) {
                //   This is the equivalent of the product of matrices
                //   $P T S$, where $P$ is an orthographic projection
                //   matrix with $l = 0, r = a, b = 0, t = b$,
                //   i.e. one that maps coordinates to pixels, T is a
                //   translation by $\lfloor x \rfloor + {1 \over 2}$,
                //   $\lfloor y \rfloor + {1 \over 2}$, in pixels and
                //   S is a scaling matrix to scale the 2 by 2 quad to
                //   the size of the text.

                //   We shift the position to the next pixel center,
                //   because during rasterization, data that are
                //   associated with fragmnets are interpolated based
                //   on the fragment centers.  The shift should ensure
                //   that all filled fragments end up with texture
                //   coordinates that precisely correspond to a
                //   texel.^[This is not critical, since we filter the
                //   texture anyway, but it won't hurt either.]

                const GLfloat x = floorf(
                    10.0f + i + t->width / 2.0f + t->offset) + 0.5f;
                const GLfloat y = floorf(
                    11.0f + i + t->height / 2.0f + t->descent) + 0.5f;

                const GLfloat S[16] = {
                    t->width / a, 0.0f, 0.0f, x * 2.0f / a - 1.0f,
                    0.0f, t->height / b, 0.0f, y * 2.0f / b - 1.0f,
                    0.0f, 0.0f, 1.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 1.0f
                };

                glUseProgram(sprite.name);
                glUniformMatrix4fv(sprite.matrix, 1, GL_TRUE, S);

                if (i == 0) {
                    glUniform4f(sprite.color, 1.0f, 0.85f, 0.24f, 1.0f);
                } else {
                    glUniform4f(sprite.color, 0.15f, 0.15f, 0.15f, 1.0f);
                }

                glBindVertexArray(w->vao);
                glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            }

            glDisable(GL_BLEND);
        }
    }

    glBindVertexArray(0);
    glUseProgram(0);

    glfwSwapBuffers(w->window);
    unlock_window(w);

    return true;
}

bool refresh_windows(void)
{
    bool p = false;

    for (struct window *w = windows; w; w = w->next) {
        p = p || refresh_window(w);
    }

    return p;
}

// ## Refreshing Object Geometry

// Since this mostly concerns the GL context, we also take care of
// loading viewport geometry here.  For the nitty-gritty of how this
// geometry is structured, ref: Loading Object Geometry.

// Here we only concern ourselves with bind this geometry to viewports
// and loading it in their VBOs.

struct object *objects;

void refresh_object(
    const char *name,
    size_t n, float *vertices,
    size_t m, unsigned int *triangles,
    size_t l, unsigned int *edges)
{
    struct object *o;

    // Again, we look for a pre-existing object with the given name.

    for (o = objects; o && strcmp(o->name, name); o = o->next);

    for (struct window *w = windows; w; w = w->next) {
        for (struct viewport *v = w->viewports; v; v = v->next) {
            // Ojbects have a name which, by convention, binds them
            // with viewports.  That is to say, the object is
            // displayed in all viewports of the same name, in any
            // window.

            if (strcmp(v->name, name)) {
                continue;
            }

            // If an object doesn't already exist, we create one now.

            if (!o) {
                o = (struct object *)malloc(sizeof(struct object));
                o->next = objects;
                objects = o;

                o->name = strdup(name);
                o->vbo = 0;
                o->ebo = 0;
            }

            // Now we need to update the object with the new geometry.

            o->counts[0] = n;
            o->counts[1] = m;
            o->counts[2] = l;

            // We also calculate an AABB for the object.  This allows
            // us to center it within its viewport.

            o->bounds[0] = o->bounds[1] = o->bounds[2] = INFINITY;
            o->bounds[3] = o->bounds[4] = o->bounds[5] = -INFINITY;

            lock_window(w);

            // We generate a VBO is this is a new object ^[VBOs are
            // shared, so we only need one for each object.] and
            // proceed to copy the vertex data into it.

            if (!o->vbo) {
                glGenBuffers(1, &o->vbo);
            }

            glBindBuffer(GL_ARRAY_BUFFER, o->vbo);

#if 0
            // In practice we would probably be able to do the
            // following on virtually all platforms we're expected to
            // run on.

            glBufferData(GL_ARRAY_BUFFER,
                         7 * n * sizeof(GLfloat), vertices,
                         GL_DYNAMIC_DRAW);
#else
            // Still, technically, `GLlfoat` is not guaranteed to be
            // the same as `float`, so we cast the values to
            // `GLfloat`, just to be on the safe side.

            glBufferData(GL_ARRAY_BUFFER,
                         7 * n * sizeof(GLfloat), nullptr,
                         GL_DYNAMIC_DRAW);

            {
                // We copy 7 values per vertex: 3 positinal
                // coordinates + 4 color coordinates.

                GLfloat *p = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
                for (size_t i = 0; i < 7 * n; i++) {
                    p[i] = (GLfloat)vertices[i];

                    const size_t j = i % 7;

                    // We also update the AABB.

                    if (j < 3) {
                        if (p[i] < o->bounds[j]) {
                            o->bounds[j] = p[i];
                        }

                        if (p[i] > o->bounds[j + 3]) {
                            o->bounds[j + 3] = p[i];
                        }
                    }
                }
            }

            glUnmapBuffer(GL_ARRAY_BUFFER);
#endif
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            // The situation is analogous for the EBO.  We use the
            // same buffer object for both triangle and edge indices;
            // no need to switch more state than is necessary.

            if (!o->ebo) {
                glGenBuffers(1, &o->ebo);
            }

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, o->ebo);

            glBufferData(
                GL_ELEMENT_ARRAY_BUFFER,
                (m + l) * sizeof(GLuint), nullptr,
                GL_DYNAMIC_DRAW);

#if 0
            glBufferSubData(
                GL_ELEMENT_ARRAY_BUFFER,
                0, m * sizeof(GLuint),
                triangles);

            glBufferSubData(
                GL_ELEMENT_ARRAY_BUFFER,
                m * sizeof(GLuint), l * sizeof(GLuint),
                edges);
#else
            {
                GLuint *p = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);

                for (size_t i = 0; i < m; i++) {
                    p[i] = triangles[i];
                }

                for (size_t i = 0; i < l; i++) {
                    p[m + i] = edges[i];
                }
            }

            glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
#endif
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

            unlock_window(w);

            // Now we need to associate the buffer objects with the
            // viewport's VAO.

            if (v->object != o) {
                lock_window(w);

                // We bind the new object with any viewports that
                // share its name by:

                //   1. allocating the vertex array object, if it
                //   doesn't already have one,

                if (!v->vao) {
                    glGenVertexArrays(1, &v->vao);
                }

                //   2. binding the VBO and EBO to the VAO and finally

                glBindVertexArray(v->vao);
                glBindBuffer(GL_ARRAY_BUFFER, o->vbo);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, o->ebo);

                //   3. setting up the vertex attribute for the vertex data.

                glEnableVertexAttribArray(0);
                glVertexAttribPointer(
                    0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(GLfloat),
                    (void*)0);

                glEnableVertexAttribArray(1);
                glVertexAttribPointer(
                    1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(GLfloat),
                    (void*)(3 * sizeof(GLfloat)));

                glBindVertexArray(0);

                // Note that GL_ELEMENT_ARRAY_BUFFER_BINDING is a part
                // of the VAO state and is reset when the VAO binding
                // is reset above.

                glBindBuffer(GL_ARRAY_BUFFER, 0);
                unlock_window(w);

                v->object = o;
            }

            // Having calculated the object's AABB, we can recenter
            // the viewport, but we only do so if the user hasn't
            // adjusted the translation already.  We want to help, not
            // annoy.

            if (v->translation[0] == 0.0f
                && v->translation[1] == 0.0f
                && v->translation[2] == 0.0f) {
                translate_viewport(v, NAN, NAN, NAN);
            }

            // Finally we present the window found to be showing the
            // object to the user, if so configured.

            if (settings.present_on_reload) {
                glfwShowWindow(w->window);
                glfwPostEmptyEvent();
            }
        }
    }
}

// ## Printing Object Geometry

// The following function prints the contents of a window in one of
// several formats.  It is mostly useful to prepare figuresf for
// documentation.

void print_window(struct window *w, GLint format, FILE *fp)
{
    // We use the window name as document title and set the document
    // viewport dimensions to match our window.

    {
        int a, b;
        glfwGetFramebufferSize(w->window, &a, &b);

        safely_assert(
            gl2psBeginPage(
                w->name, "gammadb",
                (GLint []){0, 0, a, b},
                format, GL2PS_BSP_SORT,
                GL2PS_NO_OPENGL_CONTEXT | GL2PS_NO_BLENDING
                | GL2PS_OCCLUSION_CULL,
                GL_RGBA, 0, nullptr, 0, 0 ,0,
                0, fp, nullptr) == GL2PS_SUCCESS);
    }

    // We need to bind and map VBOs and EBOs, so we need to lock the
    // window's context.

    lock_window(w);

    for (struct viewport *v = w->viewports; v; v = v->next) {
        // GL2PS was designed to use the legacy GL feedback render
        // mode to retrieve the to-be-drawn geometry.  This no longer
        // works with core profile GL and, although we could implement
        // a similar approach using transform feedback, it is
        // probabaly easier to just project the geometry ourselves.

        // The macro below carries out the standard GL coordinate
        // transformations from object to clip coordinates by applying
        // the viewport's matrix, then by perspective division to NDC
        // and finally to window coordinates via the viewport
        // transformation.

        // See the section titled "Coordinate Transformations" in the
        // OpenGL Core Profile specification for more details.

        const GLfloat o_x = v->left, o_y = v->bottom;
        const GLfloat p_x = v->right - v->left, p_y = v->top - v->bottom;
        const GLfloat s = (v->far - v->near) / 2.0f;
        const GLfloat b = (v->near + v->far) / 2.0f;

#define PROJECT(I)                                                      \
        {                                                               \
            const GLfloat *v_ = p + 7 * I, *M_ = v->matrix;             \
            const GLfloat w_ =                                          \
                M_[12] * v_[0] + M_[13] * v_[1] + M_[14] * v_[2] + M_[15]; \
            GLfloat *u_ = vertices[I].xyz;                              \
                                                                        \
            u_[0] = (                                                   \
                (M_[0] * v_[0] + M_[1] * v_[1] + M_[2] * v_[2] + M_[3]) \
                / w_ + 1.0f) * p_x / 2.0f + o_x;                        \
            u_[1] = (                                                   \
                (M_[4] * v_[0] + M_[5] * v_[1] + M_[6] * v_[2] + M_[7]) \
                / w_ + 1.0f) * p_y / 2.0f + o_y;                        \
            u_[2] = (                                                   \
                (M_[8] * v_[0] + M_[9] * v_[1] + M_[10] * v_[2] + M_[11]) \
                / w_ * s + b);                                          \
        }

        //   If selected, we first draw the viewport frames.  We draw
        //   these on the far plane.  Although it should not matter,
        //   it seems to throw off GL2PS's depth sorting if it's not
        //   behind all geometry.

        if (settings.print_frames) {
            GL2PSvertex v[] = {
                {{o_x, o_y, s + b}, {0.0f, 0.0f, 0.0f, 0.0f}},
                {{o_x + p_x, o_y, s + b}, {0.0f, 0.0f, 0.0f, 0.0f}},
                {{o_x + p_x, o_y + p_y, s + b}, {0.0f, 0.0f, 0.0f, 0.0f}},
                {{o_x, o_y + p_y, s + b}, {0.0f, 0.0f, 0.0f, 0.0f}},
                {{o_x, o_y, s + b}, {0.0f, 0.0f, 0.0f, 0.0f}}};

            for (size_t i = 0; i < 4; i++) {
                gl2psAddPolyPrimitive(
                    GL2PS_LINE, 2, v + i,
                    0, 1.0f, 1.0f,
                    0xffff, 1,
                    1.0f,
                    GL2PS_LINE_CAP_ROUND,
                    GL2PS_LINE_JOIN_MITER,
                    0);
            }
        }

        struct object *o = v->object;

        if (!o) {
            continue;
        }

        // Before transforming the object, we need to make sure the
        // viewport's projection matrix is up-to-date, since such
        // print commands are likely to be part of batch jobs, so that
        // the window might not have been presented yet.

        if (v->stale.projection) {
            refresh_viewport(v);
            v->stale.projection = false;
        }

        // If there are more than one viewports in the window, we
        // begin a new viewport for each, otherwise contents from one
        // viewport will be able to spill over into the others.

        if (w->viewports->next) {
            safely_assert(
                gl2psBeginViewport((GLint []){o_x, o_y, p_x, p_y})
                == GL2PS_SUCCESS);
        }

        // We now map the object's VBO and transform its vertices (and
        // also simply copy the vertex colors).

        GL2PSvertex vertices[o->counts[0]];

        glBindBuffer(GL_ARRAY_BUFFER, o->vbo);

        {
            GLfloat *p = glMapBuffer(GL_ARRAY_BUFFER, GL_READ_ONLY);

            for (GLsizei i = 0; i < o->counts[0]; i++) {
                PROJECT(i);
                memcpy(vertices[i].rgba, &p[7 * i + 3], 4 * sizeof(GLfloat));
            }
        }

        glUnmapBuffer(GL_ARRAY_BUFFER);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

#undef PROJECT

        // Since we can't handle transparency very well anyway^[Only
        // the SVG and PDF formats support it, the latter not too
        // well], we instead use it to request that the edges be
        // stippled more or less densly, according to the alpha value.
        // We also leave triangles with the same color as the edge
        // color unfilled by convention, unless they have a zero alpha
        // value.  We use this to draw plain lines in digrams.

        // For this purpose, we set up the stipple patterns below,
        // with 1 - 16 evenly spaced bits set.

        GLushort patterns[16] = {};

        for (int i = 1; i <= 16; i++) {
            for (int j = 1; j <= i ; j++) {
                patterns[i - 1] |= (1 << ((int)(round(16.0 / i * j)) - 1));
            }
        }

        // We're now ready to map the EBO and:

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, o->ebo);

        {
            //   1. assemble and draw the triangles that make up the
            //   object^[These will be filled wihtout a border, so we
            //   needn't worry about the fact that they're potentially
            //   triangulations of larger polygons.], unless they're
            //   "transparent"^[We need only support the case where
            //   the whole shape has uniform alpha value, so we just
            //   look at the first vertex.], followed by

            GL2PSvertex v[3];
            GLuint *p = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_READ_ONLY);

            for (GLsizei i = 0; i < o->counts[1]; i++) {
                memcpy(v + (i % 3), vertices + p[i], sizeof(GL2PSvertex));

                if (i % 3 == 0
                    && v[0].rgba[0] == settings.edge_color[0]
                    && v[0].rgba[1] == settings.edge_color[1]
                    && v[0].rgba[2] == settings.edge_color[2]
                    && v[0].rgba[3] > 0) {
                     i += 2;
                     continue;
                }

                if (i % 3 == 2) {
                    gl2psAddPolyPrimitive(
                        GL2PS_TRIANGLE, 3, v,
                        1, 1.0f, 1.0f,
                        0xffff, 1,
                        1.0f,
                        GL2PS_LINE_CAP_BUTT,
                        GL2PS_LINE_JOIN_MITER,
                        0);
                }
            }

            //   2. the possibly stippled edges, drawn in the edge
            //   color.

            const GLuint *q = p + o->counts[1];
            for (GLsizei i = 0; i < o->counts[2]; i++) {
                const GL2PSvertex *v_qi = vertices + q[i];
                memcpy(&v[i % 2].xyz, v_qi, sizeof(GL2PSxyz));
                memcpy(
                    &v[i % 2].rgba,
                    (GL2PSrgba) {
                        (GLfloat)settings.edge_color[0],
                        (GLfloat)settings.edge_color[1],
                        (GLfloat)settings.edge_color[2],
                        (GLfloat)settings.edge_color[3]
                    },
                    sizeof(GL2PSrgba));

                if (i % 2 == 1) {
                    gl2psAddPolyPrimitive(
                        GL2PS_LINE, 2, v,
                        0, 0.0f, 0.0f,
                        patterns[(int)round(v_qi->rgba[3] * 16.0f) - 1], 1,
                        1.0f,
                        GL2PS_LINE_CAP_ROUND,
                        GL2PS_LINE_JOIN_MITER,
                        0);
                }
            }
        }

        // We can now clean up and move on the the next viewport.

        glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        safely_assert(gl2psEndViewport() == GL2PS_SUCCESS);
    }

    unlock_window(w);

    // We expect no errors here, but `GL2PS_NO_FEEDBACK` can be
    // returned when printing an empty window.  We're ok with that.

    {
#ifndef NDEBUG
        const GLint i =
#endif
            gl2psEndPage();

        assert(i == GL2PS_SUCCESS || i == GL2PS_NO_FEEDBACK);
    }
}
