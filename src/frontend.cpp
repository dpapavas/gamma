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

#include <forward_list>

#include "kernel.h"
#include "macros.h"
#include "boxed_operations.h"

void print_message(Operation::Message_level level, const char *s, const int n)
{
    // Create a dummy operation type, to get the current source location.

    class: public Operation {
    public:
        void link() override {
            assert_not_reached();
        }

        std::string describe() const override {
            assert_not_reached();
        }

        void evaluate() override {
            assert_not_reached();
        }
    } op;

    // Print the message.

    if (auto f = op.annotations.find("file"); f != op.annotations.end()) {
        switch (level) {
        case Operation::NOTE: std::cerr << ANSI_COLOR(1, 32); break;
        case Operation::WARNING: std::cerr << ANSI_COLOR(1, 33); break;
        case Operation::ERROR: std::cerr << ANSI_COLOR(1, 31); break;
        }

        std::cerr << f->second << ANSI_COLOR(0, 37) << ":";
    }

    if (auto l = op.annotations.find("line"); l != op.annotations.end()) {
        std::cerr << ANSI_COLOR(1, 37) << l->second << ANSI_COLOR(0, 37)
                  << ": ";
    }

    std::cerr.write(s, n);
    std::cerr << std::endl;
}

void add_output_operations(std::string name, std::vector<Boxed_polyhedron> &v)
{
    const std::string suffixes[] = {"", ".stl", ".off", ".wrl"};
    const int flags[] = {
        Flags::output, Flags::output_stl, Flags::output_off, Flags::output_wrl};

    std::forward_list<std::pair <const std::string, int>> outputs;
    const int n = sizeof(suffixes) / sizeof(suffixes[0]);
    assert(n == sizeof(flags) / sizeof(flags[0]));

    // Output can be enabled per-format, e.g. with the --output-stl
    // option.  In which case an output with name `foo` will be
    // written into file `foo.stl`.

    for (int i = 0; i < n; i++) {
        if (flags[i]) {
            outputs.push_front({name + suffixes[i], i});
        }
    }

    // Specific outputs can also be enabled eg. with the
    // -o/--output=spec options.  An output spec can be of the form of
    // the form `foo.ext`, or `foo.ext:bar`.  The first form writes
    // the output with name `foo` to the file `foo.ext`, with a format
    // determined by the extension.  The second form, works in the
    // same way, but it explicitly selects the output with name `bar`.

    for (const auto x: Options::outputs) {
        if (std::size_t j = x.find_first_of(':');
            j != std::string::npos) {

            if (x.compare(j + 1, std::string::npos, name)) {
                continue;
            }

            for (int i = 0; i < n; i++) {
                const std::size_t m = suffixes[i].size();

                if (j >= m && !x.compare(j - m, m, suffixes[i])) {
                    outputs.push_front({x.substr(0, j), i});
                }
            }
        } else {
            const std::size_t m = name.size();

            if (x.compare(0, m, name)) {
                continue;
            }

            for (int i = 0; i < n; i++) {
                if (!x.compare(m, std::string::npos, suffixes[i])) {
                    outputs.push_front({x, i});
                }
            }
        }
    }

    if (outputs.empty()) {
        if (Flags::warn_outputs) {
            std::ostringstream s;
            s << "unused output '" << name << '\'';

            std::string t = s.str();
            print_message(Operation::WARNING, t.c_str(), t.size());
        }

        return;
    }

    // If some outputs have been enabled, convert it to a mesh and create the
    // required write operations.

    std::vector<std::shared_ptr<Polyhedron_operation<Surface_mesh>>> w;
    w.reserve(v.size());

    for (const auto &p: v) {
        w.push_back(
            std::visit(
                [](auto &&x) {
                    return CONVERT_TO<Surface_mesh,
                                      Polyhedron_operation<Surface_mesh>>(x);
                }, p));
    }

    for (const auto &[s, i]: outputs) {
        switch (i) {
            case 0:
            PIPE(s.c_str(), std::vector(w));
            break;

            case 1:
            WRITE_STL(s.c_str(), std::vector(w));
            break;

            case 2:
            WRITE_OFF(s.c_str(), std::vector(w));
            break;

            case 3:
            WRITE_WRL(s.c_str(), std::vector(w));
            break;
        }
    }
}
