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

#ifndef CHAMFERING_OPERATIONS_H
#define CHAMFERING_OPERATIONS_H

#include "selection.h"
#include "polyhedron_operations.h"

enum struct Chamfering_operation_mode {
    INNER = 0,
    OUTER
};

template<typename T, bool Fillet=false, bool Make_only=false>
class Chamfering_operation:
    public Unary_operation<Polyhedron_operation<T>> {
public:
    const std::shared_ptr<Edge_selector> edge_selector;
    const FT parameters[2];
    const Chamfering_operation_mode mode;

    Chamfering_operation(
        const std::shared_ptr<Polyhedron_operation<T>> &p,
        const std::shared_ptr<Edge_selector> &q,
        const FT &l, const FT &m, enum Chamfering_operation_mode d):
        Unary_operation<Polyhedron_operation<T>>(p),
        edge_selector(q), parameters{l, m}, mode(d) {}

    Chamfering_operation(
        const std::shared_ptr<Polyhedron_operation<T>> &p,
        const std::shared_ptr<Edge_selector> &q,
        const FT &r, enum Chamfering_operation_mode d):
        Unary_operation<Polyhedron_operation<T>>(p),
        edge_selector(q), parameters{r, Tolerances::curve}, mode(d) {}

    void evaluate() override;

    std::string describe() const override {
        const char *s[] = {
            "chamfer_inner", "chamfer_outer", "chamfer",
            "make_inner_chamfer", "make_outer_chamfer", "make_chamfer",
            "fillet_inner", "fillet_outer", "fillet",
            "make_inner_fillet", "make_outer_fillet", "make_fillet"};

        return compose_tag(
            s[6 * Fillet + 3 * Make_only + static_cast<int>(mode)],
            this->operand, edge_selector, parameters);
    }
};

template<typename T>
using Make_chamfering_operation = Chamfering_operation<T, false, true>;

template<typename T>
using Fillet_operation = Chamfering_operation<T, true, false>;

template<typename T>
using Make_fillet_operation = Chamfering_operation<T, true, true>;

#endif
