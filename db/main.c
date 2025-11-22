#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

#include <readline/readline.h>
#include <readline/history.h>

#include <getopt.h>

#include "common.h"

#define VERSION_NUMBER "0.1.0"

// ---

// # The Debugger

// The debugger allows visualization and inspection of intermediate
// and final results.  It also affords what some may find to be a more
// convenient way to carry out the developemnt of a computation in
// general.

// It is designed to outwardly resemble a typical command-line
// debugger, although its actual operation differs substantially.  It
// allows inspection of geometry through one or more windows, which
// may be further subdivided into rectangular subsections, called
// viewports.  The geometry in question can be loaded from files on
// disk or from the compiler via IPC channels, such as UNIX domain
// sockets.

GLuint compile_shader(GLenum type, const char *source)
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

GLuint create_program(GLuint vertex, GLuint fragment)
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

static void error_callback(int error, const char *description)
{
    fprintf(stderr, "GLFW error: %s\n", description);
}

// ## Command Line Options

enum {
    VERSION = 1000,
};

static struct {
    int no_init;
    int batch;
} flags;

static struct option options[] = {
    {"help", no_argument, nullptr, 'h'},
    {"version", no_argument, nullptr, VERSION},

    {"no-init", no_argument, &flags.no_init, 1},
    {"batch", no_argument, &flags.batch, 1},
    {"command", required_argument, nullptr, 'c'},
    {"execute", required_argument, nullptr, 'x'},

    {nullptr, 0, nullptr, 0}
};

volatile bool running = true;

// ## Input on the Terminal

// When evaluating commands from strings (from the terminal or command
// line switches), we wrap them in `FILE *`.  This allows us to use
// our existing parsing machinery without change.

static int evaluate(char *s)
{
    FILE *fp = fmemopen(s, strlen(s), "r");
    const int n = read_commands(fp);
    fclose(fp);

    return n;
}

// Executing commands from files is more straightforward.

static int execute(char *name)
{
    FILE *fp = strcmp(name, "-") ? fopen(name, "r") : stdin;

    if (fp) {
        const int n = read_commands(fp);

        if (fp != stdin) {
            fclose(fp);
        }

        return n;
    }

    return -1;
}

// ### Completion

// The functions below implement completion via GNU Readline.  Below,
// we provide completion candidates given a set of keywords that are
// relevant in the current context.

static char **completion_candidates;

static char *completion_generator(const char *text, int state)
{
    static int i, n;

    if (!state) {
        i = 0;
        n = strlen(text);
    }

    for(char *s; (s = completion_candidates[i++]);) {
        if (!strncmp(s, text, n)) {
            return strdup(s);
        }
    }

    return nullptr;
}

// These macros facilitate choosing a set of completion candidates
// given the input so far.  (They only allow completing a keyword
// argument when it is the first argument of the command, but that's
// all we need.)

#define TRY_COMPLETION(CMD, ...)                                        \
    {                                                                   \
        int i_;                                                         \
                                                                        \
        for (i_ = 0; isspace(rl_line_buffer[i_]) && i_ < start; i_++);  \
                                                                        \
        const size_t n_ = start - i_, m_ = strlen(CMD);                 \
        if ((m_ == 0 && n_ == 0)                                        \
            || (n_ > 0 && m_ > 0                                        \
                && !strncmp(                                            \
                    rl_line_buffer + i_,                                \
                    CMD,                                                \
                    n_ > m_ ? m_ : n_)))                                \
            __VA_ARGS__                                                 \
    }                                                                   \

#define TRY_ARG_COMPLETION(CMD, ...)                                    \
    TRY_COMPLETION(                                                     \
        CMD, {                                                          \
            for (i_ += m_;                                              \
                 isspace(rl_line_buffer[i_]) && i_ < start;             \
                 i_++);                                                 \
                                                                        \
            if (i_ == start) {                                          \
                completion_candidates = __VA_ARGS__;                    \
                return rl_completion_matches(                           \
                    text, completion_generator);                        \
            } else {                                                    \
                return nullptr;                                         \
            }                                                           \
        })

// Completions can be carried out in various contexts:

static char **completion_function(const char *text, int start, int end)
{
    rl_attempted_completion_over = 1;

    //   1. When at the beginning of the line (ignoring potential
    //   whitespace), when we complete the command, or

    TRY_ARG_COMPLETION(
        "", (char *[]) {
            "quit", "exit", "window", "hide", "present", "resize", "focus",
            "split", "target", "rotate", "translate", "pan", "zoom", "view",
            "load", "run", "info", "set", "show", nullptr});

    //   2. when completing keyword arguments for certain commands, or

    TRY_ARG_COMPLETION(
        "split", (char *[]) {"horizontally", "vertically", nullptr});
    TRY_ARG_COMPLETION(
        "view", (char *[]) {"orthographic", "perspective", nullptr});
    TRY_ARG_COMPLETION("run", (char *[]) {"single", "all", nullptr});

    TRY_ARG_COMPLETION(
        "info", (char *[]) {"windows", "viewports", "objects", nullptr});

    char *v[] = {
        "program", "args", "default-color", "mouse-sensitivity", nullptr};

    TRY_ARG_COMPLETION("set", v);
    TRY_ARG_COMPLETION("show", v);

    //   3. when specifying files to read from,

    TRY_COMPLETION(
        "load", {
            for(int i = start - 1; i > 0; i--) {
                if (isspace(rl_line_buffer[i])) {
                    continue;
                }

                if (rl_line_buffer[i] == '<') {
                    return rl_completion_matches(text, rl_filename_completion_function);
                } else {
                    return nullptr;
                }
            }
        });

    return nullptr;
}

#undef TRY_COMPLETION
#undef TRY_ARG_COMPLETION

// ### Reading Terminal Input

// We want to allow the user to enter commands on the terminal.  This
// is simple enough when using GNU Readline to provide all the
// requisite comforts, except for one point: Since we need the main
// thread for graphical interaction (GLFW doesn't allow calling most
// of its functions from other threads), we need to handle terminal
// input in another thread.  This is straightforward, until it's time
// to quit.  See ref: The Main Function, below for how this is
// handled.

static void *do_input(void *arg)
{
    rl_attempted_completion_function = completion_function;

    char *s = nullptr;
    while (running && (s = readline("# "))) {
        if (s[0] != '\0') {
            add_history(s);

            evaluate(s);
        }

        free(s);
    }

    assert(!running || s == nullptr);

    // Should this thread exit, which should only happen if the user
    // quits via @kbd{Ctrl-d}, we want to exit the whole application.

    running = false;
    glfwPostEmptyEvent();

    return nullptr;
}

// ## IPC

// We want to be able to read geometry and commands from outside, so
// we set up an IPC channel.  As for terminal input, we need to do so
// while displaying graphics and can't afford to block while waiting
// for the other end, so we handle IPC in its own thread too.

static void *do_ipc(void *arg)
{
    // When receiving a `SIGTERM` we don't want to print an error
    // message.

#define EXIT(MSG)                               \
    do {                                        \
        if (errno != EINTR || running) {        \
            perror(MSG);                        \
        }                                       \
        goto exit;                              \
    }                                           \
    while (false)

    // We use a UNIX domain socket to listen for incoming connections
    // on.

    const int listen_socket = socket(AF_UNIX, SOCK_STREAM, 0);

    if (listen_socket == -1) {
        perror("Failed to create listening socket");
        return nullptr;
    }

    // We bind the socket to an abstract address, to spare ourselves
    // the need to clean up files on the filesystem.

    struct sockaddr_un addr;

#define NAME "inspector"
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path + 1, NAME);

    if (bind(listen_socket,
             (const struct sockaddr *)&addr,
             offsetof(struct sockaddr_un, sun_path) + sizeof(NAME)) == -1) {
        EXIT("Failed to bind listening socket");
    }
#undef NAME

    // We can now listen for incoming connections.

    if (listen(listen_socket, 20) == -1) {
        EXIT("Failed to listen on socket");
    }

    while (running) {
        // First we wait for one.

        const int data_socket = accept(listen_socket, nullptr, nullptr);

        if (data_socket == -1) {
            EXIT("Failed to accept connection");
            goto exit;
        }

        // For each connection we accept it and parse the received
        // commands.  We wrap the data socket inside a `FILE *` to let
        // the C library handle I/O on the file.

        FILE *fp = fdopen(data_socket, "r");

        read_commands(fp);

        close(data_socket);
    }

exit:
    close(listen_socket);

    // This thread shouldn't exit unless we're already in the process
    // of shutting down, but should it die, we want to exit the whole
    // application.  No point in continuing, if we cant' udpate
    // geomtery.

    running = false;
    glfwPostEmptyEvent();

    return nullptr;
}

#undef EXIT

// ## The Main Function

// First we define a signal handler.  See below for how it's used.

static void signal_handler(int sig)
{
    running = false;
    glfwPostEmptyEvent();
}

int main(int argc, char *argv[])
{
    // We begin by initializing the GLFW.

    glfwSetErrorCallback(error_callback);

    if (!glfwInit()) {
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);

    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 16);
#ifdef DEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
#endif
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // We will have more to say about signal handling below, but for
    // now, we need to set up a handler that will flip the `running`
    // flag and cause the application to exit.  We assign it to the
    // TERM and USR1 signals.  The former is meant to handle the case
    // where the TERM signal is sent to our process externally (as
    // with `kill -TERM`) and will be caught by the input thread (as
    // it's blocked on all others; see below) causing it to exit along
    // with the rest of the application.

    // Using the same handler for `USR1` signals^[The `USR1` signal is
    // chosen because Readline doesn't interfere with it.] allows us
    // to raise this signal from anywhere, if we wish to quit the
    // application.  This is also the reason we need to set up the
    // handler early, since a quit command might appear in an init
    // file, or as argument to the `-c` switch.

    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGUSR1, &sa, nullptr);

    int n, option;
    while ((n = -1, option = getopt_long(
                argc, argv,
                "-hc:x:",
                options, &n)) != -1) {
        switch (option) {

        case VERSION:
            puts("Gamma Debugger " VERSION_NUMBER "\n"
                 "Copyright (C) 2025 Dimitris Papavasiliou.\n\n"

                 "This program is free software; you can redistribute it and/or modify\n"
                 "it under the terms of the GNU General Public License as published by\n"
                 "the Free Software Foundation; either version 3 of the License, or\n"
                 "(at your option) any later version.\n\n"

                 "This program is distributed in the hope that it will be useful,\n"
                 "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
                 "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
                 "GNU General Public License for more details.\n\n"

                 "You should have received a copy of the GNU General Public License\n"
                 "along with this program. If not, see http://www.gnu.org/licenses/.");

            exit(EXIT_SUCCESS);

        case 'h':
            printf("Usage: %s [OPTION...]\n\n"
                   "Options:\n"
                   "  -h, --help            Display this help message.\n"
                   "  --version             Display version information.\n\n"

                   "  -c COMMAND, --command=COMMAND\n"
                   "                        Execute a single command.  May be used\n"
                   "                        multiple times.\n"
                   "  -x FILE, --execute=FILE\n"
                   "                        Execute commands from a file.  May be\n"
                   "                        used multiple times.\n"
                   "  --no-init             Do not read initialization files.\n"
                   "  --batch               Exit after processing options.\n",
                   argv[0]);

            exit(EXIT_SUCCESS);

        case 'c':
            if (evaluate(optarg) < 0) {
                exit(EXIT_FAILURE);
            }

            break;

        case 'x':
            if (execute(optarg) < 0) {
                perror("Could not execute init file");
                exit(EXIT_FAILURE);
            }

            break;

        case '?':
            exit(EXIT_FAILURE);
        }
    }

    if (flags.batch) {
        exit(EXIT_SUCCESS);
    }

    // We read in the local init file, if it exists, and execute any
    // commands in it.

    if (!flags.no_init) {
        if (execute(".gammadbinit") < 0) {
            if (errno != ENOENT) {
                perror("Could not execute command file");
                exit(EXIT_FAILURE);
            }
        }
    }

    // We setup signal handling and create threads, starting with the
    // thread for terminal input.

    pthread_t threads[2] = {};

    if (pthread_create(&threads[1], nullptr, do_input, nullptr)) {
        perror("Failed to create input thread\n");
    }

    // One gets the idea that Readline was not designed with
    // multi-threaded applications in mind.  Since it sets up its own
    // signal handling and since signal disposition is a per-process
    // attribute^[In a multithreaded application, the disposition of a
    // particular signal (whether it's ignored, handled, etc.)  is the
    // same for all threads] we might have Readline's signal handler
    // invoked by another thread.  This may be fine, but Readline
    // seems to intefere with signal handling, so we opt for the
    // conservative approach:

    // We block all signals (apart from USR1) on all but the input
    // thread, making Readline solely in charge of handling them.

    sigset_t set;
    sigfillset(&set);
    sigdelset(&set, SIGUSR1);
    pthread_sigmask(SIG_SETMASK, &set, nullptr);

    if (pthread_create(&threads[0], nullptr, do_ipc, nullptr)) {
        perror("Failed to create IPC thread\n");
    }

    // Now we run the main loop and clean up when we've determined
    // that it's time to quit.

    while (running) {
        refresh_windows();
        glfwWaitEvents();
    }

    // We want to signal our threads to clean up properly, before we
    // join them.

    // The input thread will typically be blocked, with Readline
    // waiting to read the next command.  Although not explicitly
    // stated in Readline's documentation, based on Readline sources,
    // handling of "terminal" signals, like TERM or HUP, will result
    // in the blocked call to `readline` exit returning `nullptr`.

    if (threads[1]) {
        pthread_kill(threads[1], SIGTERM);
        pthread_join(threads[1], nullptr);
    }

    // For the IPC thread, the signal will result in the
    // socket-related calls failing with `EINTR`.  We exit when that
    // happens.

    if (threads[0]) {
        pthread_kill(threads[0], SIGUSR1);
        pthread_join(threads[0], nullptr);
    }

    glfwTerminate();

    return 0;
}
