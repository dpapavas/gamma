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

#ifndef OPTIONS_H
#define OPTIONS_H

#include <forward_list>
#include <string>
#include <unistd.h>

enum class Polyhedron_booleans_mode {
    AUTO,
    COREFINE,
    NEF,
};

enum class Diagnostics_color_mode {
    AUTO,
    ALWAYS,
    NEVER,
};

enum class Language {
    AUTO,
    LUA,
    SCHEME,
};

#define ANSI_COLOR(i, j) (                                              \
    Options::diagnostics_color == Diagnostics_color_mode::ALWAYS        \
    || (Options::diagnostics_color == Diagnostics_color_mode::AUTO      \
        && isatty(1) && isatty(2))                                      \
    ? "\033[" #i ";" #j "m" : "")

namespace Flags {
    // Debugging

    extern int dump_abridged_tags;
    extern int dump_annotations;

    // Diagnostics

    extern int warn_fatal_errors;
    extern int warn_error;

    extern int warn_duplicate;
    extern int warn_manifold;
    extern int warn_nef;
    extern int warn_unused;
    extern int warn_store;
    extern int warn_load;
    extern int warn_outputs;

    extern int warn_mesh_valid;
    extern int warn_mesh_closed;
    extern int warn_mesh_manifold;
    extern int warn_mesh_degenerate;
    extern int warn_mesh_intersects;
    extern int warn_mesh_bounds;
    extern int warn_mesh_oriented;

    // Evaluation

    extern int evaluate;
    extern int fold_transformations;
    extern int fold_booleans;
    extern int fold_flushes;
    extern int eliminate_dead_operations;
    extern int store_operations;
    extern int load_operations;

    // Output

    extern int output;
    extern int output_stl;
    extern int output_off;
    extern int output_wrl;

    // Scheme backend

    extern int eliminate_tail_calls;
}

namespace Options {
    // Debugging

    extern const char *dump_graph;
    extern const char *dump_operations;
    extern const char *dump_log;
    extern int dump_short_tags;
    extern int diagnostics_shorten_tags;

    // Diagnostics

    extern Diagnostics_color_mode diagnostics_color;
    extern int diagnostics_elide_tags;

    // Evaluation

    extern Language language;
    extern int threads;
    extern int rewrite_pass_limit;
    extern int store_compression;
    extern int store_threshold;

    // Output

    extern std::forward_list<std::string> outputs;

    // Warnings

    extern Polyhedron_booleans_mode polyhedron_booleans;

    // Backend

    extern std::forward_list<std::pair<std::string, std::string>> definitions;
    extern std::forward_list<std::string> include_directories;
    extern std::forward_list<std::string> scheme_features;
}

int parse_options(int argc, char* argv[]);

#endif
