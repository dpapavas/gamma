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

#include <cstdint>
#include <cstring>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <mutex>

#include <CGAL/exceptions.h>

#include "options.h"
#include "operation.h"

// ---

// ## Operation Evaluation Messages

// This member function takes care of outputting a message regarding
// itself to the user.  This is typically done during evaluation of
// the operation, to notify the user about something that transpired
// during the course of it (typically a warning, error or simply
// noteworthy information).

static std::mutex mutex;

void Operation::message(Message_level level, const std::string &message) const
{
    std::string t;

    // As the evaluation progresses, there tend to form longer and
    // longer strings of nested operations, whose tags are
    // consequently quite long.  For instance, even for a simple
    // program calculating a hollow sphere, the final operation would
    // be tagged with something like
    // `'write_off("a.off",mesh(difference(sphere(2,1/15,@/ 1/1000000),
    // sphere(1,1/15,1/1000000))))'`.

    // Such tags are hard for the programmer to parse and are not very
    // useful as parts of diagnostic messages.  We therefore usualy
    // *elide* tags, by substituting operations deeper than a given
    // level with an ellipsis.

    // With `--diagnostics-elide-tags=2`, the above tag would
    // therefore become `'write_off("a.off",@/ mesh(difference(...)))'`.

    if (Options::diagnostics_elide_tags < 0) {
        t = tag;
    } else {
        std::ostringstream s;

        int i = 0;
        for (const char d: tag) {
            if (d == ')') {
                i -= 1;
            }

            if (i <= Options::diagnostics_elide_tags) {
                s.put(d);
            }

            if (d == '(') {
                i += 1;

                if (i == Options::diagnostics_elide_tags + 1) {
                    s.write("...", 3);
                }
            }
        }

        t = s.str();
    }

    // Even after elision, tags can still be too long (consider a
    // union of 100 translated spheres).  We therefore also shorthen
    // the tag, to a given maximum length.

    if (Options::diagnostics_shorten_tags >= 0
        && static_cast<int>(t.size()) > Options::diagnostics_shorten_tags) {
        t.erase(Options::diagnostics_shorten_tags, std::string::npos);
        t += "...";
    }

    // Now, assuming the operation has been annotated by the front end
    // with source file `hello.scm`, line number 42 and column number
    // 6, we output the error message `"No world to greet"` as:

    // ```
    // hello.scm:42:6: in operation 'greet(world)'
    // hello.scm:42:6: error: No world to greet
    // ```

    // Normally, operations will alway have a tag (`'greet(world)'` in
    // the example above), which will allow us to print the first
    // line.  As a special case (ref: Program Messages), we allow
    // dummy operations, which are instantiated temporarily for the
    // purpose of printing messages not related to any operations.
    // These don't have a tag and we simply skip the first line.

    // We print the location and message tag below.  Since multiple
    // workers might be printing messages concurrently, we use a mutex
    // to synchronize output to the console.

    std::lock_guard<std::mutex> lock(mutex);

    for (int i = t.empty(); i < 2; i++) {
        if (auto f = annotations.find("file"); f != annotations.end()) {
            switch (level) {
            case NOTE: std::cerr << ANSI_COLOR(1, 32); break;
            case WARNING: std::cerr << ANSI_COLOR(1, 33); break;
            case ERROR: std::cerr << ANSI_COLOR(1, 31); break;
            }

            std::cerr << f->second << ANSI_COLOR(0, 37) << ":";
        }

        for (const char *x: {"line", "column"}) {
            if (auto l = annotations.find(x); l != annotations.end()) {
                std::cerr << ANSI_COLOR(1, 37)
                          << l->second << ANSI_COLOR(0, 37)
                          << ": ";
            }
        }

        if (i == 0) {
            std::cerr << "in operation '" << t << "'\n";
        }
    }

    // This is followed by the message kind.

    switch (level) {
    case NOTE:
        std::cerr << ANSI_COLOR(1, 32) << "note"
                  << ANSI_COLOR(0, 37) << ": ";
        break;
    case WARNING:
        std::cerr << ANSI_COLOR(1, 33) << "warning"
                  << ANSI_COLOR(0, 37) << ": ";
        break;
    case ERROR:
        std::cerr << ANSI_COLOR(1, 31) << "error"
                  << ANSI_COLOR(0, 37) << ": ";
        break;
    }

    // Finally, we output the message.  It can be useful to refer to
    // the operation itself in the message, so we substitue a `%` in
    // the message string with the operation tag.

    for (auto c = message.cbegin(); c != message.cend(); c++) {
        if (*c != '%') {
            std::cerr.put(*c);
        } else if (c + 1 != message.cend() && *(c + 1) == '%') {
            std::cerr.put(*c++);
        } else {
            std::cerr << '\'' << ANSI_COLOR(1, 37) << t
                      << ANSI_COLOR(0, 37) << '\'';
        }
    }

    std::cerr << std::endl;

    // If the user specified `-Werror`, we turn a warning into the
    // error.

    if (Flags::warn_error && level == WARNING) {
        throw operation_warning_error("previous warning treated as error");
    }
}

// ## Operation Dispatch

// To dispatch an operation is to produce its result its result.  This
// can happen in one of two (or, more precisely, three) ways.

bool Operation::dispatch()
{
    // If `--dry-run` has been specified, we don't evaluate the
    // operation at all.  The evaluation process doesn't differ in all
    // other respects; it just doesn't calculate anything.

    if (Flags::dry_run) {
        return false;
    }

    // If the operation is loadable, we attempt to load it from its
    // store.

    if (loadable) {
        if (load()) {
            annotations.insert({"loaded", store_path});

            if (Flags::warn_load) {
                message(WARNING, "Operation % was loaded");
            }

            return false;
        }

        return true;
    }

    // If no such store exists, we attempt to evaluate it.  We also
    // measure the time needed to do so and annotate the operation
    // with this time.

    auto t_0 = std::chrono::steady_clock::now();
    evaluate();
    float delta = std::chrono::duration_cast<std::chrono::duration<float>>(
        std::chrono::steady_clock::now() - t_0).count();

    {
        std::ostringstream s;
        s.precision(2);
        s << delta << "s";

        annotations.insert({"in", s.str()});
    }

    // This time also counts towards the operation's cost, which also
    // includes the time needed to calculate all its ancestors.  In
    // other words, it is the time we would need to produce the
    // operation's result from scratch.

    cost += delta;

    {
        std::ostringstream s;
        s.precision(2);
        s << cost << "s";

        annotations.insert({"cost", s.str()});
    }

    // If operation storing is enabled and the cost exceeds a small
    // threshold value (which is there so as not to spend more time
    // storing and loading an operation than we would've spent
    // evaluating) we also store the operation.

    if (Flags::store_operations
        && cost > Options::store_threshold) {
        if (store()) {
            annotations.insert({"stored", store_path});

            if (Flags::warn_store) {
                message(WARNING, "Operation % was stored");
            }
        }
    }

    return false;
}
