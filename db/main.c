#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <poll.h>
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

// ## Input on the Terminal

// When evaluating commands from strings (from the terminal or command
// line switches), we wrap them in `FILE *`.  This allows us to use
// our existing parsing machinery without change.

static int evaluate(const char *s)
{
    FILE *fp = fmemopen((char *)s, strlen(s), "r");
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
    } while (false)

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
                "define", "undefine", "kill", "write", "toggle", nullptr});
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
            "print-frames", "default-vertex-color", "default-view", "default-zoom",
            "default-rotation", "default-translation", "edge-line-width",
            "mouse-sensitivity", "vertex-point-size", nullptr};

        WHEN_IN_1("set", {
                return MATCHES(nullptr, v);
            });

        WHEN_IN_1("show", {
                return MATCHES(nullptr, v);
            });
    }

    WHEN_IN_1("toggle", {
        return MATCHES(
            nullptr,
            (char *[]) {"maximized", "vertices", "edges", "faces", nullptr});
    });

    WHEN_IN_1("resize", {
        return MATCHES(nullptr, (char *[]) {"fullscreen", nullptr});
    });

    //   3. files to read from, or write to, or

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

    WHEN_IN(
        "write", {
            return rl_completion_matches(
                text, rl_filename_completion_function);
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

// ### Printing Output Messages

// When running interactively, we need to make sure we don't print
// over Readline's prompt.  This is only a concern if Readline is
// currently in the process of reading a command, i.e. not when the
// user has finished entering it, in which case `rl_done` will be
// true.

// In batch mode, we can simply print messages immediately.

void begin_print(void)
{
    if (rl_readline_state && !rl_done) {
        rl_clear_visible_line();
    }
}

void end_print(void)
{
    if (rl_readline_state && !rl_done) {
        rl_on_new_line();
        rl_redisplay();
    }
}

static void print(FILE *fp, const char *format, va_list ap)
{
    begin_print();
    vfprintf(fp, format, ap);
    end_print();
}

void print_output(const char *format, ...)
{
    if (settings.quiet) {
        return;
    }

    va_list ap;
    va_start(ap, format);
    print(stdout, format, ap);
    va_end(ap);
}

void print_error(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    print(settings.batch ? stderr : stdout, format, ap);
    va_end(ap);
}

// ### Running the Inferior

// Running the inferior needs to be handled differently, depending on
// whether we're running interactively, or in batch mode.  In either
// case though, we need to fork a new process and execute the given
// command in it.  That's handled below.

static int ipc_socket, run_pipe[2], inferior_pid;

static int fork_inferior(const char *s, bool redirect)
{
    print_output("Running: %s\n", s);

    const pid_t pid = fork();

    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        // When running in quiet mode, we discard output from the
        // inferior, otherwise if running interactively, we redirect
        // output messages to a pipe, to print them in a controlled
        // manner.

        if (settings.quiet) {
            const int fd = open("/dev/null", O_WRONLY);

            if (fd == -1) {
                return -1;
            }

            assert(fd > STDERR_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
        } else if (redirect && (
                close(run_pipe[0]) == -1
                || dup2(run_pipe[1], 1) == -1
                || dup2(run_pipe[1], 2) == -1)) {
            return -1;
        }

        // We give the forked process its own process group, so that
        // we can later send signals to both the shell and the
        // inferior.

        // The alternative would be to send to our own process group
        // after blocking delivery of the signal to ourselves.  This
        // would work fine when we're the process group leader,
        // i.e. we're started directly from the shell.  If instead we
        // were started from a script for instance, we would take it
        // down with us.

        setpgid(0, 0);

        execl("/bin/sh", "sh", "-c", s, (char *)nullptr);

        // The only way to get here, is if `execl` failed, as it
        // otherwise replaces the process image and we are no more.

        return -1;
    }

    inferior_pid = pid;
    return 0;
}

static void empty_handler(int sig)
{
}

// Running can be either:

void run_inferior(const char *s)
{
    //   1. in batch mode, in which case we fork and immediately block
    //   waiting for incoming connections until a `CHLD` signal from
    //   the terminating inferior interrupts us, or

    if (settings.batch) {
        struct sigaction sa, old;
        sa.sa_handler = empty_handler;
        sa.sa_flags = 0;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGCHLD, &sa, &old);

        assert(!inferior_pid || waitpid(inferior_pid, nullptr, WNOHANG) != -1);

        if (fork_inferior(s, false) == -1) {
            goto error;
        }

        while (true) {
            const int fd = accept(ipc_socket, nullptr, nullptr);

            if (fd == -1) {
                if (errno != EINTR) {
                    print_error("Could not connect the IPC socket\n");
                }

                break;
            }

            FILE *fp = fdopen(fd, "r");
            read_commands(fp);
            fclose(fp);
        }

        sigaction(SIGCHLD, &old, nullptr);

        return;
    }

    //   2. when running interactively, in which case we first check
    //   if a run is already in progress.

    switch (waitpid(inferior_pid, nullptr, WNOHANG)) {
    case 0:
        print_error("error: a run is already in progress\n");
        return;

    case -1:
        if (errno != ECHILD || inferior_pid) {
            print_error("error: could not wait on run (%s)\n", strerror(errno));
            return;
        }
    }

    //   If not, we just fork, redirecting output from the inferior to
    //   a pipe.  Communication, either on this pipe or the IPC socket
    //   will be handled in the main loop.

    if (fork_inferior(s, true) == -1) {
      error:
        print_error("error: could not start the run (%s)\n", strerror(errno));
    }
}

int kill_inferior()
{
    if (inferior_pid) {
        return kill(-inferior_pid, SIGTERM);
    }

    return 0;
}

// ### Reading Terminal Input

// We want to allow the user to enter commands on the terminal.  This
// is simple enough when using GNU Readline to provide all the
// requisite comforts, except for a couple of points:

//   1. We need to respond to events from multiple sources, including
//   user input on the terminal, commands from the IPC socket, output
//   from the inferior and user actions from the UI.  The first three
//   can be handled by polling on file descriptors, but the last must
//   be handled via `glfwWaitEvents`.

//   One option would be to alternately poll and wait with a timeout,
//   but that would introduce a latency/CPU waste tradeoff.  Instead
//   we opt for the slightly more complex approach of using a thread;
//   see below.

//   2. Readline needs control of the terminal while reading user
//   input.  On the other hand, we also need to output messages to it,
//   usually after the user has entered a command, but perhaps also
//   asynchronously, from commands read via IPC, or from key bindings,
//   or, finally, from the inferior.

//   We therefore use Readline through its alternative callback-based
//   interface and also set up a pipe to write messages to, which we
//   can then print to the output, taking care not to get in the way
//   of Readline.

// Here's the function called by Readline when a new line of user
// input is available:

static void line_handler(char *line)
{
    // When the user presses k`Ctrl-d`, `line` will be null and we
    // exit in response.

    if (!line) {
        exit(EXIT_SUCCESS);
    }

    // When there is input, we first skip any initial whitespace.

    char *t;
    for (t = line; isspace(*t); t++);

    // If after that, the line entered by the user is empty, we repeat
    // the last command.  GNU History's manual doesn't seem to be very
    // clear here.  It says:

    //   > The range of valid values of offset starts at
    //   > `history_base` and ends at `history_length` - 1

    // Looking at the implementation of `history_get`, this should
    // probably read "at `history_base` + `history_length` - 1".

    if (*t == '\0') {
        if (history_length > 0) {
            evaluate(history_get(history_base + history_length - 1)->line);
        }
    } else {
        add_history(line);
        evaluate(line);
    }

    free(line);
}

// We only poll for input on a separate thread and inform the main
// thread to handle it.  This is done so as to keep almost all
// functionality in one thread (the main thread) and avoid race
// conditions, or the need for extensive synchronization.

static volatile bool ipc_ready, pipe_ready, stdin_ready;
static pthread_mutex_t poll_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t poll_cond = PTHREAD_COND_INITIALIZER;

// We also handle signal in this thread.  The handler simply flags the
// reception of the signal and lets the main loop handle it.

static volatile sig_atomic_t winch_received, int_received, term_received;

static void signal_handler(int sig)
{
    switch (sig) {
        case SIGWINCH:
            winch_received = 1;
            return;
        case SIGINT:
            int_received = 1;
            return;
        case SIGTERM:
            term_received = 1;
            return;
    }

    assert_not_reached();
}

static void *poll_streams(void *arg)
{
    while (true) {
        struct pollfd fds[] = {
            {ipc_socket, POLLIN, 0},
            {run_pipe[0], POLLIN, 0},
            {STDIN_FILENO, POLLIN, 0}
        };

        if (poll(fds, 3, -1) == -1) {
            // Any delivered signals will cause `poll` to return,
            // whereupon we wake up the main thread.

            assert(errno == EINTR);
            assert(winch_received || int_received || term_received);

            glfwPostEmptyEvent();
            continue;
        }

        // We need to synchronize access to the flags, as they're
        // accessed from both threads.  Upon finding one ore more
        // streams ready, we update the flags and wake up the main
        // thread via `glfwPostEmptyEvent`.

        pthread_mutex_lock(&poll_mutex);

        ipc_ready = fds[0].revents & POLLIN;
        pipe_ready = fds[1].revents & POLLIN;
        stdin_ready = fds[2].revents & POLLIN;

        glfwPostEmptyEvent();

        // If we now were to loop immediately back to `poll`, we would
        // likely find the same streams ready again, as the main
        // thread might not have had time to read all available data.
        // We would then block on `poll_mutex`, waiting for the main
        // thread to read the streams and immediately wake it up again
        // to read data that has already been read.  This would cause
        // it to block.

        // We therefore wait on a condition variable after waking the
        // main thread, which signals it after reading the streams.

        pthread_cond_wait(&poll_cond, &poll_mutex);
        pthread_mutex_unlock(&poll_mutex);
    }

    return nullptr;
}

// ## The Debugger Main Function

// This cleanup function will be registerd to run at exit.  It kills
// the inferior, if it's running and resets the terminal state.

static void clean_up(void)
{
    if (rl_readline_state & RL_STATE_CALLBACK) {
        rl_callback_handler_remove();
    }

    if (kill_inferior() == -1) {
        print_error("Could not stop run (%s)\n", strerror(errno));
    }
}

int main(int argc, char *argv[])
{
    // We begin by initializing the GLFW.

    glfwSetErrorCallback(error_callback);

    if (!glfwInit()) {
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);

    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
#ifndef NDEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
#endif
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // We want to be able to read geometry and commands from outside,
    // so we set up an IPC channel.  We use a UNIX domain socket to
    // listen for incoming connections on.

    ipc_socket = socket(AF_UNIX, SOCK_STREAM, 0);

    if (ipc_socket == -1) {
        print_error("Could not create IPC socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // We bind the socket to an abstract address, to spare ourselves
    // the need to clean up files on the filesystem.

    {
        struct sockaddr_un addr = {};

        addr.sun_family = AF_UNIX;
        socklen_t n;

        n = (
            offsetof(struct sockaddr_un, sun_path) +
            snprintf(
                addr.sun_path + 1, sizeof(addr.sun_path) - 1,
                "gammadb-%d", getpid()) + 1);

        if (bind(ipc_socket, (const struct sockaddr *)&addr, n) == -1) {
            print_error("Could not bind socket: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    if (listen(ipc_socket, 20) == -1) {
        print_error("Could not listen on ICP socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // We also create the inferior pipe now, before parsing command
    // line arguments that might lead to it being executed.  Ref:
    // Running the Inferior.

    if (pipe(run_pipe) == -1) {
        print_error("Could not create run pipe: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (atexit(clean_up)) {
        print_error("Could not install cleanup handler\n");
        exit(EXIT_FAILURE);
    }

    int n, option, no_init = 0;
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
  --target=TARGET       Create a window on startup and set its main\n\
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
            settings.batch = 1;
            settings.present_on_reload = false;
            break;

        case ARGS:
        {
            size_t n = 0;
            for (int i = optind; i < argc; n += strlen(argv[i++]) + 1);

            settings.args = (char *)malloc(n + 1);

            if (n == 0) {
                settings.args[0] = '\0';
            } else {
                for (char *s = settings.args; optind < argc; optind++) {
                    s = stpcpy(s, argv[optind]);
                    s = stpcpy(s, " ");
                }
            }

            break;
        }

        case TARGET:
        {
            struct viewport *v = find_window(optarg)->viewports;

            free((char *)v->name);
            v->name = strdup(optarg);

            break;
        }

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

                if (fp != stdin && fclose(fp)) {
                    print_error(
                        "Could not close file '%s': %s\n",
                        optarg, strerror(errno));
                    exit(EXIT_FAILURE);
                }
            } else {
                print_error(
                    "Could not open file '%s': %s\n", optarg, strerror(errno));
                exit(EXIT_FAILURE);
            }

            break;
        }

        case '?':
            exit(EXIT_FAILURE);
        }
    }

    if (settings.batch) {
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

            if (fclose(fp)) {
                print_error("Could not close init file: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
        } else if (errno != ENOENT) {
            print_error("Could not open init file: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    // Before entering the main loop, we create the polling and signal
    // handling thread. Ref: Reading Terminal Input.

    pthread_t thread;

    if (pthread_create(&thread, nullptr, poll_streams, nullptr)) {
        print_error(
            "Could not create run thread: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Since we're using the callback interface, we need to intercept
    // terminal size changes and let Readline handle them.
    // Additionally, we intercept the `INT` signal to provide the
    // usual "cancel this command" behavior, as well as the `TERM`
    // signal to clean up properly on termination.

    // We also block signal delivery in the main thread, to ensure
    // they're all delivered to the polling thread.

    {
        struct sigaction sa;
        sa.sa_handler = signal_handler;
        sa.sa_flags = 0;
        sigemptyset(&sa.sa_mask);

        sigaction(SIGWINCH, &sa, nullptr);
        sigaction(SIGINT, &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);

        sa.sa_handler = empty_handler;

        sigset_t set;
        sigfillset(&set);

        if (sigprocmask(SIG_BLOCK, &set, nullptr) == -1) {
            print_error("Could not block signals: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    // We can now initialize Readline and enter the main loop, where
    // we:

    rl_attempted_completion_function = completion_function;
    rl_basic_word_break_characters = WORD_BREAK_CHARACTERS;
    rl_completion_word_break_hook = word_break_hook;
    rl_callback_handler_install("# ", line_handler);

    print_output("gammadb, version " VERSION_NUMBER "\n\
Copyright (C) 2025 Dimitris Papavasiliou.\n\
This is free software; see the source code for copying conditions.\n\
There is ABSOLUTELY NO WARRANTY; not even for MERCHANTABILITY or\n\
FITNESS FOR A PARTICULAR PURPOSE.\n\n");

    while (true) {
        //   1. update the UI as required,

        glfwWaitEvents();
        refresh_windows();

        //   2. handle any received signals and

        if (winch_received) {
            rl_resize_terminal();

            winch_received = 0;
        }

        if (int_received) {
            rl_callback_handler_remove();
            rl_crlf();
            rl_callback_handler_install("# ", line_handler);

            int_received = 0;
        }

        if (term_received) {
            exit(EXIT_SUCCESS);
        }

        //   3. read input, if available:

        pthread_mutex_lock(&poll_mutex);
        if (ipc_ready || pipe_ready || stdin_ready) {

            //   - Accept any incoming connections on the IPC socket and
            //   parse the received commands.

            if (ipc_ready) {
                const int fd = accept(ipc_socket, nullptr, nullptr);

                if (fd == -1) {
                    print_error(
                        "Could not accept connection: %s\n", strerror(errno));

                    exit(EXIT_FAILURE);
                }

                //   We wrap the data socket inside a `FILE *` to let
                //   the C library handle I/O on the file.

                FILE *fp = fdopen(fd, "r");
                read_commands(fp);
                fclose(fp);

                ipc_ready = false;
            }

            //   - Print messages from the inferior via the pipe.

            if (pipe_ready) {
                char c;

                begin_print();

                do {
                    if (read(run_pipe[0], &c, 1) != 1) {
                        print_error(
                            "Could not read from run pipe: %s\n",
                            strerror(errno));

                        exit(EXIT_FAILURE);
                    }

                    putchar(c);
                } while (c != '\n');

                end_print();
                pipe_ready = false;
            }

            //   - Send terminal input to Readline.

            if (stdin_ready) {
                rl_callback_read_char();
                stdin_ready = false;
            }

            pthread_cond_signal(&poll_cond);
        }

        pthread_mutex_unlock(&poll_mutex);
    }

    return 0;
}

#undef WORD_BREAK_CHARACTERS
