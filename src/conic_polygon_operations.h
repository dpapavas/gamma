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

#ifndef CONIC_POLYGON_OPERATIONS_H
#define CONIC_POLYGON_OPERATIONS_H

#include "tolerances.h"
#include "circle_polygon_types.h"
#include "conic_polygon_types.h"
#include "basic_operations.h"
#include "polygon_operations.h"

void convert_conic_polygon_set(const Conic_polygon_set &S, Polygon_set &T,
                               const double tolerance);

template<typename T>
class Conic_polygon_convert_operation:
    public Unary_operation<Polygon_operation<T>,
                           Polygon_operation<Conic_polygon_set>> {

public:
    using Unary_operation<Polygon_operation<T>,
                          Polygon_operation<Conic_polygon_set>>::Unary_operation;

    std::string describe() const override {
        return compose_tag("conics", this->operand);
    }
};

template<>
class Polygon_convert_operation<Conic_polygon_set, Polygon_set>:
    public Conic_polygon_convert_operation<Polygon_set> {

public:
    using Conic_polygon_convert_operation<Polygon_set>::Conic_polygon_convert_operation;

    void evaluate() override;
};

template<>
class Polygon_convert_operation<Conic_polygon_set, Circle_polygon_set>:
    public Conic_polygon_convert_operation<Circle_polygon_set> {

public:
    using Conic_polygon_convert_operation<Circle_polygon_set>::Conic_polygon_convert_operation;

    void evaluate() override;
};

template<>
class Polygon_convert_operation<Polygon_set, Conic_polygon_set>:
    public Unary_operation<Polygon_operation<Conic_polygon_set>,
                           Polygon_operation<Polygon_set>> {

    FT tolerance;

public:
    Polygon_convert_operation<Polygon_set, Conic_polygon_set>(
        const std::shared_ptr<Polygon_operation<Conic_polygon_set>> &x):
        Unary_operation<Polygon_operation<Conic_polygon_set>,
                        Polygon_operation<Polygon_set>>(x), tolerance(Tolerances::curve) {}

    void evaluate() override;

    std::string describe() const override {
        return compose_tag("segments", this->operand, tolerance);
    }
};

#endif
