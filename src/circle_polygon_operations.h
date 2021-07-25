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
