#ifndef DEFORM_OPERATIONS_H
#define DEFORM_OPERATIONS_H

#include "selection.h"

template<typename T>
class Fair_operation:
    public Unary_operation<Polyhedron_operation<T>> {
    const std::shared_ptr<Vertex_selector> selector;
    const int continuity;

public:
    Fair_operation(
        const std::shared_ptr<Polyhedron_operation<T>> &p,
        const std::shared_ptr<Vertex_selector> &q,
        const int n):
        Unary_operation<Polyhedron_operation<T>>(p), selector(q),
        continuity(n) {}

    void evaluate() override;

    std::string describe() const override {
        return compose_tag("fair", this->operand, selector, continuity);
    }
};

template<typename T>
class Smooth_shape_operation:
    public Unary_operation<Polyhedron_operation<T>> {
    const std::shared_ptr<Face_selector> face_selector;
    const std::shared_ptr<Vertex_selector> vertex_selector;
    const FT time;
    const int iterations;

public:
    Smooth_shape_operation(
        const std::shared_ptr<Polyhedron_operation<T>> &p,
        const std::shared_ptr<Face_selector> &q,
        const std::shared_ptr<Vertex_selector> &r,
        const FT &t, const int n):
        Unary_operation<Polyhedron_operation<T>>(p),
        face_selector(q), vertex_selector(r),
        time(t), iterations(n) {}

    void evaluate() override;

    std::string describe() const override {
        return compose_tag(
            "smooth_shape",
            this->operand, face_selector, vertex_selector, time, iterations);
    }
};

template<typename T>
class Deform_operation:
    public Unary_operation<Polyhedron_operation<T>> {
    const std::shared_ptr<Vertex_selector> selector;
    std::vector<std::pair<std::shared_ptr<Vertex_selector>,
                          Aff_transformation_3>> controls;
    const FT tolerance;
    const unsigned int iterations;

public:
    Deform_operation(
        const std::shared_ptr<Polyhedron_operation<T>> &p,
        std::vector<std::pair<std::shared_ptr<Vertex_selector>,
                              Aff_transformation_3>> &&v,
        const FT &tau, const unsigned int n):
        Unary_operation<Polyhedron_operation<T>>(p), controls(std::move(v)),
        tolerance(tau), iterations(n) {}

    Deform_operation(
        const std::shared_ptr<Polyhedron_operation<T>> &p,
        const std::shared_ptr<Vertex_selector> &q,
        std::vector<std::pair<std::shared_ptr<Vertex_selector>,
                              Aff_transformation_3>> &&v,
        const FT &tau, const unsigned int n):
        Unary_operation<Polyhedron_operation<T>>(p), selector(q),
        controls(std::move(v)), tolerance(tau), iterations(n) {}

    void evaluate() override;

    std::string describe() const override {
        return compose_tag(
            "deform",
            this->operand, selector, controls, tolerance, iterations);
    }
};

template<typename T>
class Deflate_operation:
    public Unary_operation<Polyhedron_operation<T>> {
    const std::shared_ptr<Vertex_selector> selector;
    const int steps;
    const FT parameters[2];

public:
    Deflate_operation(
        const std::shared_ptr<Polyhedron_operation<T>> &p,
        const int n, const FT w_H, const FT w_M):
        Unary_operation<Polyhedron_operation<T>>(p),
        steps(n), parameters {w_H, w_M} {}

    Deflate_operation(
        const std::shared_ptr<Polyhedron_operation<T>> &p,
        const std::shared_ptr<Vertex_selector> &q,
        const int n, const FT w_H, const FT w_M):
        Unary_operation<Polyhedron_operation<T>>(p), selector(q),
        steps(n), parameters {w_H, w_M} {}

    std::string describe() const override {
        return compose_tag(
            "deflate", this->operand, selector, steps, parameters);
    }

    void evaluate() override;
};

#endif
