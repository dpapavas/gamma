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

#ifndef POLYGON_OPERATIONS_H
#define POLYGON_OPERATIONS_H

#include "kernel.h"
#include "tolerances.h"
#include "polygon_types.h"
#include "transformation_types.h"
#include "basic_operations.h"
#include "transform_operation.h"

// Apply function fb to boundaries and fh to the holes of all polygons
// in set S.

template<typename T>
void for_each_polygon(
    const T &S,
    std::function<void (const typename T::Polygon_2 &)> fb,
    std::function<void (const typename T::Polygon_2 &)> fh)
{
    const int n = S.number_of_polygons_with_holes();
    std::vector<typename T::Polygon_with_holes_2> v;

    v.reserve(n);
    S.polygons_with_holes(std::back_inserter(v));

    for (const typename T::Polygon_with_holes_2 &P: v) {
        fb(P.outer_boundary());

        for (auto H = P.holes_begin();
             H != P.holes_end();
             fh(*H), ++H);
    }
}

template<typename T>
void for_each_polygon(
    const T &S,
    std::function<void (const typename T::Polygon_2 &)> f)
{
    for_each_polygon<T>(S, f, f);
}

// Apply function fb to boundaries and fh to the holes of all polygons in
// set S, producing set R, in the style of std::transform.

template<typename U, typename T>
void transform_polygon_set(
    const U &S, T &R,
    std::function<typename T::Polygon_2(const typename U::Polygon_2 &)> fb,
    std::function<typename T::Polygon_2(const typename U::Polygon_2 &)> fh)
{
    const int n = S.number_of_polygons_with_holes();
    std::vector<typename U::Polygon_with_holes_2> v;

    v.reserve(n);
    S.polygons_with_holes(std::back_inserter(v));

    for (const typename U::Polygon_with_holes_2 &P: v) {
        typename T::Polygon_with_holes_2 Q(fb(P.outer_boundary()));

        for (auto H = P.holes_begin(); H != P.holes_end(); ++H) {
            Q.add_hole(fh(*H));
        }

        R.insert(Q);
    }
}

template<typename U, typename T>
void transform_polygon_set(
    const U &S, T &R,
    std::function<typename T::Polygon_with_holes_2(
                               const typename U::Polygon_with_holes_2 &)> f)
{
    const int n = S.number_of_polygons_with_holes();
    std::vector<typename U::Polygon_with_holes_2> v;

    v.reserve(n);
    S.polygons_with_holes(std::back_inserter(v));

    for (const typename U::Polygon_with_holes_2 &P: v) {
        R.insert(f(P));
    }
}

// Same as above but applying f to both holes and boundaries.

template<typename U, typename T>
void transform_polygon_set(
    const U &S, T &R,
    std::function<typename T::Polygon_2(const typename U::Polygon_2 &)> f)
{
    transform_polygon_set<U, T>(S, R, f, f);
}

template<typename T>
class Polygon_operation: public Operation {
protected:
    std::shared_ptr<T> polygon;

public:
    Polygon_operation(): polygon(nullptr) {}

    bool dispatch() override {
        bool p = Operation::dispatch();

        if (polygon) {
            const int n = polygon->number_of_polygons_with_holes();
            int h = 0, v = 0;

            for_each_polygon(
                *polygon,
                [&h, &v](const typename T::Polygon_2 &G) {
                    v += G.size();
                },
                [&h, &v](const typename T::Polygon_2 &G) {
                    v += G.size();
                    h += 1;
                });

            annotations.insert({
                    {"polygons", std::to_string(n)},
                    {"vertices", std::to_string(v)},
                    {"holes", std::to_string(h)}});
        }

        return p;
    }

    std::shared_ptr<T> get_value() const {
        assert(polygon);
        return polygon;
    };

    bool store() override;
    bool load() override;
};

// Primitives

class Ngon_operation:
    public Source_operation<Polygon_operation<Polygon_set>> {

    std::vector<Point_2> points;

public:
    Ngon_operation(std::vector<Point_2> &&v): points(std::move(v)) {}

    std::string describe() const override {
        return compose_tag("polygon", points);
    }

    void evaluate() override;
};

class Regular_polygon_operation:
    public Source_operation<Polygon_operation<Polygon_set>> {
    const int sides;
    const FT radius, tolerance;

public:
    Regular_polygon_operation(const int n, const FT &r):
        sides(n), radius(r), tolerance(Tolerances::projection) {}

    std::string describe() const override {
        return compose_tag("regular_polygon", sides, radius, tolerance);
    }

    void evaluate() override;
};

// Transform and conversion bases

template<typename T>
class Polygon_transform_operation:
    public Transform_operation<Polygon_operation<T>,
                               Aff_transformation_2> {

public:
    using Transform_operation<Polygon_operation<T>,
                              Aff_transformation_2>::Transform_operation;

    void evaluate() override;
};

class Polygon_flush_operation:
    public Sequentially_foldable_operation<Polygon_operation<Polygon_set>> {

    FT coefficients[2][2];

public:
    Polygon_flush_operation(
        const std::shared_ptr<Polygon_operation<Polygon_set>> &p,
        const FT &lambda, const FT &mu):
        Sequentially_foldable_operation<Polygon_operation<Polygon_set>>(p),
        coefficients{
            {CGAL::min(lambda, FT(0)), CGAL::max(lambda, FT(0))},
            {CGAL::min(mu, FT(0)),CGAL::max(mu, FT(0))}} {}

    std::string describe() const override {
        return compose_tag(
            "flush", this->operand, coefficients[0], coefficients[1]);
    }

    void evaluate() override;
    bool fold_operand(const Polygon_operation<Polygon_set> *p) override;
};

template<typename T, typename U>
class Polygon_convert_operation:
    public Unary_operation<Polygon_operation<U>,
                           Polygon_operation<T>> {};

// Convex hull

class Polygon_hull_operation:
    public Nary_operation<Polygon_operation<Polygon_set>> {

    Polygon::Container points;

public:
    using Nary_operation<Polygon_operation<Polygon_set>>::push_back;

    void push_back(const Point_2 &P) {
        points.push_back(P);
    }

    void evaluate() override;

    std::string describe() const override {
        return compose_tag("hull", operands, points);
    }
};

// Minkowski sum

class Polygon_minkowski_sum_operation:
    public Binary_operation<Polygon_operation<Polygon_set>> {

public:
    using Binary_operation<Polygon_operation<Polygon_set>>::Binary_operation;

    std::string describe() const override {
        return compose_tag("minkowski_sum", first, second);
    }

    void evaluate() override;
    bool try_fold();
};

#define DEFINE_SET_OPERATION(OP)                                        \
template<typename T>                                                    \
class Polygon_##OP##_operation:                                         \
    public Binary_operation<Polygon_operation<T>> {                     \
                                                                        \
public:                                                                 \
    using Binary_operation<Polygon_operation<T>>::Binary_operation;     \
                                                                        \
    std::string describe() const override {                             \
        return compose_tag(#OP, this->first, this->second);             \
    }                                                                   \
                                                                        \
    void evaluate() override {                                          \
        assert(!this->polygon);                                         \
                                                                        \
        this->polygon = std::make_shared<T>();                          \
        this->polygon->OP(*this->first->get_value(),                    \
                          *this->second->get_value());                  \
    }                                                                   \
                                                                        \
    bool try_fold();                                                    \
};

DEFINE_SET_OPERATION(join)
DEFINE_SET_OPERATION(difference)
DEFINE_SET_OPERATION(intersection)
DEFINE_SET_OPERATION(symmetric_difference)

#undef DEFINE_SET_OPERATION

template<typename T>
class Polygon_complement_operation:
    public Unary_operation<Polygon_operation<T>> {

public:
    using Unary_operation<Polygon_operation<T>>::Unary_operation;

    std::string describe() const override {
        return compose_tag("complement", this->operand);
    }

    void evaluate() override;
};

// Polygon offset

class Polygon_offset_operation:
    public Sequentially_foldable_operation<Polygon_operation<Polygon_set>> {
    FT offset;

public:
    Polygon_offset_operation(
        const std::shared_ptr<Polygon_operation<Polygon_set>> &p, const FT &delta):
        Sequentially_foldable_operation<Polygon_operation<Polygon_set>>(p),
        offset(delta) {}

    std::string describe() const override {
        return compose_tag("offset", operand, offset);
    }

    void evaluate() override;
    bool fold_operand(const Polygon_operation<Polygon_set> *p) override;
};

#endif
