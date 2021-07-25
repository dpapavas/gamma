#ifndef OPERATION_H
#define OPERATION_H

#include <algorithm>
#include <string>
#include <unordered_set>
#include <unordered_map>

#include "assertions.h"
#include "compose_tag.h"

class operation_warning_error: public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// An operation is a wrapper around a process that creates, modifies
// or consumes geometry.  It has a tag, which must be unique (and is
// typically a textual representation of the operation) and it can be
// evaluated to yield a result, typically some geometry.

class Operation {
protected:
    std::string tag, tag_digest, store_path;

public:
    static std::function<void(Operation &)> hook;
    std::unordered_set<Operation *> predecessors, successors;
    std::unordered_map<std::string, std::string> annotations;
    bool selected, loadable;
    float cost;

    enum Message_level {
        NOTE,
        WARNING,
        ERROR
    };

    void message(Message_level level, std::string message);

public:
    Operation(): selected(false), loadable(false), cost(0.0) {
        if (hook) {
            hook(*this);
        }
    }

    Operation(const Operation &) = delete;
    Operation &operator=(const Operation &) = delete;

    Operation(Operation &&) = delete;
    Operation &operator=(Operation &&) = delete;

    virtual std::string describe() const = 0;
    virtual void link() = 0;

    void unlink_from(Operation *p) {
        if (predecessors.erase(p) > 0) {
            safely_assert(p->successors.erase(this) > 0);
        } else if (successors.erase(p) > 0) {
            safely_assert(p->predecessors.erase(this) > 0);
        } else {
            assert_not_reached();
        }
    };

    virtual void evaluate() = 0;
    virtual bool dispatch();

    void select();

    virtual bool store() {
        return false;
    }

    virtual bool load() {
        return false;
    }

    const std::string &reset_tag() {
        return (tag = describe());
    }

    const std::string &get_tag() const{
        assert(!tag.empty());
        return tag;
    }

    std::string digest();
};

// Tag composition

template<typename T>
struct compose_tag_helper<std::shared_ptr<T>,
                          std::enable_if_t<
                              std::is_base_of_v<Operation, T>>> {
    static void compose(std::ostringstream &s, const std::shared_ptr<T> &x) {
        if (x) {
            s << std::static_pointer_cast<Operation>(x)->get_tag() << ",";
        }
    }
};

#endif
