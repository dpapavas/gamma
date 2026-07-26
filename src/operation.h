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

#ifndef OPERATION_H
#define OPERATION_H

#include <algorithm>
#include <functional>
#include <list>
#include <string>
#include <memory>
#include <unordered_set>
#include <unordered_map>

#include "assertions.h"
#include "options.h"
#include "compose_tag.h"

// Document: program

// # Operations

// An operation is a wrapper around a process that creates, modifies
// or consumes geometry.  It has a tag, which must be unique (and is
// typically a textual representation of the operation) and it can be
// evaluated to yield a result, typically some geometry.

// ## The Evaluation Graph

// Operations are created as a result of the evaluation of front end
// code. For instance, the `(tetrahedron 1 1 1)` Scheme form, or the
// equivalient `tetrahedron(1, 1, 1)` in Lua, produce a
// `Tetrahedron_operation`.  This particular operation can be
// evaulated straight away; we say it is a *source operation*, but but
// in most practical situations, we'll want to describe a more
// elaborate computation, where many operations will depend on the
// results of others.  Take for instance, the following front end
// code:

// ```
// (output "foo.stl" (difference (sphere 2) (sphere 1)))
// ```

// This would produce four operations. Two are of type
// `Sphere_operation` which, again, are source operations.  Another,
// would be a `Polyhedron_difference_operation`, which is a
// `Binary_operation` and takes the two spheres as arguments,
// producing a hollow sphere when evaluated.

// We say that the sphere operations are *predecessors* to the
// difference operation, meaninng that their evaluation must precede
// that of the difference operation, since you can't take the
// difference of geometry you don't yet have.  Equally, we say that
// the difference operation is a *successor* of both sphere
// operations.  Finally there would also be a `Write_STL_operation`,
// which is a sink operation.  It will take the geometry resulting
// from the evaluation of the difference (of which it therefore is a
// successor) and write it to a file on disk, producing no output, in
// the sense that no geometry is produced that can serve as input to
// another operation.
// foo

// In this way, operations resulting from the evaluation of front end
// code are organized into a graph, like the one drawn in
// ref:graph-example, with edges pointing from predecessors to
// successors.  This can then be postprocessed and evaluated.  Ref:
// Operation Evaluation.

// Figure:graph-example
// ```graph
// "sphere: radius=1" -> "difference"
// "sphere: radius=2" -> "difference"
// "difference" -> "write"
// ```
//   A simple evaluation graph.

// ## The Base Operation Class

class Operation {
public:
    // Operations objects may contain large structures, such as CGAL
    // geometry, so we don't want to copy or move them.  We create
    // them on the heap as shared pointers and only use these (or the
    // underlying raw pointers) to access them.

    // We therefore delete the corresponding constructors.

    Operation(const Operation &) = delete;
    Operation &operator=(const Operation &) = delete;

    Operation(Operation &&) = delete;
    Operation &operator=(Operation &&) = delete;

    // This is a simple hook to have a function executed for each
    // instantiated operation.  Mostly useful for language front ends.

    inline static void (*hook) (Operation &);

    Operation(): selected(false), loadable(false), rewritten(false), cost(0.0) {
        if (hook) {
            hook(*this);
        }
    }

    // *Annotations* describe aspects of the operation (the number
    // of facets, or vertices, where in the fronend code it was
    // created, etc.) and serve mainly diagnostic purposes.  They
    // should not have any functonal significance, i.e. the evaluation
    // of the operation should not depend on them.

    std::unordered_map<std::string, std::string> annotations;

    // The *tag* of the operation is a unique textual description of
    // the operation.  It allows "mapping" the operation (ref:
    // Operation Mapping) and can be digested into a small hash value,
    // the `tag_digest`, in order to form the `store_path`, which is
    // the file where this operation will be stored, if that is deemed
    // necessary.  Ref: Operation Caching.

    std::string tag, tag_digest, store_path;

    // These flags facilitate evaluation of the operation.

    //   selected := essentially colors the node during the various
    //   traversals of the graph that occur in the course of
    //   evaluation.

    //   loadable := marks the node as loadable from a stored cache
    //   file, if one exists and loading has been enabled.  Ref:
    //   Operation Caching.

    //   rewritten := marks the operatio as having been the target of
    //   a graph rewrite.  Ref: Graph Rewriting.

    bool selected, loadable, rewritten;

    // This is the cummulative time needed to evaluate this operation,
    // that is, the time needed to evaluate the operation itself and
    // all of its ancestors.

    float cost;

    // During evalutation of the operation, it may be useful to print
    // a message to the user, either with information that may be of
    // interest, or to issue a warning, or to announce that the
    // evaluation failed due to an error.

    // For more details, ref: Operation Evaluation Messages.

    enum Message_level {
        NOTE,
        WARNING,
        ERROR
    };

    void message(Message_level level, const std::string &message) const;

    // These pointer sets implement the evaluation graph (ref: The
    // Evaluation Graph).  The idea behind making them sets is to
    // ensure erase operastion are fast when unlinking nodes.  It is
    // likely that the overhead of using sets offsets any benefits for
    // the, likely small, size that is expected in the ususal case,
    // but then again, some calculations may involve operations with a
    // large number of successors or operands.  It probably won't make
    // much difference in practice anyway.

    std::unordered_set<Operation *> predecessors, successors;

    // The following operations allow linking or unlinking ourselves
    // wrt to a *successor* `op`.  These are the only operations that
    // should manipulate these sets and it should only happen when
    // doing graph rewrites, or when an operation with operands is
    // initially created.

    void unlink_from(Operation *op) {
        safely_assert(successors.erase(op) > 0);
        safely_assert(op->predecessors.erase(this) > 0);
    };

    void link_to(Operation *op) {
        this->successors.insert(op);
        op->predecessors.insert(this);
    }

    // Anchor: `Operation` destructor

    // The operation will be destroyed when there are no more shared
    // pointers referencing it.  This can happen either

    //   1. prior to evaluation, if at the time of closing the front
    //   end, it has already been collected as garbage,

    //   2. at the start of evaluation, if it is not an ancestor of a
    //   sunk operation (ref: Sunk Operations), or

    //   3. after evaluation, when the result has been output and we
    //   clean up.

    // At any of those points the operation must also be unlinked from
    // the graph, to maintain its consistency.  This is done in the
    // destructor below.

    ~Operation() {
        for (auto &x: predecessors) {
            safely_assert(x->successors.erase(this));
        }

        for (auto &x: successors) {
            safely_assert(x->predecessors.erase(this));
        }

        predecessors.clear();
        successors.clear();
    }

    // The following virtual member functions implement the specifics
    // of each operationd and, as such, are implemented in derived
    // classes.

    //   describe := composes the operations tag; ref: Tag Composition.

    //   reset := resets the shared pointers held by the operation to
    //   its operands, as soon as they're no longer needed.  This
    //   happens either when the operation has been evaluated, or when
    //   it is determined that the operation can be loaded, so that
    //   the whole predecessor subgraph of this operation is no longer
    //   required.

    virtual std::string describe() const = 0;
    virtual void reset() = 0;

    // The rest are used to evaluate the operation.  The calculation
    // itself can be done with `evaluate`, but we only do that if we
    // cannot load the operation from disk with `load`.  Once
    // evaluated, the result may we serialized and stored to disk with
    // `store`.

    // All this is taken care of in `dispatch`; ref: Operation
    // Dispatch.

    virtual bool dispatch();

private:
    virtual bool store() const {
        return false;
    }

    virtual bool load() {
        return false;
    }

    virtual void evaluate() = 0;
};

// This function template constructs an operation of type `T` with the
// given arguments.  This should be the only way by which operations
// are created in practice.

template<typename T, typename U = Operation, typename... Args>
std::shared_ptr<U> make_operation(Args &&... args)
{
    auto p = std::make_shared<T>(std::forward<Args>(args)...);

    if constexpr (std::is_same_v<U, Operation>) {
        return p;
    }

    return std::static_pointer_cast<U>(p);
}

// This template facilitates tag composition; ref: Tag Composition.

template<typename T>
struct compose_tag_helper<std::shared_ptr<T>,
                          std::enable_if_t<
                              std::is_base_of_v<Operation, T>>> {
    static void compose(std::ostringstream &s, const std::shared_ptr<T> &x) {
        if (x) {
            assert(!x->tag.empty());
            s << x->tag << ",";
        }
    }
};

// We throw this to turn a warning into an error when `-Werror` is in
// effect; ref: Operation Evaluation Messages.

class operation_warning_error: public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Document: none

#endif
