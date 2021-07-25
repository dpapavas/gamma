#ifndef EVALUATION_H
#define EVALUATION_H

#include "options.h"

void begin_unit(const char *name);
void evaluate_unit();
std::shared_ptr<Operation> find_operation(const std::string &k);
void rehash_operation(const std::string &k);
void insert_operation(const std::shared_ptr<Operation> p);
bool erase_operation(const Operation *p);

template<typename T, typename U = Operation, typename F, typename... Args>
std::shared_ptr<U> add_operation(F &&first, Args &&... args)
{
    std::shared_ptr<T> p;

    // We either accept a ready-made shared pointer to the operation,
    // or the arguments needed to construct it.

    if constexpr (
        sizeof...(args) == 0
        && std::is_same_v<F, const std::shared_ptr<T> &>) {
        p = first;
    } else {
        p = std::make_shared<T>(
            std::forward<F>(first), std::forward<Args>(args)...);
    }

    // If an identical operation already exists, return the existing
    // operation.

    p->reset_tag();
    std::shared_ptr<Operation> q = find_operation(p->get_tag());

    if (q) {
        if (Flags::warn_duplicate) {
            p->message(Operation::WARNING, "operation % already instantiated");
            q->message(Operation::NOTE, "first instance of operation");
        }

        if constexpr (std::is_same_v<U, Operation>) {
            return q;
        } else {
            return std::dynamic_pointer_cast<U>(q);
        }
    } else {
        p->link();
        insert_operation(p);

        if constexpr (std::is_same_v<U, T>) {
            return p;
        } else {
            return std::static_pointer_cast<U>(p);
        }
    }
}

#endif
