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

// Document: program

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
    print_error("GLFW error: %s\n", description);
}

// ## Command Line Options

enum {
    VERSION = 1000,
    NO_INIT,
    BATCH,
    ARGS,
    TARGET
};

static struct option options[] = {
    {"help", no_argument, nullptr, 'h'},
    {"version", no_argument, nullptr, VERSION},

    {"quiet", no_argument, nullptr, 'q'},
    {"no-init", no_argument, nullptr, NO_INIT},
    {"batch", no_argument, nullptr, BATCH},
    {"args", no_argument, nullptr, ARGS},
    {"target", required_argument, nullptr, TARGET},
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
                "load", "run", "info", "set", "show", "bind", "unbind", "print",
                "define", "undefine", nullptr});
    });

    //   2. completing keyword arguments for certain commands, or

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
            (char *[]) {
                "windows", "viewports", "objects", "bindings",
                "definitions", nullptr});
    });

    {
        char *v[] = {
            "program", "args", "quiet", "present-on-reload", "resize-on-split",
            "print-frames", "default-color", "default-view", "default-zoom",
            "default-rotation", "default-translation", "edge-color",
            "mouse-sensitivity", nullptr};

        WHEN_IN_1("set", {
                return MATCHES(nullptr, v);
            });

        WHEN_IN_1("show", {
                return MATCHES(nullptr, v);
            });
    }

    //   3. files to read from, or

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

                break;
            }
        });

    //   4. the key in `bind` commands,

#include "keys.h"

    WHEN_IN_1(
        "bind", {

            char *v[sizeof(modifier_keys) / sizeof(modifier_keys[0])
                    + sizeof(function_keys) / sizeof(function_keys[0])
                    + 1], **p = v;

            for (size_t i = 0;
                 i < sizeof(modifier_keys) / sizeof(modifier_keys[0]); i++) {
                *p++ = modifier_keys[i].name;
            }

            for (size_t i = 0;
                 i < sizeof(function_keys) / sizeof(function_keys[0]); i++) {
                *p++ = function_keys[i].name;
            }

            *p = nullptr;

            return MATCHES("-", v);
        });

    //   5. the name in `window` commands,

    WHEN_IN_1(
        "window", {
            size_t n = 0;
            for (struct window *w = windows; w; w = w->next, n++);

            char *v[n + 1], **p = v;

            for (struct window *w = windows; w;
                 *p++ = (char *)w->name,w = w->next);

            *p = nullptr;

            return MATCHES(nullptr, v);
        });

    //   6. the names in `target` and `load` commands.

    {
        size_t n = 0;

        for (struct window *w = windows; w; w = w->next, n++) {
            for (struct viewport *v = w->viewports; v; v = v->next) {
                n += (v->name[0] != '\0');
            }
        }

        for (struct object *o = objects; o; o = o->next) {
            n += (o->name[0] != '\0');
        }

        char *v[n + 1], **p = v;

        for (struct window *w = windows; w; w = w->next, n++) {
            for (struct viewport *v = w->viewports; v; v = v->next) {
                if (v->name[0] != '\0') {
                    *p++ = (char *)v->name;
                }
            }
        }

        for (struct object *o = objects; o; o = o->next) {
            if (o->name[0] != '\0') {
                *p++ = (char *)o->name;
            }
        }

        *p = nullptr;

        WHEN_IN_1("target", {
                return MATCHES(nullptr, v);
            });

        WHEN_IN_1("load", {
                return MATCHES(nullptr, v);
            });
    }

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
// to quit.  See ref: The Debugger Main Function, below for how this
// is handled.

static void *do_input(void *arg)
{
    // We've inherited the signal mask from the main thread, which has
    // almsot all signals blocked.  Since we're supposed to be in
    // charge of signal handling, we need to reset it.

    sigset_t set;
    sigemptyset(&set);
    pthread_sigmask(SIG_SETMASK, &set, nullptr);

    rl_attempted_completion_function = completion_function;
    rl_basic_word_break_characters = WORD_BREAK_CHARACTERS;
    rl_completion_word_break_hook = word_break_hook;

    char *s = nullptr;
    while (running && (s = readline("# "))) {
        char *t;
        for (t = s; isspace(*t); t++);

        // If the line entered by the user is empty, we repeat the
        // last command.  GNU History's manual doesn't seem to be very
        // clear here.  It says:

        //   > The range of valid values of offset starts at
        //   > history_base and ends at history_length - 1

        // Looking at the implementation of `history_get`, this should
        // probably read "at history_base + history_length - 1".

        if (*t == '\0') {
            if (history_length > 0) {
                evaluate(history_get(history_base + history_length - 1)->line);
            }
        } else {
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

#define EXIT(MSG)                                       \
    do {                                                \
        if (errno != EINTR || running) {                \
            print_error(MSG ": %s\n", strerror(errno)); \
        }                                               \
        goto exit;                                      \
    }                                                   \
    while (false)

    // We use a UNIX domain socket to listen for incoming connections
    // on.

    const int listen_socket = socket(AF_UNIX, SOCK_STREAM, 0);

    if (listen_socket == -1) {
        EXIT("Failed to create listening socket");
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

// ## The Debugger Main Function

// First we need to define a signal handler.  See below for more
// details.

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
#ifndef NDEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
#endif
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // We'll be creating threads, so we'll want to join them before
    // exit, which means we need to set up signal handling.

    // We set up a handler that will flip the `running` flag and cause
    // the application to exit.  We assign it to the TERM and USR1
    // signals.  The former is meant to handle the case where the TERM
    // signal is sent to our process externally (as with `kill -TERM`)
    // and will be caught by the input thread (as it's blocked on all
    // others; see below)^[It may also be caught by the main thread if
    // it arrives during command line option processing.] causing it
    // to exit along with the rest of the application.

    // Using the same handler for `USR1` signals^[The `USR1` signal is
    // chosen because Readline doesn't interfere with it.] allows us
    // to raise this signal from anywhere if and when we wish to quit
    // the application.  This is also the reason we need to set up the
    // handler early, since a quit command might appear in an init
    // file, or as argument to the `-c` switch.

    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGUSR1, &sa, nullptr);

    // Now, one gets the impression that Readline was not designed
    // with multi-threaded applications in mind.  Since it sets up its
    // own signal handling and since signal disposition is a
    // per-process attribute^[In a multithreaded application, the
    // disposition of a particular signal (whether it's ignored,
    // handled, etc.)  is the same for all threads] we might have
    // Readline's signal handler invoked by another thread.  This may
    // be fine, but Readline seems to intefere with signal handling
    // anyway, so we opt for the conservative approach:

    // We block all signals (apart from USR1) on all but the input
    // thread, making Readline solely in charge of handling them.

    sigset_t set;
    sigfillset(&set);
    sigdelset(&set, SIGUSR1);
    pthread_sigmask(SIG_SETMASK, &set, nullptr);

    // We need to spawn the IPC thread early, for the same reasons
    // given for early signal handling setup above.

    pthread_t threads[2] = {};

    if (pthread_create(&threads[0], nullptr, do_ipc, nullptr)) {
        print_error("Failed to create IPC thread: %s\n", strerror(errno));
    }

    int n, option, no_init = 0, batch = 0;
    while ((n = -1, option = getopt_long(
                argc, argv,
                "-hc:x:q",
                options, &n)) != -1) {
        switch (option) {
        case 'h':
            printf("\
Usage: %s [OPTION...]\n\
\n\
Options:\n\
  -h, --help            Display this help message.\n\
  --version             Display version information.\n\
\n\
  -q, --quiet           Do not print any messages to standard output.\n\
  -c COMMAND, --command=COMMAND\n\
                        Execute a single command.  May be used\n\
                        multiple times.\n\
  -x FILE, --execute=FILE\n\
                        Execute commands from a file.  May be\n\
                        used multiple times.\n\
  --no-init             Do not read initialization files.\n\
  --batch               Exit after processing options.\n\
  --args [ARGS ...]     Set the \"args\" option to the following\n\
                        arguments.\n\
  --target TARGET       Create a window on startup and set its main\n\
                        viewport target.\n", argv[0]);

            exit(EXIT_SUCCESS);

        case VERSION:
            print_output("\
Gamma Debugger " VERSION_NUMBER "\n\
Copyright (C) 2025 Dimitris Papavasiliou.\n\
\n\
This program is free software; you can redistribute it and/or modify\n\
it under the terms of the GNU General Public License as published by\n\
the Free Software Foundation; either version 3 of the License, or\n\
(at your option) any later version.\n\
\n\
This program is distributed in the hope that it will be useful,\n\
but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
GNU General Public License for more details.\n\
\n\
You should have received a copy of the GNU General Public License\n\
along with this program. If not, see http://www.gnu.org/licenses/.\n");

            exit(EXIT_SUCCESS);

        case 'q':
            settings.quiet = true;
            break;

        case NO_INIT:
            no_init = 1;
            break;

        case BATCH:
            batch = 1;
            settings.present_on_reload = false;
            break;

        case ARGS:
        {
            size_t n = 0;
            for (int i = optind; i < argc; n += strlen(argv[i++]) + 1);

            settings.args = (char *)malloc(n);
            for (char *s = settings.args;; optind++) {
                s = stpcpy(s, argv[optind]);

                if (optind == argc - 1) {
                    break;
                }

                s = stpcpy(s, " ");
            }
        }

            break;

        case TARGET:
        {
            struct viewport *v = find_window(optarg)->viewports;

            free((char *)v->name);
            v->name = strdup(optarg);
        }
        break;

        case 'c':
            if (evaluate(optarg) < 0) {
                exit(EXIT_FAILURE);
            }

            break;

        case 'x':
        {
            FILE *fp = strcmp(optarg, "-") ? fopen(optarg, "r") : stdin;

            if (fp) {
                if (read_commands(fp) < 0) {
                    exit(EXIT_FAILURE);
                }

                if (fp != stdin) {
                    fclose(fp);
                }
            } else {
                fprintf(
                    stderr, "Could not open file '%s': %s\n",
                    optarg, strerror(errno));
                exit(EXIT_FAILURE);
            }
        }

            break;

        case '?':
            exit(EXIT_FAILURE);
        }
    }

    if (batch) {
        exit(EXIT_SUCCESS);
    }

    // We read in the local init file, if it exists, and execute any
    // commands in it.

    if (!no_init) {
        FILE *fp = fopen(".gammadbinit", "r");

        if (fp) {
            if (read_commands(fp) < 0) {
                exit(EXIT_FAILURE);
            }

            fclose(fp);
        } else if (errno != ENOENT) {
            print_error("Could not open init file: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    // Now that we've established that this is not a batch run, we can
    // go ahead and start the thread for terminal input.

    if (pthread_create(&threads[1], nullptr, do_input, nullptr)) {
        print_error("Failed to create input thread: %s\n", strerror(errno));
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
