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

#ifndef POLYHEDRON_OPERATIONS_H
#define POLYHEDRON_OPERATIONS_H

#include "tolerances.h"
#include "polyhedron_types.h"
#include "transformation_types.h"
#include "basic_operations.h"
#include "transform_operation.h"

template<typename T>
class Polyhedron_operation: public Threadsafe_operation {
protected:
    std::shared_ptr<T> polyhedron;

public:
    Polyhedron_operation(): polyhedron(nullptr) {}
    Polyhedron_operation(const bool p):
        Threadsafe_operation(p), polyhedron(nullptr) {}

    bool dispatch() override;

    std::shared_ptr<T> get_value() const {
        assert(polyhedron);
        return polyhedron;
    };

    bool store() override;
    bool load() override;
};

template<typename T>
class Unsafe_polyhedron_operation: public Polyhedron_operation<T> {
public:
    Unsafe_polyhedron_operation(): Polyhedron_operation<T>(false) {}
};

template<typename T>
class Polyhedron_transform_operation:
    public Transform_operation<Polyhedron_operation<T>,
                               Aff_transformation_3> {

public:
    using Transform_operation<Polyhedron_operation<T>,
                              Aff_transformation_3>::Transform_operation;

    void evaluate() override;
};

template<typename T>
class Polyhedron_flush_operation:
    public Sequentially_foldable_operation<Polyhedron_operation<T>> {

    FT coefficients[3][2];

public:
    Polyhedron_flush_operation(
        const std::shared_ptr<Polyhedron_operation<T>> &p,
        const FT &lambda, const FT &mu, const FT &nu):
        Sequentially_foldable_operation<Polyhedron_operation<T>>(p),
        coefficients{
            {CGAL::min(lambda, FT(0)), CGAL::max(lambda, FT(0))},
            {CGAL::min(mu, FT(0)),CGAL::max(mu, FT(0))},
            {CGAL::min(nu, FT(0)),CGAL::max(nu, FT(0))}} {}

    std::string describe() const override {
        return compose_tag(
            "flush", this->operand,
            coefficients[0], coefficients[1], coefficients[2]);
    }

    void evaluate() override;
    bool fold_operand(const Polyhedron_operation<T> *p) override;
};

// Conversion operations

template<typename T, typename U>
class Polyhedron_convert_operation;

template<typename T>
class Polyhedron_convert_operation<Polyhedron, T>:
    public Unary_operation<Polyhedron_operation<T>,
                           Polyhedron_operation<Polyhedron>> {
public:
    using Unary_operation<Polyhedron_operation<T>,
                          Polyhedron_operation<Polyhedron>>::Unary_operation;

    void evaluate() override;

    std::string describe() const override {
        return compose_tag("polyhedron", this->operand);
    }
};

template<typename T>
class Polyhedron_convert_operation<Nef_polyhedron, T>:
    public Unary_operation<Polyhedron_operation<T>,
                           Polyhedron_operation<Nef_polyhedron>> {
public:
    using Unary_operation<Polyhedron_operation<T>,
                          Polyhedron_operation<Nef_polyhedron>>::Unary_operation;

    void evaluate() override;

    std::string describe() const override {
        return compose_tag("nef", this->operand);
    }
};

template<typename T>
class Polyhedron_convert_operation<Surface_mesh, T>:
    public Unary_operation<Polyhedron_operation<T>,
                           Polyhedron_operation<Surface_mesh>> {
public:
    using Unary_operation<Polyhedron_operation<T>,
                          Polyhedron_operation<Surface_mesh>>::Unary_operation;

    void evaluate() override;

    std::string describe() const override {
        return compose_tag("mesh", this->operand);
    }
};

// Set operations

#define DEFINE_SET_OPERATION(OP)                                        \
template<typename T>                                                    \
class Polyhedron_## OP ##_operation:                                    \
    public Binary_operation<Polyhedron_operation<T>> {                  \
                                                                        \
public:                                                                 \
    using Binary_operation<Polyhedron_operation<T>>::Binary_operation;  \
                                                                        \
    std::string describe() const override {                             \
        return compose_tag(#OP, this->first, this->second);             \
    }                                                                   \
                                                                        \
    void evaluate() override;                                           \
    bool try_fold();                                                    \
};

DEFINE_SET_OPERATION(join)
DEFINE_SET_OPERATION(difference)
DEFINE_SET_OPERATION(intersection)

#undef DEFINE_SET_OPERATION

template<typename T>
class Polyhedron_complement_operation:
    public Unary_operation<Polyhedron_operation<T>> {

public:
    using Unary_operation<Polyhedron_operation<T>>::Unary_operation;

    std::string describe() const override {
        return compose_tag("complement", this->operand);
    }

    void evaluate() override;
};

class Polyhedron_boundary_operation:
    public Unary_operation<Polyhedron_operation<Nef_polyhedron>> {

public:
    using Unary_operation<Polyhedron_operation<Nef_polyhedron>>::Unary_operation;

    std::string describe() const override {
        return compose_tag("boundary", operand);
    }

    void evaluate() override;
};

class Polyhedron_symmetric_difference_operation:
    public Binary_operation<Polyhedron_operation<Nef_polyhedron>> {

public:
    using Binary_operation<Polyhedron_operation<Nef_polyhedron>>::Binary_operation;

    std::string describe() const override {
        return compose_tag("symmetric_difference", this->first, this->second);
    }

    void evaluate() override;
};

template<typename T>
class Polyhedron_clip_operation:
    public Unary_operation<Polyhedron_operation<T>> {

    Plane_3 plane;

public:
    Polyhedron_clip_operation(
        const std::shared_ptr<Polyhedron_operation<T>> &p,
        const Plane_3 &Pi):
        Unary_operation<Polyhedron_operation<T>>(p), plane(Pi) {}

    void evaluate() override;

    std::string describe() const override {
        return compose_tag("clip", this->operand, plane);
    }
};

// Subdivision

template<typename T>
class Polyhedron_subdivision_operation:
    public Unary_operation<Polyhedron_operation<T>> {

protected:
    unsigned int depth;

public:
    Polyhedron_subdivision_operation(
        const std::shared_ptr<Polyhedron_operation<T>> &p,
        const unsigned int n):
        Unary_operation<Polyhedron_operation<T>>(p), depth(n) {}
};

template<typename T>
class Loop_subdivision_operation:
    public Polyhedron_subdivision_operation<T> {

public:
    using Polyhedron_subdivision_operation<T>::Polyhedron_subdivision_operation;

    void evaluate() override;

    std::string describe() const override {
        return compose_tag("loop", this->operand, this->depth);
    }
};

template<typename T>
class Catmull_clark_subdivision_operation:
    public Polyhedron_subdivision_operation<T> {

public:
    using Polyhedron_subdivision_operation<T>::Polyhedron_subdivision_operation;

    void evaluate() override;

    std::string describe() const override {
        return compose_tag("catmull_clark", this->operand, this->depth);
    }
};

template<typename T>
class Doo_sabin_subdivision_operation:
    public Polyhedron_subdivision_operation<T> {

public:
    using Polyhedron_subdivision_operation<T>::Polyhedron_subdivision_operation;

    void evaluate() override;

    std::string describe() const override {
        return compose_tag("doo_sabin", this->operand, this->depth);
    }
};

template<typename T>
class Sqrt_3_subdivision_operation:
    public Polyhedron_subdivision_operation<T> {

public:
    using Polyhedron_subdivision_operation<T>::Polyhedron_subdivision_operation;

    void evaluate() override;

    std::string describe() const override {
        return compose_tag("sqrt_3", this->operand, this->depth);
    }
};

// Convex hull

class Polyhedron_hull_operation:
    public Nary_operation<Operation, Polyhedron_operation<Polyhedron>> {

    std::vector<Polyhedron_operation<Polyhedron> *> polyhedra;
    std::vector<Polyhedron_operation<Nef_polyhedron> *> nef_polyhedra;
    std::vector<Polyhedron_operation<Surface_mesh> *> meshes;
    std::vector<Point_3> points;

public:
    void evaluate() override;

    std::string describe() const override {
        return compose_tag("hull", operands, points);
    }

    void push_back(
        const std::shared_ptr<Polyhedron_operation<Polyhedron>> &p) {
        polyhedra.push_back(p.get());

        Nary_operation<Operation, Polyhedron_operation<Polyhedron>>::push_back(
            std::static_pointer_cast<Operation>(p));
    }

    void push_back(
        const std::shared_ptr<Polyhedron_operation<Nef_polyhedron>> &p) {
        nef_polyhedra.push_back(p.get());

        Nary_operation<Operation, Polyhedron_operation<Polyhedron>>::push_back(
            std::static_pointer_cast<Operation>(p));
    }

    void push_back(
        const std::shared_ptr<Polyhedron_operation<Surface_mesh>> &p) {
        meshes.push_back(p.get());

        Nary_operation<Operation, Polyhedron_operation<Polyhedron>>::push_back(
            std::static_pointer_cast<Operation>(p));
    }

    void push_back(const Point_3 &P) {
        points.push_back(P);
    }
};

// Minkowski sum

class Polyhedron_minkowski_sum_operation:
    public Binary_operation<Polyhedron_operation<Nef_polyhedron>> {

public:
    using Binary_operation<Polyhedron_operation<Nef_polyhedron>>::Binary_operation;

    std::string describe() const override {
        return compose_tag("minkowski_sum", first, second);
    }

    void evaluate() override;
};

// Primitive operations

class Tetrahedron_operation:
    public Source_operation<Polyhedron_operation<Polyhedron>> {

    FT a, b, c;

public:
    Tetrahedron_operation(const FT &a_, const FT &b_, const FT &c_):
        a(a_), b(b_), c(c_) {}

    void evaluate() override;

    std::string describe() const override {
        return compose_tag("tetrahedron", a, b, c);
    }
};

class Square_pyramid_operation:
    public Source_operation<Polyhedron_operation<Polyhedron>> {

    FT a, b, c;

public:
    Square_pyramid_operation(const FT &a_, const FT &b_, const FT &c_):
        a(a_), b(b_), c(c_) {}

    void evaluate() override;

    std::string describe() const override {
        return compose_tag("square_pyramid", a, b, c);
    }
};

class Octahedron_operation:
    public Source_operation<Polyhedron_operation<Polyhedron>> {

    FT a, b, c, d;

public:
    Octahedron_operation(const FT &a_, const FT &b_, const FT &c_):
        a(a_), b(b_), c(c_), d(c_) {}

    Octahedron_operation(
        const FT &a_, const FT &b_, const FT &c_, const FT &d_):
        a(a_), b(b_), c(c_), d(d_) {}

    void evaluate() override;

    std::string describe() const override {
        return compose_tag("octahedron", a, b, c, d);
    }
};

class Cuboid_operation:
    public Source_operation<Polyhedron_operation<Polyhedron>> {

    FT a, b, c;

public:
    Cuboid_operation(const FT &a_, const FT &b_, const FT &c_):
        a(a_), b(b_), c(c_) {}

    void evaluate() override;

    std::string describe() const override {
        return compose_tag("cuboid", a, b, c);
    }
};

class Icosahedron_operation:
    public Source_operation<Polyhedron_operation<Polyhedron>>
{
    FT radius, tolerance;

public:
    Icosahedron_operation(const FT &r):
        radius(r), tolerance(Tolerances::projection) {}

    void evaluate() override;

    std::string describe() const override {
        return compose_tag("icosahedron", radius, tolerance);
    }
};

class Sphere_operation:
    public Source_operation<Polyhedron_operation<Polyhedron>> {

    FT radius, tolerances[2];

public:
    Sphere_operation(const FT &r):
        radius(r), tolerances{Tolerances::curve, Tolerances::projection} {}

    void evaluate() override;

    std::string describe() const override {
        return compose_tag("sphere", radius, tolerances);
    }
};

// A regular right parimid.

class Regular_pyramid_operation:
    public Source_operation<Polyhedron_operation<Polyhedron>> {
    const int sides;
    const FT radius, height, tolerance;

public:
    Regular_pyramid_operation(const int n, const FT &r, const FT &h):
        sides(n), radius(r), height(h), tolerance(Tolerances::projection) {}

    std::string describe() const override {
        return compose_tag(
            "regular_pyramid", sides, radius, height, tolerance);
    }

    void evaluate() override;
};

// A regular right bipyramid, potentially assymetric or inverted.

class Regular_bipyramid_operation:
    public Source_operation<Polyhedron_operation<Polyhedron>> {
    const int sides;
    const FT radius, heights[2], tolerance;

public:
    Regular_bipyramid_operation(const int n, const FT &r, const FT &h):
        sides(n), radius(r), heights{h, h}, tolerance(Tolerances::projection) {}

    Regular_bipyramid_operation(
        const int n, const FT &r, const FT &h_1, const FT &h_2):
        sides(n), radius(r), heights{h_1, h_2},
        tolerance(Tolerances::projection) {}

    std::string describe() const override {
        return compose_tag(
            "regular_bipyramid", sides, radius, heights, tolerance);
    }

    void evaluate() override;
};

#endif
