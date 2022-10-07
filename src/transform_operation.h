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

#ifndef TRANSFORM_OPERATION_H
#define TRANSFORM_OPERATION_H

#include "basic_operations.h"

template<typename T, typename A>
class Transform_operation: public Sequentially_foldable_operation<T> {
    template<template<typename> typename, template<typename> typename, typename>
    friend bool try_fold_transformation(Operation *x);

protected:
    A transformation;

public:
    Transform_operation(
        const std::shared_ptr<T> &p,
        const A &X):
        Sequentially_foldable_operation<T>(p), transformation(X) {}

    std::string describe() const override {
        return compose_tag("transform", this->operand, transformation);
    }

    bool fold_operand(const T *p) override {
        const Transform_operation<T, A> *t =
            dynamic_cast<const Transform_operation<T, A> *>(p);

        if (!t) {
            return false;
        }

        this->transformation = this->transformation * t->transformation;

        return true;
    }
};

#endif
