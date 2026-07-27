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

#ifndef TRANSFORM_OPERATION_H
#define TRANSFORM_OPERATION_H

#include "basic_operations.h"

template<typename T, typename A>
class Transform_operation: public Unary_operation<T> {
public:
    A transformation;

    Transform_operation(
        const std::shared_ptr<T> &p,
        const A &X):
        Unary_operation<T>(p), transformation(X) {}

    std::string describe() const override {
        return compose_tag("transform", this->operand, transformation);
    }
};

#endif
