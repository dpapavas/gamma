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
#include "evaluation.h"

namespace Flags {
    // Debugging

    int dump_abridged_tags = 1;
    int dump_annotations = 1;

    // Diagnostics

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
    int warn_mesh_intersects = 0;
    int warn_mesh_bounds = 0;
    int warn_mesh_oriented = 0;

    // Evaluation

    int evaluate = 1;
    int fold_transformations = 1;
    int fold_booleans = 1;
    int fold_flushes = 1;
    int eliminate_dead_operations = 1;
    int store_operations = 1;
    int load_operations = 1;

    // Output

    int output = 0;
    int output_stl = 0;
    int output_off = 0;
    int output_wrl = 0;

    // Scheme backend

    int eliminate_tail_calls = 1;
}

namespace Options {
    // Debugging

    const char *dump_graph;
    const char *dump_operations;
    const char *dump_log;
    int dump_short_tags = -1;

    // Diagnostics

    Diagnostics_color_mode diagnostics_color =
        Diagnostics_color_mode::AUTO;
    int diagnostics_elide_tags = 1;
    int diagnostics_shorten_tags = 50;

    // Evaluation

    Language language = Language::AUTO;
    int threads = 0; //std::thread::hardware_concurrency();
    int store_compression = 6;
    int store_threshold = 1;
    int rewrite_pass_limit = -1;

    // Output

    std::forward_list<std::string> outputs;

    // Warnings

    Polyhedron_booleans_mode polyhedron_booleans =
        Polyhedron_booleans_mode::AUTO;

    // Backend

    std::forward_list<std::pair<std::string, std::string>> definitions;
    std::forward_list<std::string> include_directories = {
#ifdef HAVE_SCHEME
        SCHEME_LIBRARY_DIR,
#endif

#ifdef HAVE_LUA
        LUA_LIBRARY_DIR,
#endif
    };

#ifdef HAVE_SCHEME
    std::forward_list<std::string> scheme_features;
#endif
}

static bool has_suffix(const char *s, const char *x)
{
    const char *c = std::strrchr(s, '.');

    if (c) {
        return std::strcmp(c + 1, x) == 0;
    }

    return false;
}

int parse_options(int argc, char *argv[])
{
    enum {
        DIAGNOSTICS_COLOR = 1000,
        DIAGNOSTICS_ELIDE_TAGS,
        DUMP_GRAPH,
        DUMP_OPERATIONS,
        DUMP_LOG,
        DUMP_SHORT_TAGS,
        DIAGNOSTICS_SHORTEN_TAGS,
        POLYHEDRON_BOOLEANS,
        STORE_COMPRESSION,
        STORE_THRESHOLD,
        REWRITE_PASS_LIMIT};

    static struct option options[] = {
        {"help", no_argument, 0, 'h'},

        // Debugging

        {"dump-abridged-tags", no_argument, &Flags::dump_abridged_tags, 1},
        {"no-dump-abridged-tags", no_argument, &Flags::dump_abridged_tags, 0},
        {"dump-annotations", no_argument, &Flags::dump_annotations, 1},
        {"no-dump-annotations", no_argument, &Flags::dump_annotations, 0},
        {"dump-graph", optional_argument, 0, DUMP_GRAPH},
        {"no-dump-graph", no_argument, 0, -DUMP_GRAPH},
        {"dump-operations", optional_argument, 0, DUMP_OPERATIONS},
        {"no-dump-operations", no_argument, 0, -DUMP_OPERATIONS},
        {"dump-log", optional_argument, 0, DUMP_LOG},
        {"no-dump-log", no_argument, 0, -DUMP_LOG},
        {"no-dump-short-tags", no_argument, &Options::dump_short_tags, -1},
        {"dump-short-tags", optional_argument, 0, DUMP_SHORT_TAGS},
        {"no-diagnostics-shorten-tags", no_argument, &Options::diagnostics_shorten_tags, -1},
        {"diagnostics-shorten-tags", optional_argument, 0, DIAGNOSTICS_SHORTEN_TAGS},

        // Diagnostics

        {"no-diagnostics-color", optional_argument, 0, -DIAGNOSTICS_COLOR},
        {"diagnostics-color", optional_argument, 0, DIAGNOSTICS_COLOR},
        {"no-diagnostics-elide-tags", no_argument, 0, -DIAGNOSTICS_ELIDE_TAGS},
        {"diagnostics-elide-tags", optional_argument, 0, DIAGNOSTICS_ELIDE_TAGS},

        // Evaluation

        {"threads", required_argument, 0, 't'},
        {"no-threads", no_argument, &Options::threads, 0},
        {"evaluate", no_argument, &Flags::evaluate, 1},
        {"no-evaluate", no_argument, &Flags::evaluate, 0},
        {"fold-transformations", no_argument, &Flags::fold_transformations, 1},
        {"no-fold-transformations", no_argument, &Flags::fold_transformations, 0},
        {"fold-booleans", no_argument, &Flags::fold_booleans, 1},
        {"no-fold-booleans", no_argument, &Flags::fold_booleans, 0},
        {"fold-flushes", no_argument, &Flags::fold_flushes, 1},
        {"no-fold-flushes", no_argument, &Flags::fold_flushes, 0},
        {"polyhedron-booleans", required_argument, 0, POLYHEDRON_BOOLEANS},
        {"eliminate-dead-operations", no_argument, &Flags::eliminate_dead_operations, 1},
        {"no-eliminate-dead-operations", no_argument, &Flags::eliminate_dead_operations, 0},
        {"store-operations", no_argument, &Flags::store_operations, 1},
        {"no-store-operations", no_argument, &Flags::store_operations, 0},
        {"load-operations", no_argument, &Flags::load_operations, 1},
        {"no-load-operations", no_argument, &Flags::load_operations, 0},
        {"store-compression", optional_argument, 0, STORE_COMPRESSION},
        {"no-store-compression", no_argument, &Options::store_compression, -1},
        {"rewrite-pass-limit", required_argument, 0, REWRITE_PASS_LIMIT},
        {"no-rewrite-pass-limit", no_argument, &Options::rewrite_pass_limit, -1},
        {"store-threshold", required_argument, 0, STORE_THRESHOLD},
        {"no-store-threshold", no_argument, &Options::store_threshold, 0},

        // Output

        {"output", required_argument, 0, 'o'},
        {"no-output", no_argument, &Flags::output, 0},
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

        // Scheme

#ifdef HAVE_SCHEME
        {"eliminate-tail-calls", no_argument, &Flags::eliminate_tail_calls, 1},
        {"no-eliminate-tail-calls", no_argument, &Flags::eliminate_tail_calls, 0},
#endif

        {0, 0, 0, 0}
    };

#define PUSH_SIMPLE_OPTION(X) {                 \
        Options::X.push_front(optarg);          \
        break;                                  \
    }

#define PUSH_OPTION(X, VAL) {                   \
        Options::X.push_front(VAL);             \
        break;                                  \
    }

#define STRING_OPTION(X) {                      \
        Options::X = optarg;                    \
        break;                                  \
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
        const bool p = !!std::strncmp(optarg, "no-", 3);                \
        bool is_group = false;                                          \
        optarg += (p ? 0 : 3);                                          \

#define WARN_OPTIONS_GROUP(X)                                   \
    if (!std::strncmp(optarg, #X, sizeof(#X) - 1)) {            \
        char *_s = optarg + sizeof(#X);                         \
        char *optarg = _s;                                      \
        is_group = (optarg[-1] == '\0');

#define WARN_OPTION_S(X, S)                     \
    if (is_group || !std::strcmp(optarg, S)) {  \
        Flags::warn_## X = p;                   \
        if (!is_group) {                        \
            break;                              \
        }                                       \
    }

#define WARN_OPTION(X) WARN_OPTION_S(X, #X)

#define WARN_OPTIONS_GROUP_END                                      \
        }                                                           \
                                                                    \
        if (is_group) {                                             \
            break;                                                  \
        }                                                           \
                                                                    \
        is_group = false;                                           \

#define WARN_OPTIONS_END                        \
    }

#define OPTION_END goto invalid_argument;

    int argc_max;

    // Leave out any arguments meant for the scripts.

    for (argc_max = 1;
         argc_max < argc && std::strcmp(argv[argc_max - 1], "--");
         argc_max++);

    // This allows us to run parse_options more than once (mostly in
    // tests).

    optind = 1;

    int n, option;
    while ((n = -1, option = getopt_long(
                argc_max, argv,
                "-ht:W:x:"
#ifdef HAVE_SCHEME
                "F:"
#endif
                "I:D:o:i",
                options, &n)) != -1) {
        switch (option) {
        case 1: {
            int (*run)(const char *input, char **first, char **last);

            if (Options::language == Language::LUA
                || (Options::language == Language::AUTO
                    && has_suffix(optarg, "lua"))) {
#ifdef HAVE_LUA
                run = run_lua;
#else
                std::cerr << argv[0] << ": "
                          << optarg
                          << (": Lua backend is not enabled; ignoring input "
                              "file\n");
                break;
#endif
            } else if (Options::language == Language::SCHEME
                       || (Options::language == Language::AUTO
                           && has_suffix(optarg, "scm"))) {
#ifdef HAVE_SCHEME
                run = run_scheme;
#else
                std::cerr << argv[0] << ": "
                          << optarg
                          << (": Scheme backend is not enabled; ignoring input "
                              "file\n");
                break;
#endif
            } else {
                std::cerr << argv[0] << ": "
                          << optarg
                          << (": cannot determine language from suffix; "
                              "please specify explicitly\n");
                break;
            }

            /* Load and evaluate the source file. */

            std::filesystem::path p(optarg);
            std::string s = p.filename();

            Options::include_directories.push_front(p.parent_path().native());

            begin_unit(s.c_str());

            if (run(optarg, argv + argc_max, argv + argc) != 0) {
                return -EXIT_FAILURE;
            }

            evaluate_unit();

            Options::include_directories.pop_front();

            break;
        }

        case 'h':
            std::cout
                << "Usage: " << argv[0] << " [OPTION...] FILE... [-- ARG...]\n\n"
                << ("Options:\n"
                    "  -h, --help            Display this help message.\n\n"

                    "Debugging options:\n"
                    "  --dump-operations[=FILE] Dump evaluated operations.\n"
                    "  --dump-log[=FILE]        Dump evaluation log.\n"
                    "  --dump-graph[=FILE]      Dump evaluation graph.\n"
                    "  --no-dump-abridged-tags  Do not substitute operands in dumped operation\n"
                    "                           tags with evaluation sequence numbers.\n"
                    "  --no-dump-annotations    Do not annotate dumped operations.\n"
                    "  --dump-short-tags[=N]    Limit the maximum length in dumped tags.\n\n"

                    "Evaluation options:\n"
                    "  -t N, --threads=N     Use no more than specified number of evaluation threads.\n"
                    "  --polyhedron-booleans=MODE\n"
                    "                        Set polyhedron boolean operation execution strategy.\n"
                    "                        MODE can be one of 'nef', 'auto'.\n"
                    "  --no-evaluate         Go through the motions, but don't evaluate anything.\n"
                    "  --no-fold-transformations\n"
                    "                        Disable transformation operation folding.\n"
                    "  --no-fold-booleans    Disable boolean operation folding.\n"
                    "  --no-fold-flushes     Disable flush operation folding.\n"
                    "  --no-eliminate-dead-operations\n"
                    "                        Do not skip evaluation of unneeded operations.\n"
                    "  --no-store-operations Do not store evaluated operations to disk.\n"
                    "  --no-load-operations  Do not load stored operations from disk.\n"
                    "  --store-compression[=LEVEL]\n"
                    "                        Compress stored operations.\n"
                    "  --no-store-compression Do not compress stored operations.\n"
                    "  --store-threshold[=N] Don't store operations with cumulative evaluation\n"
                    "                        time below the specified threshold (in seconds).\n"
                    "  --no-store-threshold  Store all operations, irrespective of evaluation time.\n"
                    "  --rewrite-pass-limit=N Perform at most N rewrite passes.\n\n"


                    "Output options:\n"
                    "  --output-stl, --stl   Write each defined output to a file, in STL format.\n"
                    "  --output-off, --off   Write each defined output to a file, in OFF format.\n"
                    "  --output-wrl, --wrl   Write each defined output to a file, in WRL format.\n"
                    "  -o FILE, --output=FILE Write output with the same name as FILE, sans the \n"
                    "                        suffix, in a format determined by the suffix.\n"
                    "  -o FILE:OUTPUT, --output=FILE:OUTPUT\n"
                    "                        Write output with name OUTPUT to file FILE, in a \n"
                    "                        format determined by the suffix.\n\n"

                    "Backend options:\n"
                    "  -x LANG               Specify the language of the following input files.\n"
                    "  -I DIR                Add the directory DIR to the list of directories\n"
                    "                        to be searched for modules or libraries.\n"
                    "  -D NAME               Predefine global variable with a value of true (or set\n"
                    "                        its value if it is already defined).\n"
                    "  -D NAME=VALUE         Predefine global variable with the given value (or set\n"
                    "                        its value if it is already defined).  The value is\n"
                    "                        interpreted in the context of the active language.\n\n"

#ifdef HAVE_SCHEME
                    "Scheme backend options:\n"
                    "  -F FEAT               Add the feature FEAT to the list of Scheme features.\n"
                    "  --no-eliminate-tail-calls\n"
                    "                        Disable tail-call elimination.\n"
#endif

                    "Diagnostics options:\n"
                    "  --diagnostics-color[=WHEN]\n"
                    "                        Colorize diagnostics output. WHEN can be one of\n"
                    "                        'always', 'never', 'auto'.\n"
                    "  --diagnostics-elide-tags[=DEPTH]\n"
                    "                        Elide tags in diagnostics, past the specified level.\n"
                    "  --diagnostics-shorten-tags[=N]\n"
                    "                        Limit the maximum length of tags in diagnostics.\n"

                    "  -Wfatal-errors        Discontinue evaluation after the first error.\n"
                    "  -Werror               Treat all warnings as errors.\n"

                    "  -Wduplicate           Warn if an operation is instantiated multiple times.\n"
                    "  -Wmanifold            Warn if an operation results in a non-manifold mesh.\n"

                    "  -Wmesh                Enable all -Wmesh- warnings.\n"
                    "  -Wmesh-valid          Warn if an operation results in an invalid mesh.\n"
                    "  -Wmesh-closed         Warn if an operation results in a mesh with holes.\n"
                    "  -Wmesh-manifold       Warn if an operation results in a non-manifold mesh.\n"
                    "  -Wmesh-degenerate     Warn if an operation results in a mesh with degenerate\n"
                    "                        faces or edges.\n"
                    "  -Wmesh-intersects     Warn if an operation results in a self-intersecting mesh.\n"
                    "  -Wmesh-bounds         Warn if an operation results in a mesh that doesn't \n"
                    "                        bound a volume.\n"
                    "  -Wmesh-oriented       Warn if an operation results in a mesh that is not\n"
                    "                        oriented outwards.\n"

                    "  -Wnef                 Warn if a polyhedron operation could have been\n"
                    "                        executed without using Nef polyhedra.\n"
                    "  -Wunused              Warn if an operation is instantiated, but not used.\n"
                    "  -Wstore               Warn if an operation is stored.\n"
                    "  -Wload                Warn if an operation is loaded.\n"
                    "  -Woutputs             Warn if an output is left unused.\n")
                << std::endl;

            std::exit(EXIT_SUCCESS);
            break;

        case -DIAGNOSTICS_COLOR:
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
            Options::diagnostics_elide_tags = -1;
            break;

        case DIAGNOSTICS_ELIDE_TAGS:
            OPTIONAL_ARGUMENT(diagnostics_elide_tags, 1);
            INTEGER_OPTION(diagnostics_elide_tags, i >= 0);

        case DUMP_GRAPH:
            OPTIONAL_ARGUMENT(dump_graph, "\0");
            [[fallthrough]];
        case -DUMP_GRAPH:
            STRING_OPTION(dump_graph);

        case DUMP_OPERATIONS:
            OPTIONAL_ARGUMENT(dump_operations, "\0");
            [[fallthrough]];
        case -DUMP_OPERATIONS:
            STRING_OPTION(dump_operations);

        case DUMP_LOG:
            OPTIONAL_ARGUMENT(dump_log, "\0");
            [[fallthrough]];
        case -DUMP_LOG:
            STRING_OPTION(dump_log);

        case DUMP_SHORT_TAGS:
            OPTIONAL_ARGUMENT(dump_short_tags, 50);
            INTEGER_OPTION(dump_short_tags, i >= 0);

        case DIAGNOSTICS_SHORTEN_TAGS:
            OPTIONAL_ARGUMENT(diagnostics_shorten_tags, 50);
            INTEGER_OPTION(diagnostics_shorten_tags, i >= 0);

        case POLYHEDRON_BOOLEANS:
            NOMINAL_OPTION(polyhedron_booleans,
                           "corefine", Polyhedron_booleans_mode::COREFINE);
            NOMINAL_OPTION(polyhedron_booleans,
                           "nef", Polyhedron_booleans_mode::NEF);
            NOMINAL_OPTION(polyhedron_booleans,
                           "auto", Polyhedron_booleans_mode::AUTO);
            OPTION_END;

        case STORE_COMPRESSION:
            OPTIONAL_ARGUMENT(store_compression, 6);
            INTEGER_OPTION(store_compression, i >= 0 && i <= 9);

        case STORE_THRESHOLD:
            INTEGER_OPTION(store_threshold, i >= 0);

        case REWRITE_PASS_LIMIT:
            INTEGER_OPTION(rewrite_pass_limit, i >= 0);

        case 't': INTEGER_OPTION(threads, i >= 0);

        case 'W':
            WARN_OPTIONS_BEGIN;

            WARN_OPTION_S(fatal_errors, "fatal-errors");
            WARN_OPTION(error);

            WARN_OPTION(duplicate);
            WARN_OPTION(manifold);
            WARN_OPTION(nef);
            WARN_OPTION(unused);
            WARN_OPTION(store);
            WARN_OPTION(load);
            WARN_OPTION(outputs);

            WARN_OPTIONS_GROUP(mesh);
            WARN_OPTION_S(mesh_valid, "valid");
            WARN_OPTION_S(mesh_closed, "closed");
            WARN_OPTION_S(mesh_manifold, "manifold");
            WARN_OPTION_S(mesh_degenerate, "degenerate");
            WARN_OPTION_S(mesh_degenerate, "degenerate");
            WARN_OPTION_S(mesh_intersects, "intersects");
            WARN_OPTION_S(mesh_bounds, "bounds");
            WARN_OPTION_S(mesh_oriented, "oriented");
            WARN_OPTIONS_GROUP_END;

            WARN_OPTIONS_END;
            OPTION_END;

        case 'x':
            OPTIONAL_ARGUMENT(language, Language::AUTO);

#ifdef HAVE_LUA
            NOMINAL_OPTION(language, "lua", Language::LUA);
#endif

#ifdef HAVE_SCHEME
            NOMINAL_OPTION(language, "scheme", Language::SCHEME);
#endif

            NOMINAL_OPTION(language, "auto", Language::AUTO);
            OPTION_END;

        case 'I':
            PUSH_SIMPLE_OPTION(include_directories);

        case 'D':
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

#ifdef HAVE_SCHEME
        case 'F':
            PUSH_SIMPLE_OPTION(scheme_features);
#endif

        case 'o':
            if (!optarg) {
                Flags::output = 1;
                break;
            }

            PUSH_SIMPLE_OPTION(outputs);

        case '?':
            return -EXIT_FAILURE;
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

        return -EXIT_FAILURE;
    }

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
