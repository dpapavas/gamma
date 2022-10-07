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

#ifndef EXTRUSION_OPERATION_H
#define EXTRUSION_OPERATION_H

#include <vector>

#include "polyhedron_operations.h"
#include "polygon_operations.h"

class Extrusion_operation:
    public Unary_operation<Polygon_operation<Polygon_set>,
                           Unsafe_polyhedron_operation<Polyhedron>> {

    const std::vector<Aff_transformation_3> transformations;

public:
    Extrusion_operation(
        const std::shared_ptr<Polygon_operation<Polygon_set>> p,
        std::vector<Aff_transformation_3> &&v):
        Unary_operation(p), transformations(std::move(v)) {}

    std::string describe() const override {
        return compose_tag("extrusion", operand, transformations);
    }

    void evaluate() override;
};

#endif
