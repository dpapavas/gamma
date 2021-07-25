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
