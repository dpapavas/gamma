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

#include <typeinfo>

#include "kernel.h"
#include "polygon_operations.h"
#include "circle_polygon_types.h"
#include "conic_polygon_types.h"
#include "polyhedron_operations.h"
#include "transform_operation.h"
#include "macros.h"
#include "rewrites.h"
#include "evaluation.h"

// Document: program

// # Graph Rewriting

// Sometimes we can restructure a set of operations in such a way,
// that the result is the same but the calculation can be carried out
// more efficiently.

// We define functions, called *rewriters* for each such case below.
// These oeprate on specific operation types, check the local graph
// topology to determine whether a rewrite is appropriate and carry it
// out if it is.  The result is a locally modified evaulation graph.

// We annotate rewritten operations with the number of rewrite
// operations carried out on it, using the utility function below.
// More importantly, it also sets the operation's `rewritten` flag,
// which will prompt a retag of the rewritten operation (ref:
// Rewriting the Graph)

static void update_operation(Operation *op)
{
    auto it = op->annotations.find("rewrites");

    if (it == op->annotations.end()) {
        op->annotations.insert({"rewrites", "1"});
    } else {
        const int n = std::stoi(it->second);
        it->second = std::to_string(n + 1);
    }

    op->rewritten = true;
}

// ## Boolean Operation Chain Folding

// Union and intersection operations are associative, which allows us
// to reorder them.  Consider a union of 4 polyhedra $p + q + r + s$.
// We can evaluate them as $((p + q) + r) + s$, but we could also
// evaluate $(p + q) + (r + s)$ instead.  To get an idea of the
// difference, inspect the operation graphs created by the
// ref: `polyhedron_union` test.

// The latter order is usually to be preferred.  For instance
// evaluating the union of 1000 unit cuboids (ref:
// `large_polyhedron_union` test), to form a cuboid of size 1000x1x1
// takes an order of maginute less with the rewritten order.

// ### Detecting Chains

// First we need to determine whether we have a foldable chain.  We
// can do this by testing whether a given opration (`op` below) forms
// a chain *link* with its successor (it must have only one; `op_1`
// below).  If it does, applying the same test iteratively to its
// successors will trace the whole chain.

// The successor `op_1` may have `op` as its first operand, forming
// part of a left-associative chain $(((p + q) + r) + ...)$, or as its
// second operand, forming part of a right-associative chain $(... +
// (r + (p + q)))$.  The whole chain can be either purely left or
// right-associative, or it may be a mixture of both.

// For associative operations, we generally do not care, otherwise we
// may want to restrict the kind of chains we apply rewrites to.

// The `walk_chain` function below performs this test.  It also
// advances the given pointer to the successor and returns the other
// node (marked `q` below).

enum Associativity {LEFT = 1, RIGHT, BOTH};

template<enum Associativity A, typename T>
static decltype(std::declval<T>().first) walk_chain(T *&op)
{
    // In order for a chain to be foldable, we require its links to have a
    // single successor, which should be a boolean operation of the same
    // kind, forming the next link.

    // In other words, the linkage of `op` should be like the
    // following.

    // ```graph
    // {op_1 -> op -> p}
    // op -> q
    // op_1 -> r
    // ```

    if (op->successors.size() != 1) {
        return nullptr;
    }

    auto *op_1 = dynamic_cast<T *>(*(op->successors.begin()));

    if (!op_1) {
        return nullptr;
    }

    // One predecessor of `op_1` will be `op`.  We require the other
    // to not be a boolean operation of the same kind to avoid
    // complications with cross-linked chains.  It also enables
    // iteratively folding the same chain.

    if constexpr (A & LEFT) {
        if (op_1->first.get() == op) {
            if (std::dynamic_pointer_cast<T>(op_1->second)) {
                return nullptr;
            }

            op = op_1;
            return op_1->second;
        }
    }

    if constexpr (A & RIGHT) {
        if (op_1->second.get() == op) {
            if (std::dynamic_pointer_cast<T>(op_1->first)) {
                return nullptr;
            }

            op = op_1;
            return op_1->first;
        }
    }

    return nullptr;
}

// ### Associative operations

// These include operations such as boolean union or intersection,
// which are associative.

template<typename T>
static bool try_fold_associative_operation(Operation *op)
{
    assert(dynamic_cast<T *>(op));

    // We want to start at the beginning of the chain if one exists.
    // If we're preceded by an operation that forms a chain with us,
    // then that is not the case, so we refuse to fold.

    for (auto it = op->predecessors.begin();
         it != op->predecessors.end();
         it++) {
        if (auto p = dynamic_cast<T *>(*it);
            p && walk_chain<BOTH>(p)) {
            return false;
        }
    }

    // Now we are at the end of a chain, if one exists, meaning that
    // `op`'s predecessors do not continue it.  We still need to
    // follow `op`'s successors to see if they do form a chain and
    // how far it goes.

    // This is a simple matter of applying the same test iteratively:

    T *op_4 = static_cast<T *>(op);

    int n = 1;
    decltype(op_4->first) u;
    for (auto x = walk_chain<BOTH>(op_4);
         x;
         n++, u = x, x = walk_chain<BOTH>(op_4));

    // Technically, a chain of two operations (i.e. three terms -- $a
    // + b + c$ ^[Since we're dealing with associative operations, the
    // distribution of parentheses is unimportant, so we leave them
    // out.  Similarly, we do not label graph edges.  Any edge could
    // correspond to any of the operation's two operands equally
    // well.]) is a chain, but we can't fold it.

    if (n < 3) {
        return false;
    }

    // The smallest chain we *can* fold is $p + q + r + s$.

    // ```graph
    // {op_2 -> op_1 -> op -> p}
    // op -> q
    // op_1 -> r
    // op_2 -> s
    // ```

    // We can rewrite it to $(p + q) + (r + s)$, noting that it only
    // requires transposing a couple of edges between `op_1` and
    // `op`:

    //   1. `op_1` -> `r` becomes `op_1` -> `op_2`
    //   2. `op_2` -> `op_1` becomes `op_2` -> `r`

    // The result is a split of the initial chain at the midpoint,
    // with one of the boolean operations now operating on the two
    // smaller chains.

    // ```graph
    // {op_2 -> r; op -> p}
    // op -> q
    // op_2 -> s
    // op_1 -> op_2; op_1 -> op
    // ```

    // One difficulty is that we need a `shared_ptr` to `op_2`, in
    // order to create the new edge to it, which we don't have.  We
    // can overcome this in various ways, but it's simpler to use the
    // following rewrite instead:

    // ```graph
    // {op_1 -> s; op -> p}
    // op -> q
    // op_2 -> op_1; op_2 -> op
    // op_1 -> r
    // ```

    // This rewrite calculates $(p + q) + (s + r)$, shifting the first
    // term of the first sub-chain to the end, but since the
    // operations are associative, this is of no consequence.

    // In the general case, the situation will look as follows.

    // ```graph
    // {op_4 -> op_3 -> ... -> op_2 -> op_1 -> ... -> op -> q}
    // op -> p
    // op_1 -> r
    // op_2 -> s
    // op_3 -> t
    // op_4 -> u
    // ```

    // The rewritten graph should then become:

    // ```graph
    // {op_3 -> ... -> op_2 -> u}
    // {op_1 -> ... -> op -> q}
    // op -> p
    // op_1 -> r
    // op_2 -> s
    // op_3 -> t
    // op_4 -> op_3; op_4 -> op_1
    // ```

    // Again, we only need to transpose a couple of edges:

    //   1. `op_4` -> `u` becomes `op_4` -> `op_1`
    //   2. `op_2` -> `op_1` becomes `op_2` -> `u`

    // Seen another way, this just swaps operands `p` and `op_1`
    // between `op_2` and `op_4`.

    // But first, we need to retrace the chain and find `op_2` and
    // `op_1` (having already found `op_4` and `u` above).

    T *op_2 = static_cast<T *>(op), *op_1;
    for (int i = 0; i < n / 2; i++) {
        op_1 = op_2;
        op_2 = static_cast<T *>(*op_2->successors.begin());
    }

    // Now it's just a matter of finding the correct operand pointers
    // and swapping them.

    assert(op_2->first.get() == op_1 || op_2->second.get() == op_1);
    assert(op_4->first == u || op_4->second == u);

    std::swap(
        op_2->first.get() == op_1 ? op_2->first : op_2->second,
        op_4->first == u ? op_4->first : op_4->second);

    // Finally we also need to update the graph linkage and annotate
    // the modified nodes.

    u->unlink_from(op_4);
    u->link_to(op_2);

    op_1->unlink_from(op_2);
    op_1->link_to(op_4);

    update_operation(op_2);
    update_operation(op_4);

    return true;
}

// ### Difference Operations

// We can't fold subtraction chains, since the operation is not
// associative.  Instead, we transform $p - q - r - ...$ to $p - (q +
// r + ...)$.

// This transformation alone seems to result in a significant speed-up
// for corefinement-based operations, but we can then fold the
// resulting chain of unions as well.

template<typename T>
static bool try_fold_difference_operation(Operation *op)
{
    assert(dynamic_cast<T *>(op));

    // We start by detecting whether we're at the start of a chain, as
    // we did for associative folding.  This time though we require
    // the chain to be left-associative, i.e. $(((p - q) - r) - ...)$,
    // which is what we usually mean with $(p - q - r - ...)$.

    for (auto it = op->predecessors.begin();
         it != op->predecessors.end();
         it++) {
        if (auto p = dynamic_cast<T *>(*it);
            p && walk_chain<LEFT>(p)) {
            return false;
        }
    }

    // In the general case, the chain of difference operations $p - q
    // - ... - r - s$ will look as below.  Here the order of
    // operations is important and we label the edges by the operand
    // number.

    // ```graph
    // edge: 1
    // {op_2 -> ... -> op_1 -> op -> p}
    // edge: 2
    // op -> q
    // op_1 -> r
    // op_2 -> s
    // ```

    // We want to rewrite it and turn `op_1`, ..., `op_2` into union
    // operations.  The last difference operation (`op_2`), was the
    // start of the chain, so any nodes using the result of the
    // difference chain would have pointed to it.  We're therefore
    // going to reuse it as the starting difference in the rewritten
    // chain.  This will then become:

    // ```graph
    // edge: 1
    // {op_2; "op_1+" -> ... -> "op+" -> q}
    // op_2 -> p
    // edge: 2
    // op_2 -> "op_1+"
    // "op+" -> r
    // "op_1+" -> s
    // ```

    T *op_ = static_cast<T *>(op), *op_1 = op_;

    // The smallest difference chain we can rewrite, is of length 2
    // (i. e. $p - q - r$ to $p - (q + r)$).  If we can't walk the
    // chain at least once, we bail out.

    if (!walk_chain<LEFT>(op_1)) {
        return false;
    }

    using P = typename T::value_type;
    using U = std::conditional_t<
        std::is_base_of_v<Polyhedron_operation<P>, T>,
        Polyhedron_join_operation<P>,
        Polygon_join_operation<P>>;

    // We start by making a note of the operands of the initial
    // operation `op`.  One of these, `p` will be used by the last
    // remaining difference at the end of the rewrite.  The other `q`,
    // must be linked to the succeeding operation (`op_1`, or rather
    // the new union operation we're going to create in its place.

    auto p = op_->first;
    auto q = std::static_pointer_cast<U>(op_->second);

    // Now we need to create the union chain.  At each iteration we
    // reuse `q` as the pointer to the previous iteration's operation;
    // `op_1` is the difference node to be replaced.

    do {
        // Unlink the difference operation node.  We only care about
        // destroying the successor links in its operands; the
        // difference node itself is going to be destroyed anyway.

        op_1->first->unlink_from(op_1);
        op_1->second->unlink_from(op_1);

        // Create a new union operation node and link it to the node
        // from the previous iteration (initially `q`, then `op_1`,
        // ...) and the original difference operation's other operand
        // (`r`, ...).

        q = make_operation<U, U>(q, op_1->second);

        q->first->link_to(q.get());
        q->second->link_to(q.get());

        assert(q->predecessors.size() == 1 + std::size_t(q->first != q->second));
        assert(q->successors.size() == 0);

        insert_operation(q);
        update_operation(q.get());
    } while (walk_chain<LEFT>(op_1));

    // We reuse the last replaced difference.  This is `op_1` below,
    // but corresponds to `op_2` in the graph.  Similarly `q`
    // corresponds to `op_1+`.

    op_1->first = p;
    op_1->second = q;
    p->link_to(op_1);
    q->link_to(op_1);

    assert(op_1->predecessors.size() == 2);
    update_operation(op_1);

    return true;
}

// ## Unary Operation Chain Folding

// Consider transformation operations.  A long string of
// transformation operations, finally applied to a polyhedron, say
// $p$, that is $T_1 * T_2 * ... * T_n * p$ can be replaced by a
// single transformation of the polyhedron, i.e. $T * p$, where $T$ is
// the product of all $T_n$ operations.

// The same can be done for other operations, such as flushes and
// offsets, albeit with a differently calculated "product".

// To implement the rewrite, we only try to find chains of length 3,
// i.e. $T_1 * T_2 * p$ and fold them to $T_{12} * p$.  If a chain is
// longer, say, $T_1 * T_2 * T_3 * p$, we first fold it to $T_1 *
// T_{23} * p$, which is again foldable and will be reduced to
// $T_{123} * p$.  Longer chains proceed similarly.

// ```graph
// { ... -> op -> op_1 -> ... -> p}
// ```

// This must be folded to the following, where $op_2 = op * op_1$.

// ```graph
// { ... -> op_2 -> ... -> p}
// ```

// We generally use the same operation `op`, but update it suitably to
// produce the composite effect.

// ### Calculating the Composite Operation

// The folded operation (`op_2`) must produce the same result as the
// combination of the original operations.  The details, naturally,
// differ for each kind of operation.

// For transformation operation we calculate the product, as
// implemented by CGAL.

template<typename T, typename A>
static void fold_operand(
    Transform_operation<T, A> *op, const Transform_operation<T, A> *op_1)
{
    op->transformation = op->transformation * op_1->transformation;
}

// For flush operations, we need to combine the coefficients of the
// original pair of flushes.

template<typename T,
         typename U = std::enable_if_t<
             std::is_same_v<Polyhedron_flush_operation<Polyhedron>, T>
             || std::is_same_v<Polyhedron_flush_operation<Nef_polyhedron>, T>
             || std::is_same_v<Polyhedron_flush_operation<Surface_mesh>, T>
             || std::is_same_v<Polygon_flush_operation, T>>>
static void fold_operand(T *op, const T *op_1)
{

    const FT (*b)[2] = op->coefficients, (*a)[2] = op_1->coefficients;

    // This computation can be done in place, since only one of
    // a[i][0], a[i][1] is non-zero at any given time.

    for (std::size_t i = 0;
         i < sizeof(op->coefficients) / sizeof(op->coefficients[0]);
         i++) {
        op->coefficients[i][0] = (a[i][0] * (1 - b[i][1])
                                  + b[i][0] * (1 + a[i][0]));
        op->coefficients[i][1] = (a[i][1] * (1 + b[i][0])
                                  + b[i][1] * (1 - a[i][1]));
    }
}

// Offsetting a polygon by $a$ and then by $b$ is the same as
// offsetting it by $a + b$.

static void fold_operand(
    Polygon_offset_operation *op, const Polygon_offset_operation *op_1)
{
    op->offset += op_1->offset;
}

// The rewrite itself, is quite straightforward.

template<typename T>
static bool try_fold_unary_operation(Operation *op)
{
    // First we must determine whether we can fold.  We can if our
    // single operand has a single successor (ourselves) and if it
    // also is an operatio of the same kind.

    assert(op->predecessors.size() == 1);
    Operation *x = *op->predecessors.begin();

    if (x->successors.size() > 1) {
        return false;
    }

    const T *op_1 = dynamic_cast<T *>(x);

    if (!op_1) {
        return false;
    }

    assert(op_1->successors.size() == 1);
    assert(op_1->predecessors.size() == 1);

    // Now we fold `op_1` into `op` and update the linkage
    // accordingly.

    T *op_2 = static_cast<T *>(op);

    fold_operand(op_2, op_1);

    op_2->operand = op_1->operand;
    op_2->operand->link_to(op_2);

    assert(op_2->predecessors.size() == 1);

    update_operation(op_2);

    return true;
}

// ## Applying rewrites

// Finally, we need a way to apply rewrites to an operation, if such
// rewrites are possible.  We use a map keyed by the operation's type,
// where we register all enabled rewrites.

Operation_rewriter::Operation_rewriter()
{
#define ADD_POLYHEDRON_OPERATION_REWRITES(OP, F, ...)                   \
    insert({                                                            \
            {typeid(OP<Polyhedron>), F<OP<Polyhedron>>},                \
            {typeid(OP<Nef_polyhedron>), F<OP<Nef_polyhedron>>},        \
            {typeid(OP<Surface_mesh>), F<OP<Surface_mesh>>}})

#define ADD_POLYGON_OPERATION_REWRITES(OP, F, ...)                      \
    insert({                                                            \
            {typeid(OP<Polygon_set>), F<OP<Polygon_set>>},              \
            {typeid(OP<Circle_polygon_set>), F<OP<Circle_polygon_set>>}, \
            {typeid(OP<Conic_polygon_set>), F<OP<Conic_polygon_set>>}})

    if (Flags::fold_booleans) {
        // Register polyhedron boolean operantion folds.

        ADD_POLYHEDRON_OPERATION_REWRITES(
            Polyhedron_join_operation, try_fold_associative_operation);

        ADD_POLYHEDRON_OPERATION_REWRITES(
            Polyhedron_intersection_operation, try_fold_associative_operation);

        ADD_POLYHEDRON_OPERATION_REWRITES(
            Polyhedron_difference_operation, try_fold_difference_operation);

        // Register polygon boolean operantion folds.

        ADD_POLYGON_OPERATION_REWRITES(
            Polygon_join_operation, try_fold_associative_operation);

        ADD_POLYGON_OPERATION_REWRITES(
            Polygon_intersection_operation, try_fold_associative_operation);

        ADD_POLYGON_OPERATION_REWRITES(
            Polygon_difference_operation, try_fold_difference_operation);
    }

    if (Flags::fold_transformations) {
        // Register transformation operantion folds.

        ADD_POLYHEDRON_OPERATION_REWRITES(
            Polyhedron_transform_operation, try_fold_unary_operation);

        ADD_POLYGON_OPERATION_REWRITES(
            Polygon_transform_operation, try_fold_unary_operation);
    }

    if (Flags::fold_flushes) {
        // Register flush operantion folds.

        ADD_POLYHEDRON_OPERATION_REWRITES(
            Polyhedron_flush_operation, try_fold_unary_operation);

        insert({typeid(Polygon_flush_operation),
                try_fold_unary_operation<Polygon_flush_operation>});
    }

    if (Flags::fold_offsets) {
    // Register polygon offset operantion folds.

        insert({typeid(Polygon_offset_operation),
                try_fold_unary_operation<Polygon_offset_operation>});
    }

#undef ADD_POLYGON_OPERATION_REWRITES
#undef ADD_POLYHEDRON_OPERATION_REWRITES
}

// We add a small function to look up an operation by its type and try
// to apply a rewrite, if there are any.

bool Operation_rewriter::try_rewrite(Operation *op) const
{
    const auto it = find(typeid(*op));
    return it != end() && it->second(op);
}
