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

static char **completion_candidates, *suppression_characters;

static char *completion_generator(const char *text, int state)
{
    static int i, n;

    if (!state) {
        i = 0;
        n = strlen(text);
    }

    for(char *s; (s = completion_candidates[i++]);) {
        if (!strncmp(s, text, n)) {
            rl_completion_suppress_append = (
                state == 0
                && suppression_characters
                && strchr(suppression_characters, s[strlen(s) - 1]));

            return strdup(s);
        }
    }

    return nullptr;
}

// These macros facilitate choosing a set of completion candidates
// given the input so far.  (They only allow completing a keyword
// argument when it is the first argument of the command, but that's
// all we need.)

#define WHEN_IN(CMD, ...)                                               \
    do {                                                                \
        int i_, j_;                                                     \
                                                                        \
        /* Find the beginning of the last word `j_`. */                 \
                                                                        \
        for (j_ = rl_point;                                             \
             j_ > 0 && !strchr(                                         \
                 rl_completer_word_break_characters,                    \
                 rl_line_buffer[j_ - 1]);                               \
             j_--);                                                     \
                                                                        \
        /* Find the beginning of the first word `j_`. */                \
                                                                        \
        for (i_ = 0; i_ < j_ && strchr(                                 \
                 rl_completer_word_break_characters,                    \
                 rl_line_buffer[i_]);                                   \
             i_++);                                                     \
                                                                        \
        /* Test whether the first word is exactly `CMD` and if */       \
        /* there'sonly space from there to the last word, i.e. if */    \
        /* the last word is the first argument of CMD. */               \
                                                                        \
        const size_t n_ = j_ - i_, m_ = strlen(CMD);                    \
        if ((m_ == 0 && n_ == 0)                                        \
            || (m_ > 0 && n_ > m_                                       \
                && !strncmp(                                            \
                    rl_line_buffer + i_,                                \
                    CMD,                                                \
                    n_ > m_ ? m_ : n_)))                                \
            __VA_ARGS__                                                 \
    } while(false)

#define WHEN_IN_1(CMD, ...) WHEN_IN(CMD, {              \
    for (i_ += m_;                                      \
         i_ < j_ && strchr(                             \
             rl_completer_word_break_characters,        \
             rl_line_buffer[i_]) ;                      \
         i_++);                                         \
                                                        \
    if (i_ == j_) {                                     \
        __VA_ARGS__                                     \
    } else {                                            \
        return nullptr;                                 \
    }                                                   \
})

#define MATCHES(SUPP, ...)                                      \
    (suppression_characters = SUPP,                             \
        completion_candidates = __VA_ARGS__,                    \
        rl_completion_matches(text, completion_generator))

#define WORD_BREAK_CHARACTERS " \t<"

// We need to change the word break characters when completing key
// names, because we want to be able to complete modifers and keys
// separately.  We therefore add the separating `-` to the break
// characters.

static char *word_break_hook(void)
{
    WHEN_IN_1(
        "bind", {
            return WORD_BREAK_CHARACTERS "-";
        });

    return nullptr;
}

// Completions can be carried out in various contexts:

static char **completion_function(const char *text, int start, int end)
{
    rl_attempted_completion_over = 1;
    suppression_characters = nullptr;

    //   1. When at the beginning of the line (ignoring potential
    //   whitespace), when we complete the command, or

    WHEN_IN_1("", {
        return MATCHES(
            nullptr,
            (char *[]) {
                "quit", "exit", "window", "hide", "present", "resize", "focus",
                "split", "target", "rotate", "translate", "pan", "zoom", "view",
                "load", "run", "info", "set", "show", "bind", "unbind",
                nullptr});
    });

    //   2. when completing keyword arguments for certain commands, or

    WHEN_IN_1("split", {
        return MATCHES(
            nullptr,
            (char *[]) {"horizontally", "vertically", nullptr});
    });
    WHEN_IN_1("view", {
        return MATCHES(
            nullptr,
            (char *[]) {"orthographic", "perspective", "toggle", nullptr});
    });
    WHEN_IN_1("run", {
        return MATCHES(nullptr, (char *[]) {"single", "all", nullptr});
    });

    WHEN_IN_1("info", {
        return MATCHES(
            nullptr,
            (char *[]) {"windows", "viewports", "objects", "bindings", nullptr});
    });

    char *v[] = {
        "program", "args", "default-color", "mouse-sensitivity", nullptr};

    WHEN_IN_1("set", {
        return MATCHES(nullptr, v);
    });

    WHEN_IN_1("show", {
        return MATCHES(nullptr, v);
    });

    //   3. when specifying files to read from, or finally

    WHEN_IN(
        "load", {
            bool p = false;
            for(int i = start - 1; i > 0; i--) {
                if (rl_line_buffer[i] == '<') {
                    p = true;
                    continue;
                }

                if (strchr(
                        rl_completer_word_break_characters,
                        rl_line_buffer[i])) {
                    if (p) {
                        return rl_completion_matches(
                            text, rl_filename_completion_function);
                    }

                    continue;
                }

                return nullptr;
            }
        });

    //   4. when entering the key in `bind` commands.

    #include "keys.h"

    char *u[sizeof(modifier_keys) / sizeof(modifier_keys[0])
            + sizeof(function_keys) / sizeof(function_keys[0])
            + 1], **p = u;

    for (size_t i = 0; i < sizeof(modifier_keys) / sizeof(modifier_keys[0]); i++) {
        *p++ = modifier_keys[i].name;
    }

    for (size_t i = 0; i < sizeof(function_keys) / sizeof(function_keys[0]); i++) {
        *p++ = function_keys[i].name;
    }

    *p = nullptr;

    WHEN_IN_1("bind", {
            return MATCHES("-", u);
        });

    return nullptr;
}

#undef WHEN_IN_1
#undef WHEN_IN
#undef MATCHES

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
    rl_basic_word_break_characters = WORD_BREAK_CHARACTERS;
    rl_completion_word_break_hook = word_break_hook;

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

#undef WORD_BREAK_CHARACTERS

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
