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

#ifndef MESH_OPERATIONS_H
#define MESH_OPERATIONS_H

#include <CGAL/IO/Color.h>

#include "selection.h"

template<typename T>
class Color_selection_operation:
    public Unary_operation<Polyhedron_operation<Surface_mesh>> {
    const std::shared_ptr<T> selector;
    const CGAL::IO::Color color;

public:
    Color_selection_operation(
        const std::shared_ptr<Polyhedron_operation<Surface_mesh>> &p,
        const std::shared_ptr<T> &q,
        const FT r, const FT g, const FT b, const FT a):
        Unary_operation<Polyhedron_operation<Surface_mesh>>(p),
        selector(q),
        color(static_cast<unsigned char>(CGAL::to_double(r * 255)),
              static_cast<unsigned char>(CGAL::to_double(g * 255)),
              static_cast<unsigned char>(CGAL::to_double(b * 255)),
              static_cast<unsigned char>(CGAL::to_double(a * 255))) {}

    void evaluate() override;

    std::string describe() const override {
        return compose_tag(
            "color_selection", this->operand, selector,
            static_cast<int>(color.red()), static_cast<int>(color.blue()),
            static_cast<int>(color.green()), static_cast<int>(color.alpha()));
    }

    // We don't searialize color maps, so always need to re-evaluate.

    bool store() override {
        return false;
    };

    bool load() override {
        return false;
    };
};

template<typename T>
class Remesh_operation:
    public Unary_operation<Polyhedron_operation<T>> {
    const std::shared_ptr<Face_selector> face_selector;
    const std::shared_ptr<Edge_selector> edge_selector;
    const FT target;
    const int iterations;

public:
    Remesh_operation(
        const std::shared_ptr<Polyhedron_operation<T>> &p,
        const std::shared_ptr<Face_selector> &q,
        const std::shared_ptr<Edge_selector> &r,
        const FT &l, const int n):
        Unary_operation<Polyhedron_operation<T>>(p),
        face_selector(q), edge_selector(r),
        target(l), iterations(n) {}

    void evaluate() override;

    std::string describe() const override {
        return compose_tag(
            "remesh",
            this->operand, face_selector, edge_selector, target, iterations);
    }
};

template<typename T>
class Perturb_operation:
    public Unary_operation<Polyhedron_operation<T>> {
    const std::shared_ptr<Vertex_selector> selector;
    const FT magnitude;

public:
    Perturb_operation(
        const std::shared_ptr<Polyhedron_operation<T>> &p,
        const std::shared_ptr<Vertex_selector> &q,
        const FT &m):
        Unary_operation<Polyhedron_operation<T>>(p), selector(q),
        magnitude(m) {}

    void evaluate() override;

    std::string describe() const override {
        return compose_tag(
            "perturb", this->operand, selector, magnitude);
    }
};

template<typename T>
class Refine_operation:
    public Unary_operation<Polyhedron_operation<T>> {
    const std::shared_ptr<Face_selector> selector;
    const FT density;

public:
    Refine_operation(
        const std::shared_ptr<Polyhedron_operation<T>> &p,
        const std::shared_ptr<Face_selector> &q,
        const FT &rho):
        Unary_operation<Polyhedron_operation<T>>(p), selector(q), density(rho) {}

    void evaluate() override;

    std::string describe() const override {
        return compose_tag(
            "refine", this->operand, selector, density);
    }
};

template<typename T>
class Corefine_operation:
    public Binary_operation<Polyhedron_operation<T>> {

public:
    using Binary_operation<Polyhedron_operation<T>>::Binary_operation;

    std::string describe() const override {
        return compose_tag("corefine", this->first, this->second);
    }

    void evaluate() override;
};

template<typename T>
class Corefine_with_plane_operation:
    public Unary_operation<Polyhedron_operation<T>> {
    const Plane_3 plane;

public:
    Corefine_with_plane_operation(
        const std::shared_ptr<Polyhedron_operation<T>> &p,
        const Plane_3 &pi):
        Unary_operation<Polyhedron_operation<T>>(p), plane(pi) {}

    std::string describe() const override {
        return compose_tag("corefine", this->operand, plane);
    }

    void evaluate() override;
};

#endif
