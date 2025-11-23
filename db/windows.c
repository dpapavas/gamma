#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "common.h"
#include SHADER_STRINGS

// ---

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
                v && (v->left > x || v->right < x || v->bottom > y_1 || v->top < y_1)) {
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
                pan_viewport(
                    w->focus,
                    -c * (GLfloat)(x - previous_x),
                    c * (GLfloat)(y - previous_y));
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
    // printf("%d, %d\n", button, action);
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

#ifdef DEBUG
static void APIENTRY debug_message_callback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar *message,
    const void *userParam)
{
    fprintf(stderr, "GL ");

    switch (severity) {
    case GL_DEBUG_SEVERITY_NOTIFICATION:
        fprintf(stderr, "note");
        break;
    case GL_DEBUG_SEVERITY_LOW:
        fprintf(stderr, "warning");
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        fprintf(stderr, "severe warning");
        break;
    case GL_DEBUG_SEVERITY_HIGH:
        fprintf(stderr, "error");
        break;
    }

    fprintf(stderr, ": %s\n", message);
}
#endif

// ## GLSL Program State

// We use a structure to keep track of the GL name and uniform
// locations for each program we create.

// The uniform program fills each pixel with a uniform color.  It's
// mostly useful for drawing the UI elements.

struct {
    GLuint name;

    GLuint matrix, color;
} uniform;

// The `flat` program is for flat shading with per-vertex colors.

struct {
    GLuint name;

    GLuint matrix, intensity;
} flat;

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
        fprintf(stderr, "Shader compilation error:\n%s\n", s);
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
        fprintf(stderr, "Program linking error:\n%s\n", s);
    }

    return i;
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
        1,
        strdup(DEFAULT_VIEWPORT_NAME),
        true,
        0, width - 1, 0, height - 1,
        PERSPECTIVE, 0.1f, 100.0f,
        DEFAULT_VIEWPORT_ANGLE / 2.0f / 180.0f * M_PI,
        DEFAULT_VIEWPORT_ZOOM,
        {0.0f, 0.0f, 0.0f},
        EYE, EYE,
        0,
        nullptr,
        nullptr
    };

#undef EYE

    // Now, we create the actual window.  If this is not the first
    // window we share the previously created window's context.  This
    // establishes a chain of shared contexts so that any window can
    // use buffer objects created in any other.

    w->window = glfwCreateWindow(
        width, height, name, nullptr, w->next ? w->next->window : nullptr);

    if (!w->window) {
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
        fprintf(stderr, "Failed to initialize GLAD\n");
        exit(EXIT_FAILURE);
    }

    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);

    glEnable(GL_MULTISAMPLE);

    glPolygonOffset(1.0f, 1.0f);

    // If enabled, we set up a debug message callback, to be notified
    // about any GL errors.

#ifdef DEBUG
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
        // A simple uniformly coloring shader used to draw the
        // inteface or other flat shaded geometry.

        {
            assert(!uniform.name);

            const GLuint i = compile_shader(GL_VERTEX_SHADER, uniform_vertex_c);
            const GLuint j = compile_shader(GL_FRAGMENT_SHADER, uniform_fragment_c);

            uniform.name = create_program(i, j);

            glDeleteShader(i);
            glDeleteShader(j);

            uniform.matrix = glGetUniformLocation(uniform.name, "matrix");
            uniform.color = glGetUniformLocation(uniform.name, "color");
        }

        // A flat shader with per-vertex colors, used to draw objects.

        {
            assert(!flat.name);

            const GLuint i = compile_shader(GL_VERTEX_SHADER, flat_vertex_c);
            const GLuint j = compile_shader(GL_FRAGMENT_SHADER, flat_fragment_c);

            flat.name = create_program(i, j);

            glDeleteShader(i);
            glDeleteShader(j);

            flat.matrix = glGetUniformLocation(flat.name, "matrix");
            flat.intensity = glGetUniformLocation(flat.name, "intensity");
        }
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
        v->stale = true;
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

    for (struct viewport *v = w->viewports; v; v = v->next) {
        // Redraw the background and frame of the viewport.  This
        // consists in drawing a unit quad, first filled and then as a
        // line loop, scaled so that its edges run through the middle
        // of the pixels on the edge of the viewport.

        // This is done with the matrix below, along with the
        // following viewport specification.

        const GLfloat a = v->right - v->left;
        const GLfloat b = v->top - v->bottom;

        const GLfloat S[16] = {
            (a - 1) / a, 0.0f, 0.0f, 0.0f,
            0.0f, (b - 1) / b, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };

        glViewport(v->left, v->bottom, (GLsizei)a, (GLsizei)b);

        // Now we bind the uniform color program and draw the quads.

        glUseProgram(uniform.name);

        // This is a workaround around a Mesa driver bug.  See:
        // https://gitlab.freedesktop.org/mesa/mesa/-/issues/14129

        {
            const GLfloat M[16] = {};
            glUniformMatrix4fv(uniform.matrix, 1, GL_TRUE, M);
            glUniform4f(uniform.color, 0.0f, 0.0f, 0.0f, 0.0f);
        }

        glDisable(GL_DEPTH_TEST);
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

        // We can now draw the viewport's geometry, assuming it has
        // some.

        if (v->vao) {
            assert(v->object);

            // First, we recalculate the viewport's projection if
            // needed.

            if (v->stale) {
                refresh_viewport(v);
                v->stale = false;
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

            //   2. the edges, draw as a sequence of line segmenta and
            //   finally,

            glUseProgram(uniform.name);

            glUniformMatrix4fv(uniform.matrix, 1, GL_TRUE, v->matrix);
            glUniform4f(uniform.color, 1.0f, 0.0f, 0.0f, 1.0f);

            glDrawElements(
                GL_LINES, counts[2], GL_UNSIGNED_INT,
                (void *)(counts[1] * sizeof(GLuint)));

            //   3. the vertices, drawn as points.

            glUniform4f(uniform.color, 0.0f, 0.0f, 1.0f, 1.0f);
            glPointSize(5);
            glDrawArrays(GL_POINTS, 0, counts[0]);
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

            // Having calculated the object's AABB, we can
            // recenter the viewport.

            translate_viewport(v, NAN, NAN, NAN);

            // Finally we make sure the window found to be showing the
            // object is visible and updated.

            glfwShowWindow(w->window);
            glfwPostEmptyEvent();
        }
    }
}
