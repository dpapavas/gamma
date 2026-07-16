// Copyright 2022 Dimitris Papavasiliou

// This file is part of Gamma.

// Gamma is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.

// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details.

// You should have received a copy of the GNU General Public License along with
// this program. If not, see <https://www.gnu.org/licenses/>.

#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <cstring>
#include <thread>

#include <getopt.h>

#ifdef HAVE_SCHEME
#include "scheme_frontend.h"
#endif

#ifdef HAVE_LUA
#include "lua_frontend.h"
#endif

#include "options.h"
#include "operation.h"

// Document: program

// # Command Line Option Parsing

// We start with flags, i.e. boolean variables albeit encoded as
// `int`s due to the way these are handled by `getopt_long`.  These
// can relate to:

namespace Flags {
    //  1. debugging,

    int dump_abridged_tags = 1;
    int dump_annotations = 1;

    //  2. diagnostics,

    int warn_fatal_errors = 0;
    int warn_error = 0;

    int warn_duplicate = 0;
    int warn_manifold = 0;
    int warn_nef = 0;
    int warn_unused = 0;
    int warn_store = 0;
    int warn_load = 0;
    int warn_outputs = 0;

    int warn_mesh_valid = 0;
    int warn_mesh_closed = 0;
    int warn_mesh_manifold = 0;
    int warn_mesh_degenerate = 0;
    int warn_mesh_self_intersects = 0;
    int warn_mesh_bounds_volume = 0;
    int warn_mesh_oriented = 0;

    //  3. evaluation,

    int dry_run = 0;
    int evaluate = 1;
    int fold_transformations = 1;
    int fold_booleans = 1;
    int fold_flushes = 1;
    int fold_offsets = 1;
    int eliminate_dead_operations = 1;
    int store_operations = 1;
    int load_operations = 1;

    //  4. output, or

    int output = 0;
    int output_stl = 0;
    int output_off = 0;
    int output_wrl = 0;

    //  5. the Scheme front end.

    int print_scheme_warnings = 0;
}

// Similarly, there are non-flag options, again relating to:

namespace Options {
    //  1. debugging,

    const char *dump_graph;
    const char *dump_list;
    const char *dump_log;
    int dump_short_tags = -1;
    const char *ipc_address = "gammadb";

    //  2. diagnostics,

    Diagnostics_color_mode diagnostics_color =
        Diagnostics_color_mode::AUTO;
    int diagnostics_elide_tags = 1;
    int diagnostics_shorten_tags = 50;

    //  3. evaluation,

    Language language = Language::AUTO;
    int threads = std::thread::hardware_concurrency();
    int store_compression = 6;
    int store_threshold = 1;
    int rewrite_pass_limit = -1;

    //  4. output,

    std::forward_list<std::string> outputs;

    //  5. warnings, or

    Polyhedron_booleans_mode polyhedron_booleans =
        Polyhedron_booleans_mode::AUTO;

    //  6. the front ends.

    std::forward_list<std::pair<std::string, std::string>> definitions;
    std::forward_list<std::string> library_directories;
}

int parse_options(int argc, char *argv[])
{
    // This enumaraton holds identifiers for long-only options.  The
    // call to `getopt_long` returns these when it comes accross the
    // respective long options.

    enum {
        VERSION = 1000,
        DIAGNOSTICS_COLOR,
        DIAGNOSTICS_ELIDE_TAGS,
        DUMP_GRAPH,
        DUMP_LIST,
        DUMP_LOG,
        DUMP_SHORT_TAGS,
        IPC_ADDRESS,
        DIAGNOSTICS_SHORTEN_TAGS,
        NO_OUTPUT,
        POLYHEDRON_BOOLEANS,
        REWRITE_PASS_LIMIT,
        STORE_COMPRESSION,
        STORE_THRESHOLD};

    // Here are the long form option specifications.  These include
    // both long-only options and the long version of options that are
    // available in both short and long form.

    // Grouping again by category:

    static struct option options[] = {
        {"help", no_argument, nullptr, 'h'},
        {"version", no_argument, nullptr, VERSION},

        //   1. debugging,

        {"dump-abridged-tags", no_argument, &Flags::dump_abridged_tags, 1},
        {"no-dump-abridged-tags", no_argument, &Flags::dump_abridged_tags, 0},
        {"dump-annotations", no_argument, &Flags::dump_annotations, 1},
        {"no-dump-annotations", no_argument, &Flags::dump_annotations, 0},
        {"dump-graph", optional_argument, nullptr, DUMP_GRAPH},
        {"no-dump-graph", no_argument, nullptr, -DUMP_GRAPH},
        {"dump-list", optional_argument, nullptr, DUMP_LIST},
        {"no-dump-list", no_argument, nullptr, -DUMP_LIST},
        {"dump-log", optional_argument, nullptr, DUMP_LOG},
        {"no-dump-log", no_argument, nullptr, -DUMP_LOG},
        {"no-dump-short-tags", no_argument, &Options::dump_short_tags, -1},
        {"dump-short-tags", optional_argument, nullptr, DUMP_SHORT_TAGS},
        {"ipc-address", required_argument, nullptr, IPC_ADDRESS},

        //   2. diagnostics,

        {"no-diagnostics-color", optional_argument, nullptr, -DIAGNOSTICS_COLOR},
        {"diagnostics-color", optional_argument, nullptr, DIAGNOSTICS_COLOR},
        {"no-diagnostics-elide-tags", no_argument, nullptr,
         -DIAGNOSTICS_ELIDE_TAGS},
        {"diagnostics-elide-tags", optional_argument, nullptr,
         DIAGNOSTICS_ELIDE_TAGS},
        {"no-diagnostics-shorten-tags", no_argument,
         &Options::diagnostics_shorten_tags, -1},
        {"diagnostics-shorten-tags", optional_argument, nullptr,
         DIAGNOSTICS_SHORTEN_TAGS},

        //   3. evaluation,

        {"threads", required_argument, nullptr, 't'},
        {"no-threads", no_argument, &Options::threads, 0},
        {"evaluate", no_argument, &Flags::evaluate, 1},
        {"no-evaluate", no_argument, &Flags::evaluate, 0},
        {"dry-run", no_argument, &Flags::dry_run, 1},
        {"no-dry-run", no_argument, &Flags::dry_run, 0},
        {"fold-transformations", no_argument, &Flags::fold_transformations, 1},
        {"no-fold-transformations", no_argument, &Flags::fold_transformations, 0},
        {"fold-booleans", no_argument, &Flags::fold_booleans, 1},
        {"no-fold-booleans", no_argument, &Flags::fold_booleans, 0},
        {"fold-flushes", no_argument, &Flags::fold_flushes, 1},
        {"no-fold-flushes", no_argument, &Flags::fold_flushes, 0},
        {"fold-offsets", no_argument, &Flags::fold_offsets, 1},
        {"no-fold-offsets", no_argument, &Flags::fold_offsets, 0},
        {"polyhedron-booleans", required_argument, nullptr, POLYHEDRON_BOOLEANS},
        {"eliminate-dead-operations", no_argument,
         &Flags::eliminate_dead_operations, 1},
        {"no-eliminate-dead-operations", no_argument,
         &Flags::eliminate_dead_operations, 0},
        {"store-operations", no_argument, &Flags::store_operations, 1},
        {"no-store-operations", no_argument, &Flags::store_operations, 0},
        {"load-operations", no_argument, &Flags::load_operations, 1},
        {"no-load-operations", no_argument, &Flags::load_operations, 0},
        {"store-compression", optional_argument, nullptr, STORE_COMPRESSION},
        {"no-store-compression", no_argument, &Options::store_compression, -1},
        {"rewrite-pass-limit", required_argument, nullptr, REWRITE_PASS_LIMIT},
        {"no-rewrite-pass-limit", no_argument, &Options::rewrite_pass_limit, -1},
        {"store-threshold", required_argument, nullptr, STORE_THRESHOLD},
        {"no-store-threshold", no_argument, &Options::store_threshold, 0},

        //   4. output, and

        {"output", required_argument, nullptr, 'o'},
        {"no-output", required_argument, nullptr, NO_OUTPUT},
        {"output-stl", no_argument, &Flags::output_stl, 1},
        {"stl", no_argument, &Flags::output_stl, 1},
        {"no-output-stl", no_argument, &Flags::output_stl, 0},
        {"no-stl", no_argument, &Flags::output_stl, 0},
        {"output-off", no_argument, &Flags::output_off, 1},
        {"off", no_argument, &Flags::output_off, 1},
        {"no-output-off", no_argument, &Flags::output_off, 0},
        {"no-off", no_argument, &Flags::output_off, 0},
        {"output-wrl", no_argument, &Flags::output_wrl, 1},
        {"wrl", no_argument, &Flags::output_wrl, 1},
        {"no-output-wrl", no_argument, &Flags::output_wrl, 0},
        {"no-wrl", no_argument, &Flags::output_wrl, 0},

        //   5. language front end options.

        {"language", required_argument, nullptr, 'x'},
        {"library-directory", required_argument, nullptr, 'L'},
        {"define-parameter", required_argument, nullptr, 'D'},

#ifdef HAVE_SCHEME
        {"print-scheme-warnings", no_argument, &Flags::print_scheme_warnings, 1},
        {"no-print-scheme-warnings", no_argument,
         &Flags::print_scheme_warnings, 0},
#endif

        {nullptr, 0, nullptr, 0}
    };

    // We handle the manipulation of most options by means of the
    // following simple macros.

#define PUSH_SIMPLE_OPTION(X) {                 \
        Options::X.push_front(optarg);          \
        break;                                  \
    }

#define REMOVE_SIMPLE_OPTION(X) {               \
        Options::X.remove(optarg);              \
        break;                                  \
    }

#define PUSH_OPTION(X, VAL) {                   \
        Options::X.push_front(VAL);             \
        break;                                  \
    }

#define STRING_OPTION(X) {                              \
        Options::X = optarg ? strdup(optarg) : optarg;  \
        break;                                          \
    }

#define INTEGER_OPTION(X, COND) {                       \
        char *p;                                        \
                                                        \
        if (const int i = std::strtol(optarg, &p, 10);  \
            *p == '\0' && COND) {                       \
            Options::X = i;                             \
            break;                                      \
        }                                               \
                                                        \
        goto invalid_argument;                          \
    }

#define OPTIONAL_ARGUMENT(X, DEFAULT) {         \
        Options::X = DEFAULT;                   \
                                                \
        if (!optarg) {                          \
            break;                              \
        }                                       \
    }

#define NOMINAL_OPTION(X, A, V) {               \
        if (!std::strcmp(optarg, A)) {          \
            Options::X = V;                     \
            break;                              \
        }                                       \
    }

#define WARN_OPTIONS_BEGIN {                                            \
        char *s = optarg;                                               \
        bool q = false;                                                 \
                                                                        \
        const bool p = !!std::strncmp(s, "no-", 3);                     \
                                                                        \
        if (!p) {                                                       \
            s += 3;                                                     \
        }

#define WARN_OPTIONS_GROUP(X)                   \
        q = !std::strcmp(s, #X);

#define WARN_OPTION_S(X, S)                         \
        if (q || !std::strcmp(s, S)) {              \
            Flags::warn_## X = p;                   \
            if (!q) {                               \
                break;                              \
            }                                       \
        }

#define WARN_OPTION(X) WARN_OPTION_S(X, #X)

#define WARN_OPTIONS_GROUP_END                                          \
        if (q) {                                                        \
            break;                                                      \
        }                                                               \
                                                                        \
        q = false;

#define WARN_OPTIONS_END                        \
    }

#define OPTION_END goto invalid_argument;

    int argc_max;

    // We want to skip any arguments meant for the programs.

    for (argc_max = 1;
         argc_max < argc && std::strcmp(argv[argc_max - 1], "--");
         argc_max++);

    // This resetting of `optind` to 1 allows us to run parse_options
    // more than once, which is only required in tests.

    optind = 1;

    // We can now iterate over all supplied arguemts, processing each
    // as needed.

    int n, option;
    while ((n = -1, option = getopt_long(
                argc_max, argv,
                "-ht:W:x:"
#ifdef HAVE_SCHEME
                "F:"
#endif
                "L:D:o:i",
                options, &n)) != -1) {
        switch (option) {
        case 1: {
            // This is a non-option argument, i.e. a program source
            // file.  We need to figure out what language it's for,
            // based onn the current language mode and extension.

            int (*run)(const char *input, char **first, char **last);

            const char *c = std::strrchr(optarg, '.');

            if (Options::language == Language::LUA
                || (Options::language == Language::AUTO
                    && c && !std::strcmp(c + 1, "lua"))) {
#ifdef HAVE_LUA
                run = run_lua;
#else
                std::cerr << argv[0] << ": "
                          << optarg
                          << (": Lua front end is not enabled; ignoring input "
                              "file\n");
                break;
#endif
            } else if (Options::language == Language::SCHEME
                       || (Options::language == Language::AUTO
                           && c && !std::strcmp(c + 1, "scm"))) {
#ifdef HAVE_SCHEME
                run = run_scheme;
#else
                std::cerr << argv[0] << ": "
                          << optarg
                          << (": Scheme front end is not enabled; ignoring input "
                              "file\n");
                break;
#endif
            } else {
                std::cerr << argv[0] << ": "
                          << optarg
                          << (": cannot determine language from suffix; "
                              "please specify explicitly\n");
                goto error;
            }

            // We add the path the source file resides in to the
            // languages load path.  This facilitates importing of
            // libaries or including and loading files bundled with
            // the program.

            std::filesystem::path p = std::filesystem::absolute(optarg);
            std::string s = p.filename();

            if (p.has_parent_path()) {
                Options::library_directories.push_front(
                    p.parent_path().native());
            }

            // We can now execute the program.  We break early if
            // there were any errors.

            int i = run(optarg, argv + argc_max, argv + argc);

            if (p.has_parent_path()) {
                Options::library_directories.pop_front();
            }

            if (i != 0) {
                goto error;
            }

            break;
        }
            // Document: program,manual
            // Alias: [ r`[`
            // Alias: ] r`]`

            // ## Gamma Command Line Options

            // Here is a complete list of the command line options
            // accepted by Gamma.  Many options have both long and
            // short forms; both are shown in such cases.

            // Concept: command line options

        case 'h':
            //   -h :=
            //   --help := Print a short help message and exit.

            std::cout
                << "Usage: " << argv[0] << R"( [OPTION...] FILE... [-- ARG...]
Options:
  -h, --help            Display this help message.
  --version             Display version information.


Language front end options:
  -x LANG               Specify the language of following input files.
  -L DIR                Add the directory DIR to the list of directories
                        to be searched for modules or libraries.
  -D NAME               Predefine global variable with a value of true (or set
                        its value if it is already defined).
  -D NAME=VALUE         Predefine global variable with the given value (or set
                        its value if it is already defined).  The value is
                        interpreted in the context of the active language.)"

#ifdef HAVE_SCHEME
                R"(
  --print-scheme-warnings
                        Print warnings from the Scheme front end.)"
#endif

                R"(
Evaluation options:
  --dry-run             Go through the motions, but don't evaluate anything.
  --no-evaluate         Skip evaluation altogether.
  -t N, --threads=N     Use no more than specified number of additional
                        evaluation threads.
  --no-threads          Do not use additional threads for evaluation.
  --polyhedron-booleans=MODE
                        Set polyhedron boolean operation execution strategy.
                        MODE can be one of 'corefine', 'nef',  or 'auto'.
  --rewrite-pass-limit=N Perform at most N rewrite passes.
  --no-fold-transformations
                        Disable transformation operation folding.
  --no-fold-flushes     Disable flush operation folding.
  --no-fold-offsets     Disable polygon offset operation folding.
  --no-fold-booleans    Disable boolean operation folding.
  --no-eliminate-dead-operations
                        Do not skip evaluation of unneeded operations.
  --no-store-operations Do not store evaluated operations to disk.
  --no-load-operations  Do not load stored operations from disk.
  --store-compression[=LEVEL]
                        Compress stored operations.
  --no-store-compression Do not compress stored operations.
  --store-threshold=N   Don't store operations with cumulative evaluation
                        time below the specified threshold (in seconds).
  --no-store-threshold  Store all operations, irrespective of evaluation time.

Output options:
  --output-stl, --stl   Write each defined output to a file, in STL format.
  --output-off, --off   Write each defined output to a file, in OFF format.
  --output-wrl, --wrl   Write each defined output to a file, in WRL format.
  -o FILE, --output=FILE Write output with the same name as FILE, sans the
                        suffix, in a format determined by the suffix.
  -o FILE:OUTPUT, --output=FILE:OUTPUT
                        Write output with name OUTPUT to file FILE, in a
                        format determined by the suffix.
  --no-output OUTPUT    Cancel previously requested output OUTPUT.
  --ipc-address=ADDR    Set the debugger's IPC address.

Diagnostics options:
  --diagnostics-color[=WHEN]
                        Colorize diagnostics output. WHEN can be one of
                        'always', 'never', 'auto'.
  --diagnostics-elide-tags[=DEPTH]
                        Elide tags in diagnostics, past the specified level.
  --diagnostics-shorten-tags[=N]
                        Limit the maximum length of tags in diagnostics.

  -Wfatal-errors        Discontinue evaluation after the first error.
  -Werror               Treat all warnings as errors.

  -Wduplicate           Warn if an operation is instantiated multiple times.
  -Wmanifold            Warn if an operation results in a non-manifold mesh.
  -Wnef                 Warn if a polyhedron operation could have been
                        executed without using Nef polyhedra.
  -Wunused              Warn if an operation is instantiated, but not used.
  -Wstore               Warn if an operation is stored.
  -Wload                Warn if an operation is loaded.
  -Woutputs             Warn if an output is left unused.
  -Wmesh                Enable all -Wmesh- warnings.
  -Wmesh-valid          Warn if an operation results in an invalid mesh.
  -Wmesh-closed         Warn if an operation results in a mesh with holes.
  -Wmesh-manifold       Warn if an operation results in a non-manifold mesh.
  -Wmesh-degenerate     Warn if an operation results in a mesh with degenerate
                        faces or edges.
  -Wmesh-self-intersects Warn if an operation results in a self-intersecting mesh.
  -Wmesh-bounds-volume  Warn if an operation results in a mesh that doesn't
                        bound a volume.
  -Wmesh-oriented       Warn if an operation results in a mesh that is not
                        oriented outwards.

Debugging options:
  --dump-list[=FILE]       Dump evaluated operations.
  --dump-log[=FILE]        Dump evaluation log.
  --dump-graph[=FILE]      Dump evaluation graph.
  --no-dump-abridged-tags  Do not substitute operands in dumped operation
                           tags with evaluation sequence numbers.
  --no-dump-annotations    Do not annotate dumped operations.
  --dump-short-tags[=N]    Limit the maximum length in dumped tags.)"
                << std::endl;

            break;

        case VERSION:
            //   --version := Print the program version number,
            //   along with copyright information and exit.

            std::cout
                << ("Gamma " GAMMA_VERSION R"(
Copyright (C) 2022 Dimitris Papavasiliou.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see http://www.gnu.org/licenses/.)")
                << std::endl;
            break;

            // ### Options Controlling Language Front Ends

            // These options control how programs are executed by the
            // language front ends.  Unless otherwise stated, they
            // apply uniformly to all language front ends.

        case 'x':
            //   -x v`language` :=
            //   --language=v`language` := Specify explicitly the
            //   language for the following input files, rather than
            //   letting it be determined based on the file name
            //   suffix.  This option applies to all following input
            //   files until the next -x option.  Possible values for
            //   v`language` are s`scheme`, s`lua` and s`auto`.

            //   If set to s`auto`, subsequent files are handled
            //   according to their file name suffixes, as if o`-x`
            //   had not been used at all.  In this case, files ending
            //   in f`.scm`, or f`.lua` assumed to contain Scheme and
            //   Lua source code respectively.

            OPTIONAL_ARGUMENT(language, Language::AUTO);

#ifdef HAVE_LUA
            NOMINAL_OPTION(language, "lua", Language::LUA);
#endif

#ifdef HAVE_SCHEME
            NOMINAL_OPTION(language, "scheme", Language::SCHEME);
#endif

            NOMINAL_OPTION(language, "auto", Language::AUTO);
            OPTION_END;

        case 'L':
            //   -L v`dir` :=
            //   --library-directory=v`dir` := Add directory v`dir` to
            //   the front of the list of directories to be searched
            //   when a language front end attempts to load a library.
            //   Consult the language's library system documentation
            //   for details on the way in which this list of search
            //   paths is handled.

            PUSH_SIMPLE_OPTION(library_directories);

        case 'D':
            // Concept: parameters

            //   -D v`name`[=v`value`] :=
            //   --define-parameter=v`name`[=v`value`] := Define
            //   a global variable before executing each of the
            //   following programs.  If v`value` is specified, it is
            //   evaluated as an expression of the program's language
            //   and the result is set as the value of the defined
            //   variable.  If there is an error in the evaluation of
            //   the expression, it is reported before exiting.

            //   When no v`value` is specified, the variable is given
            //   a boolean true value.

            if (const char *p = strchr(optarg, '=')) {
                PUSH_OPTION(
                    definitions,
                    std::pair(
                        std::string(optarg, p - optarg), std::string(p + 1)));
            } else {
                PUSH_OPTION(
                    definitions,
                    std::pair(std::string(optarg), std::string()));
            }

            // Document: program,manual/scheme

            //   --print-scheme-warnings := Enable output of Guile
            //   warning messages.  These tend to litter the standard
            //   error stream with messages regarding Guile's
            //   operation which often just get in the way.  They are
            //   therefore normally disabled.

            // Document: program,manual

            // ### Options Controlling Evaluation

            // The following options control how operations are
            // evaluated once programs have finished executing.

            //   --dry-run := Disables actual geometric calculations
            //   during evaluation.  As a result no outputs are
            //   created.

            //   --no-evaluate := Skip the evaluation stage
            //   altogether.

        case 't': INTEGER_OPTION(threads, i >= 0);
            //   --no-threads :=
            //   -t v`n` :=
            //   --threads=v`n` := Use up to v`n` threads when
            //   evaluating operations.  This limit does not include
            //   the main thread and may be exceeded on certain
            //   occasions, but is honored on average.  If v`n` is set
            //   to zero, or with the equivalent o`--no-threads`, only
            //   the main thread is used.  This is set to the number
            //   of concurrent threads supported by the processor by
            //   default.

        case POLYHEDRON_BOOLEANS:
            //   --polyhedron-booleans=v`mode` := Polyhedron boolean
            //   operations can be evaluated either using Nef
            //   polyhedra, or via corefinement.  The former are
            //   generally more robust than the latter, but usually
            //   require more computational effort.

            //   When v`mode` is set to s`auto`, which is the default
            //   value, Gamma is free to decide how to perform the
            //   evaluation.  It can also be set to s`corefine`, or
            //   s`nef`, to force conversion of the operands of
            //   boolean operations as required to carry out the
            //   evaluation using corefinement or Nef polyhedra
            //   respectively.

            NOMINAL_OPTION(polyhedron_booleans,
                           "corefine", Polyhedron_booleans_mode::COREFINE);
            NOMINAL_OPTION(polyhedron_booleans,
                           "nef", Polyhedron_booleans_mode::NEF);
            NOMINAL_OPTION(polyhedron_booleans,
                           "auto", Polyhedron_booleans_mode::AUTO);
            OPTION_END;

        case REWRITE_PASS_LIMIT:
            //   --no-rewrite-pass-limit :=
            //   --rewrite-pass-limit=v`n` := Do not make more than
            //   v`n` passes when rewriting the evaluation graph.
            //   Such rewrites restructure the calculations that will
            //   be evaluated in various ways without altering their
            //   result, in the hope of improving evaluation
            //   efficiency.  They are applied in multiple passes, as
            //   the rewrites carried out in each pass may potentially
            //   be rewritable themselves.  Indeed, many of the
            //   rewrites depend on it.

            //   By default, or with o`--no-rewrite-pass-limit`,
            //   passes are made until no further rewrites are
            //   possible.  Set v`n` to zero to disable rewriting
            //   altogether.

            INTEGER_OPTION(rewrite_pass_limit, i >= 0);

            //   --no-fold-transformations :=
            //   --no-fold-flushes :=
            //   --no-fold-offsets := Disable folding rewrites of
            //   transformation, flush, or polygon offset
            //   operations.  These rewrites substitute a series of
            //   two or more such operations with a single equivalent
            //   operation.

            //   --no-fold-booleans := Disable folding of associative
            //   boolean operations.  Boolean folding consists in
            //   rewriting a series of three or more union or
            //   intersection operations of the form $a \cdot b \cdot
            //   c \cdot d$, to $((a \cdot b) \cdot (c \cdot d))$.
            //   This is continued recursively in subsequent passes
            //   for longer chains.

            //   Additionally, a difference chain of the form $a - b -
            //   c - \ldots$ is rewritten to $a - (b + c + \ldots)$
            //   where the union terms can be further folded in
            //   subsequent passes.

            //   --no-eliminate-dead-operations := Evaluate all
            //   operations, regardless of whether they're part of an
            //   enabled output or not.

            //   --no-store-operations :=
            //   --no-load-operations := Disable storing costly
            //   operations to disk, or loading previously stored
            //   operations instead of re-evaluating them.

        case STORE_COMPRESSION:
            //   --no-store-compression :=
            //   --store-compression[=v`level`] := Compress
            //   stored operations before writing them to a file on
            //   disk.  The compression level can be specified as a
            //   number between 0 and 9.  By default, or if ommitted,
            //   the compression level is set to 6.  A v`level` of
            //   zero is equivalent to o`--no-store-compression`
            //   and stores operations without compression.

            //   Stored operation files have names made up of a string
            //   of hexadecimal characters, followed by the extension
            //   f`.zo` if compression is used and f`.o` otherwise.

            OPTIONAL_ARGUMENT(store_compression, 6);
            INTEGER_OPTION(store_compression, i >= 0 && i <= 9);

        case STORE_THRESHOLD:
            //   --no-store-threshold :=
            //   --store-threshold=v`n` := Store an operation to disk
            //   only if the cummulative time required for its
            //   evaluation exceeded v`n` seconds.  The cumulative
            //   evaluation time is the time required for the
            //   evaluation of the operation iself and all its
            //   prerequisite operations.

            //   This is set to 1 second by default, to avoid
            //   littering the working directory with stored
            //   operations that would be easier to re-evaluate than
            //   to load from disk.  If v`n` is set to zero, or with
            //   o`--no-store-threshold`, all operations are stored to
            //   disk.

            INTEGER_OPTION(store_threshold, i >= 0);

            // Concept: outputs

            // Programs are expected to define outputs, each assigned
            // to one or more operations whose results may need to be
            // written out.  Each output has a unique name, which can
            // be any string of numbers or characters, other than
            // whitespace.

            // The following options allow selection of the outputs
            // that should have the geometry associated with them
            // computed and written out in some manner.

            //   --stl :=
            //   --output-stl :=
            //   --off :=
            //   --output-off :=
            //   --wrl :=
            //   --output-wrl := Write all defined ouputs to disk in
            //   the STL, OFF, or WRL format.  Each file is written in
            //   the current working directory, with a name made up of
            //   the ouput's name and the selected format's extension.

        case 'o':
            //   -o v`filename` :=
            //   --output=v`filename` :=
            //   -o v`filename`:v`output` :=
            //   --output=v`filename`:v`output` := When only
            //   v`filename` is specified, select the output with
            //   matching name, disregarding the extension and write
            //   it out to disk in the current directory.  The format
            //   of the written file is determined by the supplied
            //   name's extension, which should be one of f`.stl`,
            //   f`.off`, or f`.wrl`.

            //   The output that is to be written to v`filename` can
            //   also be specified explicitly after a s`:`, as in the
            //   second form shown above.  In this form, the
            //   v`filename` may also be specified without an
            //   extension.  In this case the output is not written to
            //   disk, but is instead written to an IPC channel, used
            //   for communication with the Debugger.

            PUSH_SIMPLE_OPTION(outputs);

        case NO_OUTPUT:
            //   --no-output-stl :=
            //   --no-output-off :=
            //   --no-output-wrl :=
            //   --no-output=v`filename` :=
            //   --no-output=v`filename`:v`output` := Cancel the
            //   effect of matching preceding output options.

            REMOVE_SIMPLE_OPTION(outputs);

        case IPC_ADDRESS:
            STRING_OPTION(ipc_address);
            //   --ipc-address=v`address` := Set the IPC channel
            //   address used to communicate with the Debugger.  This
            //   is option is set to the approriate address by the
            //   Debugger when it invokes Gamma.  It is generally not
            //   needed when Gamma is used on its own.

            // ### Options Controlling Diagnostic Messages

            // These options can request or suppress the output of
            // certain diagnostic messages, or control how messages
            // are formatted.

        case -DIAGNOSTICS_COLOR:
            //   --no-diagnostics-color :=
            //   --diagnostics-color[=v`when`] := Use color in
            //   diagnostic messages. Possible values for v`when` are
            //   s`never`, s`always`, or s`auto`.  The default setting
            //   is s`auto` and color is used only if the output
            //   stream is a terminal and has not been redirected to a
            //   file, for instance.

            //   The o`--no-diagnostics-color` option inhibits
            //   coloring altogether and is equivalent to
            //   o`--diagnostics-color=never`.

            Options::diagnostics_color = Diagnostics_color_mode::NEVER;
            break;

        case DIAGNOSTICS_COLOR:
            OPTIONAL_ARGUMENT(
                diagnostics_color, Diagnostics_color_mode::ALWAYS);
            NOMINAL_OPTION(diagnostics_color,
                           "never", Diagnostics_color_mode::NEVER);
            NOMINAL_OPTION(diagnostics_color,
                           "always", Diagnostics_color_mode::ALWAYS);
            NOMINAL_OPTION(diagnostics_color,
                           "auto", Diagnostics_color_mode::AUTO);
            OPTION_END;

        case -DIAGNOSTICS_ELIDE_TAGS:
            //   --no-diagnostics-elide-tags :=
            //   --diagnostics-elide-tags[=v`depth`] := Replace parts
            //   of an operation's description that are nested deeper
            //   than the specified level with s`...` in diagnostic
            //   messages.

            //   As evaluation progresses, longer and longer strings
            //   of nested operations tend to form, with
            //   correspondingly longer textual representations.  Such
            //   descriptions are hard to read and are not very
            //   useful as parts of diagnostic messages.

            //   By default, or when v`depth` is not specified, parts
            //   nested more than one level deep are elided.  The
            //   o`no-diagnostics-elide-tags` disables elision.

            Options::diagnostics_elide_tags = -1;
            break;

        case DIAGNOSTICS_ELIDE_TAGS:
            OPTIONAL_ARGUMENT(diagnostics_elide_tags, 1);
            INTEGER_OPTION(diagnostics_elide_tags, i >= 0);

        case DIAGNOSTICS_SHORTEN_TAGS:
            //   --no-diagnostics-shorten-tags :=
            //   --diagnostics-shorten-tags[=v`length`] := Truncate
            //   the description of operations in diagnostic messages
            //   at the specified length in characters, appending
            //   s`...`.

            //   By default, or when v`length` is not specified,
            //   descriptions are trucated after 50 characters.  The
            //   o`no-diagnostics-shorten-tags` disables truncation,
            //   causing descriptions to be written in full.

            OPTIONAL_ARGUMENT(diagnostics_shorten_tags, 50);
            INTEGER_OPTION(diagnostics_shorten_tags, i >= 0);

            // Options starting with o`-W` enable or suppress the
            // output of warning messages.  In most cases, such
            // diagnostic messages report constructions that, while
            // not necessarily erroneous, give cause for suspicion
            // that there may have been an error.  Sometimes, they can
            // also be of use as a way of letting the evaluator
            // identify when certain situations involving operations
            // arise.

            // For each of these options, there's also a matching
            // option with a s`no-` prefix and having the opposite
            // effect.

        case 'W':
            WARN_OPTIONS_BEGIN;

            WARN_OPTION_S(fatal_errors, "fatal-errors");
            //   -Wfatal-errors := This option causes evaluation to be
            //   aborted as soon as possible once an error occurred,
            //   rather than trying to keep going with operations that
            //   can still be evaluated.

            WARN_OPTION(error);
            //   -Werror := Turn all warnings into errors.

            WARN_OPTION(duplicate);
            //   -Wduplicate := Issue a warning when the prgoram
            //   creates on operation identical to one or more it has
            //   previously created.  This is common and not
            //   problematic in itself, as such operations are
            //   evaluated only once.  Enabling this warning can aid
            //   in identifying such situations in a program,
            //   effectively letting the evaluator determine whether
            //   two results will be identical.

            WARN_OPTION(manifold);
            //   -Wmanifold := Warn if the result of a boolean
            //   operation is not manifold.  Non-manifold geometry can
            //   create problems with subsequent operations, which can
            //   be difficult to identify.  Note that this can only
            //   happen when boolean operations are carried out using
            //   Nef polyhedra, as corefinement cannot create
            //   non-manifold results.

            WARN_OPTION(nef);
            //   -Wnef := Warn when a boolean operation is carried out
            //   using Nef polyhedra, even though it could have been
            //   done via corefinement.  Corefinement can be
            //   significantly faster but fails when the result of the
            //   operation would not be manifold.

            WARN_OPTION(unused);
            //   -Wunused := Warn when an operation is evaluated even
            //   though no other operations require its results.  This
            //   is the case by definition for certain operations
            //   which are created implicitly for each output.  It can
            //   also happen for unselected outputs when the
            //   o`--no-eliminate-dead-operations` option is used.

            WARN_OPTION(store);
            //   -Wstore := Warn when an operation is stored to disk
            //   after evaluation.  Note that this refers to storing
            //   of operations to cache their results, obviating the
            //   need for future re-evaulation, not writing of outputs
            //   to disk through the o`-o` option.

            WARN_OPTION(load);
            //   -Wload := Warn if a previously stored operation is
            //   loaded from disk instead of evaluating it.  Loading
            //   an operation also prevents evaluation of all
            //   precursor operations, unless their results are
            //   required elsewhere.

            WARN_OPTION(outputs);
            //   -Woutputs := Warn when an output has been defined by
            //   the program, but was not enabled.

            // The following group of options enable or disable checks
            // that are performed after an operation resulting in a
            // polyhedron or surface mesh has been evaluated.  They
            // have no effect on operations yielding polygons or Nef
            // polyhedra.

            // They can be useful in identifying pathological
            // geometry, which might otherwise have gone unnoticed, or
            // which may have had caused downstream operations to fail
            // with misleading diagnostic messages.  Such results can
            // arise due to programmer error, for instance when
            // boolean combinations yield non-manifold results, or
            // when surface fairing is attempted with insufficient
            // boundary constraints.  In some cases they may also be
            // the result of bugs in the underlying computational
            // geometry algorithms.

            // Note that these checks can increase evaluation time,
            // sometimes substantially, depending on the geometry.

            WARN_OPTIONS_GROUP(mesh);
            //   -Wmesh := Enable all mesh integrity warnings
            //   described below.  This option can be a convenient
            //   first line of defense when a computation fails
            //   unexpectedly, or produces invalid results.

            WARN_OPTION_S(mesh_valid, "mesh-valid");
            //   -Wmesh-valid := Warn if the result of an operation is not
            //   a valid polygon mesh.  This should not normally
            //   happen.

            WARN_OPTION_S(mesh_closed, "mesh-closed");
            //   -Wmesh-closed := Warn if the result of an operation is not
            //   closed, meaning that it is a valid mesh from a
            //   combinatorial perspective, but has holes and does
            //   therefore not represent a solid.  Such meshes are not
            //   normally produced by any operations and can only
            //   arise in pathological situations.

            WARN_OPTION_S(mesh_manifold, "mesh-manifold");
            //   -Wmesh-manifold := Warn if the result of an operation
            //   contains non-manifold vertices.

            WARN_OPTION_S(mesh_degenerate, "mesh-degenerate");
            //   -Wmesh-degenerate := Warn if the result of an
            //   operation contains degenerate edges, or faces.  An
            //   edges is considered degenerate when its endpoints
            //   coincide, while a face is degenerate if its vertices
            //   are collinear.

            WARN_OPTION_S(mesh_self_intersects, "mesh-self-intersects");
            //   -Wmesh-self-intersects := Warn if the result of an
            //   operation contains self-intersections.

            WARN_OPTION_S(mesh_bounds_volume, "mesh-bounds-volume");
            //   -Wmesh-bounds-volume := Warn if an operation results
            //   in geometry with multiple connected components, that
            //   are not consistently oriented so as to bound a
            //   volume.

            WARN_OPTION_S(mesh_oriented, "mesh-oriented");
            //   -Wmesh-oriented := Warn if some or all faces of the
            //   mesh resulting from an opeation do not have normals
            //   pointing outwards with respect to the domain bounded
            //   by the mesh.

            WARN_OPTIONS_GROUP_END;

            WARN_OPTIONS_END;
            OPTION_END;

            // The options below enable or disable logging of
            // information on the evaluation of operations as it
            // progresses.  These logs are normally written to a file,
            // but can also be sent to the standard output if a s`-`
            // is used in the place of a file name.

        case DUMP_LIST:
            //   --dump-list[=v`filename`] := Write information about
            //   evaluated operations to the specified file.  If no file
            //   name is given, output is written to the file
            //   f`evaluation.list`.  If s`-` is given as the file name,
            //   information is written to the standard output instead.

            //   One line is written for each operation directly after
            //   it has been evaluated.  An example list is show below,
            //   where lines have been broken to fit the page.  They
            //   would have appeared as single line in the output.

            //   ```
            //   $0 = sphere(5,1,1/1000000) (column: 14, cost: 0.00s,
            //      facets: 20, file: example.scm, halfedges: 60, in: 0.00s,
            //      line: 4, thread: 1, vertices: 12)
            //   $1 = sphere(4,1,1/1000000) (column: 30, cost: 0.00s,
            //      facets: 20, file: example.scm, halfedges: 60, in: 0.00s,
            //      line: 4, thread: 2, vertices: 12)
            //   $2 = difference($0,$1) (column: 18, cost: 0.00s,
            //      facets: 40, file: example.scm, halfedges: 120,
            //      in: 0.00s, line: 6, thread: 2, vertices: 24)
            //   ```

            //   Each line begins with an evaluation sequence number,
            //   followed by the operation itself printed as if it were
            //   a function call and a list of annotations.  The latter
            //   are added to the operation as it goes through its life
            //   cycle and contain information concerning its
            //   construction and evaluation.

            OPTIONAL_ARGUMENT(dump_list, "\0");
            [[fallthrough]];
        case -DUMP_LIST:
            STRING_OPTION(dump_list);

        case DUMP_LOG:
            //   --dump-log[=v`filename`] := Write information about
            //   the evaluation process to the specified file.  If no
            //   file name is given, output is written to the file
            //   f`evaluation.log`.  If s`-` is given as the file
            //   name, information is written to the standard output
            //   instead.

            //   Logged lines include information on the culling and
            //   rewriting stages before evaluation proper begins,
            //   followed by one line for each evaluation phase of
            //   each operation.

            //   An operation begins by being ready for evaluation,
            //   either because it had no prerequisite operations, or
            //   because all prerequisite operations have been
            //   evaluated.  Its evaluation then starts and finally
            //   concludes, either successfully, or in failure.

            //   An example log is show below.

            //   ```
            //   2.5e-05: rewrote 0 out of 7 operations
            //   4.85e-05: retagged 0 operations
            //   8.68e-05: selected 7 operations, with 2 ready and 2 loadable
            //   0.000166: $0 = sphere(5,1,1/1000000) started
            //   0.000203: $1 = sphere(4,1,1/1000000) started
            //   0.00109: $1 concluded
            //   0.00112: nef($1) ready
            //   0.00112: $2 = nef($1) started
            //   0.00113: $0 concluded
            //   0.00114: nef($0) ready
            //   0.00116: $3 = nef($0) started
            //   0.00165: $3 concluded
            //   0.00175: $2 concluded
            //   0.00177: difference($3,$2) ready
            //   0.00178: $4 = difference($3,$2) started
            //   0.0059: $4 concluded
            //   ```

            //   This form is more verbose than the operations list,
            //   but provides a more detailed view of an evaluation,
            //   especially if it is multithreaded.  It also includes
            //   timing information.

            OPTIONAL_ARGUMENT(dump_log, "\0");
            [[fallthrough]];
        case -DUMP_LOG:
            STRING_OPTION(dump_log);

        case DUMP_GRAPH:
            //   --dump-graph[=v`filename`] := Write the evaluation
            //   graph to the specified file, in the form of Graphviz
            //   DOT language source.  If no file name is given,
            //   output is written to the file f`evaluation.dot`.  If
            //   s`-` is given as the file name, information is
            //   written to the standard output instead.

            //   The dumped source can be processed with the Graphviz
            //   `dot` program to render the graph in many output
            //   formats.  To produce a PDF file for example, you can
            //   use the command s`dot -Tpdf evaluation.dot -o
            //   evaluation.pdf`.

            OPTIONAL_ARGUMENT(dump_graph, "\0");
            [[fallthrough]];
        case -DUMP_GRAPH:
            STRING_OPTION(dump_graph);

            // Certain aspects of the formatting of output dumped with
            // the o`--dump-list`, o`--dump-log` and o`--dump-graph`
            // options, can be controlled with the following options.

            //   --no-dump-abridged-tags := When an operation is used
            //   as an argument in a succeeding operation, it is
            //   represented by its evaluation number in dumped output
            //   above.  This option disables this convention, so that
            //   operations have their arguments written out in full.

            //   --no-dump-annotations := Do not include operation
            //   annotations in dumped output.

        case DUMP_SHORT_TAGS:
            //   --no-dump-short-tags :=
            //   --dump-short-tags[=v`length`] := Truncate the
            //   description of operations in dumped output at the
            //   specified length in characters, appending s`...`.

            //   By default, or when v`length` is not specified,
            //   descriptions are trucated after 50 characters.  The
            //   o`--no-dump-short-tags` disables truncation, causing
            //   descriptions to be written in full.

            OPTIONAL_ARGUMENT(dump_short_tags, 50);
            INTEGER_OPTION(dump_short_tags, i >= 0);

        case '?':
            goto error;
        }

        continue;

      invalid_argument:
        std::cerr << argv[0] << ": invalid argument '" << optarg;

        if (n < 0) {
            std::cerr << "' -- '" << static_cast<char>(option);
        } else {
            std::cerr << "' for option '" << options[n].name;
        }

        std::cerr << "'" << std::endl;

      error:
        optind = -EXIT_FAILURE;
        break;
    }

    // Unalias: [
    // Unalias: ]
    // Document: program

#undef INTEGER_OPTION
#undef OPTIONAL_INTEGER_OPTION
#undef NOMINAL_OPTION
#undef FLAG_OPTION
#undef PUSH_OPTION
#undef PUSH_SIMPLE_OPTION

#undef WARN_OPTIONS_BEGIN
#undef WARN_OPTIONS_GROUP
#undef WARN_OPTION
#undef WARN_OPTIONS_GROUP_END
#undef WARN_OPTIONS_END

#undef OPTION_END

    // This essentially returns the number of options successfully
    // parsed (mainly useful for testing).

    return optind;
}
