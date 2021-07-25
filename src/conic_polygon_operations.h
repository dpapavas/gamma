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
