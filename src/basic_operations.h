// Copyright 2022, 2026 Dimitris Papavasiliou

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

#ifndef BASIC_OPERATIONS_H
#define BASIC_OPERATIONS_H

#include <memory>
#include <mutex>

#include "operation.h"

// Document: program

// ## Basic Operation Classes

// Below are base classes for operations that take 0, 1, 2, or more
// *operations* as arguments.  They may take other values besides; a
// tranlation operation like `transform(sphere(1),@/translation(1,0,0))`
// is still a *unary* operation.  The translation argument, even
// though not a simple number, is still a known constant.  It does not
// need to be evaluated first, in order to evaluate the result of the
// translation operation.  The sphere argument, on the other hand,
// *does*.

// The constructor of each base class take care of linking the
// operation to the graph (forming *predecessor* links to their
// arguments and, in turn, *successor* links to themselves in those).
// They also hold shared pointers to their arguments, since they
// depend on them for their result.  Ultimately, these pointers along
// with pointers kept to sunk operations (ref: Sunk Operations)
// should be the only references preventing destruction.

// Each class also implements the `reset` member function.  Ref:
// The Base Operation Class.

template<typename T>
class Source_operation: public T {
    void reset() override {}
};

template<typename T, typename U = T>
class Unary_operation: public U {
public:
    std::shared_ptr<T> operand;

    Unary_operation(const std::shared_ptr<T> &x): operand(x) {
        operand->link_to(this);
    }

    void reset() override {
        operand.reset();
    }
};

template<typename T, typename U = T>
class Binary_operation: public U {
public:
    std::shared_ptr<T> first;
    std::shared_ptr<T> second;

    Binary_operation(
        const std::shared_ptr<T> &a, const std::shared_ptr<T> &b):
        first(a), second(b) {
        first->link_to(this);
        second->link_to(this);
    }

    void reset() override {
        first.reset();
        second.reset();
    }
};

template<typename T, typename U = T>
class Nary_operation: public U {
public:
    std::vector<std::shared_ptr<T>> operands;

    using U::U;

    Nary_operation(std::vector<std::shared_ptr<T>> &&v):
        operands(std::move(v)) {
        for (const auto &x: operands) {
            x->link_to(this);
        };
    }

    void reset() override {
        for (auto &x: operands) {
            x.reset();
        }
    }

    void push_back(const std::shared_ptr<T> &p) {
        operands.push_back(p);
        p->link_to(this);
    }
};

// ### Other Base Classes

// An operation having this base is obtains a flag, `threadsafe,`
// marking it as safe for multi-threaded evaluation.  Not all CGAL
// operations are thread-safe, so we need to arrange to evaluate some
// of them in the main thread.  Ref: ready list.

class Threadsafe_operation: public Operation {
public:
    const bool threadsafe;

    Threadsafe_operation(): threadsafe(true) {}
    Threadsafe_operation(const bool p): threadsafe(p) {}
};

// Document: none

#endif
