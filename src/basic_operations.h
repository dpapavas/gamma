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

#ifndef BASIC_OPERATIONS_H
#define BASIC_OPERATIONS_H

#include <memory>
#include <mutex>

#include "operation.h"

// Define some generic operation types.

class Threadsafe_operation: public Operation {
public:
    const bool threadsafe;

    Threadsafe_operation(): threadsafe(true) {}
    Threadsafe_operation(const bool p): threadsafe(p) {}
};

template<typename T>
class Source_operation: public T {
public:
    void link() override {}
};

class Sink_operation: public Operation {};

template<typename T, typename U = T>
class Unary_operation: public U {
protected:
    std::shared_ptr<T> operand;

public:
    Unary_operation(const std::shared_ptr<T> &x): operand(x) {}

    void link() override {
        this->predecessors.insert(operand.get());
        operand->successors.insert(this);
    }
};

template<typename T, typename U = T>
class Binary_operation: public U {
protected:
    std::shared_ptr<T> first;
    std::shared_ptr<T> second;

public:
    Binary_operation(
        const std::shared_ptr<T> &a, const std::shared_ptr<T> &b):
        first(a), second(b) {}

    void link() override {
        this->predecessors.insert(first.get());
        this->predecessors.insert(second.get());

        first->successors.insert(this);
        second->successors.insert(this);
    }
};

template<typename T, typename U = T>
class Nary_operation: public U {
protected:
    std::vector<std::shared_ptr<T>> operands;

public:
    using U::U;

    Nary_operation(std::vector<std::shared_ptr<T>> &&v):
        operands(std::move(v)) {}

    void link() override {
        for (const auto &x: operands) {
            this->predecessors.insert(x.get());
            x->successors.insert(this);
        };
    }

    void push_back(
        const std::shared_ptr<T> &p) {
        operands.push_back(p);
    }
};

template<typename T>
class Sequentially_foldable_operation: public Unary_operation<T> {
    template<template<typename> typename, template<typename> typename, typename>
    friend bool try_fold_sequentially(Operation *x);

public:
    using Unary_operation<T>::Unary_operation;

    bool try_fold();
    virtual bool fold_operand(const T *p) = 0;
};

#endif
