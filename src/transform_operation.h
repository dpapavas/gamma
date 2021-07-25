#ifndef TRANSFORM_OPERATION_H
#define TRANSFORM_OPERATION_H

#include "basic_operations.h"

template<typename T, typename A>
class Transform_operation: public Sequentially_foldable_operation<T> {
    template<template<typename> typename, template<typename> typename, typename>
    friend bool try_fold_transformation(Operation *x);

protected:
    A transformation;

public:
    Transform_operation(
        const std::shared_ptr<T> &p,
        const A &X):
        Sequentially_foldable_operation<T>(p), transformation(X) {}

    std::string describe() const override {
        return compose_tag("transform", this->operand, transformation);
    }

    bool fold_operand(const T *p) override {
        const Transform_operation<T, A> *t =
            dynamic_cast<const Transform_operation<T, A> *>(p);

        if (!t) {
            return false;
        }

        this->transformation = this->transformation * t->transformation;

        return true;
    }
};

#endif
