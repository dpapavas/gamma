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

#ifndef CIRCLE_POLYGON_OPERATIONS_H
#define CIRCLE_POLYGON_OPERATIONS_H

#include "tolerances.h"
#include "circle_polygon_types.h"
#include "basic_operations.h"
#include "polygon_operations.h"

void convert_circle_polygon_set(const Circle_polygon_set &S, Polygon_set &T,
                                const double tolerance);

template<>
class Polygon_convert_operation<Circle_polygon_set, Polygon_set>:
    public Unary_operation<Polygon_operation<Polygon_set>,
                           Polygon_operation<Circle_polygon_set>> {

public:
    using Unary_operation<Polygon_operation<Polygon_set>,
                          Polygon_operation<Circle_polygon_set>>::Unary_operation;

    void evaluate() override;

    std::string describe() const override {
        return compose_tag("circles", this->operand);
    }
};

template<>
class Polygon_convert_operation<Polygon_set, Circle_polygon_set>:
    public Unary_operation<Polygon_operation<Circle_polygon_set>,
                           Polygon_operation<Polygon_set>> {

    FT tolerances[2];

public:
    Polygon_convert_operation<Polygon_set, Circle_polygon_set>(
        const std::shared_ptr<Polygon_operation<Circle_polygon_set>> &x):
        Unary_operation<Polygon_operation<Circle_polygon_set>,
                        Polygon_operation<Polygon_set>>(x),
        tolerances{Tolerances::curve, Tolerances::projection} {}

    void evaluate() override;

    std::string describe() const override {
        return compose_tag("segments", this->operand, tolerances);
    }
};

class Circle_operation:
    public Source_operation<Polygon_operation<Circle_polygon_set>> {

    FT radius;

public:
    Circle_operation(const FT &r): radius(r) {}

    std::string describe() const override {
        return compose_tag("circle", radius);
    }

    void evaluate() override;
};

class Circular_segment_operation:
    public Source_operation<Polygon_operation<Circle_polygon_set>> {

    FT chord, sagitta;

public:
    Circular_segment_operation(const FT &c, const FT &h):
        chord(c), sagitta(h) {}

    std::string describe() const override {
        return compose_tag("segment", chord, sagitta);
    }

    void evaluate() override;
};

class Circular_sector_operation:
    public Source_operation<Polygon_operation<Circle_polygon_set>> {

    FT radius, angle;

public:
    Circular_sector_operation(const FT &r, const FT &a):
        radius(r), angle(a) {}

    std::string describe() const override {
        return compose_tag("sector", radius, angle);
    }

    void evaluate() override;
};

#endif
