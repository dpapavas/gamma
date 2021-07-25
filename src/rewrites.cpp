#include "kernel.h"
#include "polygon_operations.h"
#include "circle_polygon_types.h"
#include "conic_polygon_types.h"
#include "polyhedron_operations.h"
#include "transform_operation.h"
#include "evaluation.h"

static void update_annotation(Operation *op)
{
    auto it = op->annotations.find("rewrites");

    if (it == op->annotations.end()) {
        op->annotations.insert({"rewrites", "1"});
    } else {
        const int n = std::stoi(it->second);
        it->second = std::to_string(n + 1);
    }
}

static void retag(Operation *op)
{
    const std::string k = op->get_tag();
    op->reset_tag();
    rehash_operation(k);

    for (Operation *x: op->successors) {
        retag(x);
    }
}

//////////////////////////////////
// Transformation chain folding //
//////////////////////////////////

template<typename T>
bool Sequentially_foldable_operation<T>::try_fold()
{
    // If we're preceded by another transformation operation with no
    // other successors (i.e. otherwise unused), we can fold our
    // predecessor into ourself.

    assert(this->predecessors.size() == 1);
    Operation *p = *this->predecessors.begin();

    if (p->successors.size() > 1) {
        return false;
    }

    if (!fold_operand(static_cast<const T *>(p))) {
        return false;
    }

    Sequentially_foldable_operation<T> *q =
        static_cast<Sequentially_foldable_operation<T> *>(p);

    assert(p->successors.size() == 1);
    assert(p->predecessors.size() == 1);

    // +------+ ----> +---+ ----> +---+
    // | THIS |       | P |       | X |
    // +------+ <---- +---+ <---- +---+

    // Folds to:

    // +----------+ ----> +---+
    // | THIS * P |       | X |
    // +----------+ <---- +---+

    // Unlink and remove the to-be-folded operation from the
    // evaluation graph.

    q->unlink_from(this);
    q->unlink_from(q->operand.get());

    assert(q->predecessors.size() == 0);
    assert(q->successors.size() == 0);
    safely_assert(erase_operation(p));

    // Fold the predecessor into ourself and update the linkage.

    this->operand = q->operand;
    this->link();

    retag(this);
    update_annotation(this);

    return true;
}

#include "circle_polygon_types.h"
#include "conic_polygon_types.h"

template bool Sequentially_foldable_operation<
    Polygon_operation<Polygon_set>>::try_fold();
template bool Sequentially_foldable_operation<
    Polygon_operation<Circle_polygon_set>>::try_fold();
template bool Sequentially_foldable_operation<
    Polygon_operation<Conic_polygon_set>>::try_fold();
template bool Sequentially_foldable_operation<
    Polyhedron_operation<Polyhedron>>::try_fold();
template bool Sequentially_foldable_operation<
    Polyhedron_operation<Nef_polyhedron>>::try_fold();
template bool Sequentially_foldable_operation<
    Polyhedron_operation<Surface_mesh>>::try_fold();

/////////////////////////////////////
// Boolean operation chain folding //
/////////////////////////////////////

// In order for a chain to be foldable, its links should have a single
// successor, which should be a boolean operation forming the next
// link.  One predecessor of each link will be the previous link,
// while the other should not be a boolean operation.  This avoids
// complications with interlinked chains and also enables iteratively
// folding the same chain.

template<typename T>
static std::optional<std::pair<T *, Operation *>> test_link(T *op)
{
    // Is single successor a link?

    if (op->successors.size() != 1) {
        return {};
    }

    auto *p = dynamic_cast<T *>(*(op->successors.begin()));
    Operation *q = nullptr;

    if (!p) {
        return {};
    }

    // Is other predecessor not a link?

    auto it = p->predecessors.begin();
    q = *it;

    if (q == op) {
        // Don't include a successor, if it is a self-join of the
        // chain so far.

        if (++it == p->predecessors.end()) {
            return {};
        }

        q = *it;
    }

    if (dynamic_cast<T *>(q)) {
        return {};
    }

    return std::pair(p, q);
}

#define FOLD_ASSOCIATIVE_OPERATION(OP, P)                               \
{                                                                       \
    /* If we're not preceded by an operation that forms a chain with    \
     * us, then we're the start of a (potential) chain. */              \
                                                                        \
    for (auto it = this->predecessors.begin();                          \
         it != this->predecessors.end();                                \
         it++) {                                                        \
        if (auto p = dynamic_cast<OP *>(*it);                           \
            p && test_link(p)) {                                        \
            return false;                                               \
        }                                                               \
    }                                                                   \
                                                                        \
    /* +---+ ----> ... ----> +---+ ----> +---+ ----> ... ----> +---+    \
     * | * |                 | L |       | M |                 | Z |    \
     * +---+ < - - ... < - - +---+ < - - +---+ < - - ... < - - +---+    \
     *                                     ^                     ^      \
     *                                     |                     |      \
     *                                   +---+                 +---+    \
     *                                   | * |                 | X |    \
     *                                   +---+                 +---+    \
     *                                                                  \
     * Folds to:                                                        \
     *                                                                  \
     * +---+ ----> ... ----> +---+ ----> +---+ <---- ... <---- +---+     +---+ \
     * | * |                 | L |       | Z |                 | M | <-- | * | \
     * +---+ < - - ... < - - +---+ < - - +---+ - - > ... - - > +---+     +---+ \
     *                                                           ^      \
     *                                                           |      \
     *                                                         +---+    \
     *                                                         | X |    \
     *                                                         +---+ */ \
                                                                        \
    /* Trace the chain and find Z and X. */                             \
                                                                        \
    int n = 1;                                                          \
    OP *z = this;                                                       \
    std::shared_ptr<P> x;                                               \
                                                                        \
    while (true) {                                                      \
        auto p = test_link(z);                                          \
        Operation *q;                                                   \
                                                                        \
        if (!p) {                                                       \
            break;                                                      \
        }                                                               \
                                                                        \
        std::tie(z, q) = p.value();                                     \
                                                                        \
        if (z->first.get() == q) {                                      \
            x = z->first;                                               \
        } else {                                                        \
            x = z->second;                                              \
        }                                                               \
                                                                        \
        n += 1;                                                         \
    }                                                                   \
                                                                        \
    if (n < 3) {                                                        \
        return false;                                                   \
    }                                                                   \
                                                                        \
    /* Retrace the chain and find M and L. */                           \
                                                                        \
    OP *m = this;                                                       \
    std::shared_ptr<P> l;                                               \
                                                                        \
    {                                                                   \
        Operation *p = nullptr;                                         \
        for (int i = 0; i < n / 2; i++, p = m,                          \
                 m = static_cast<OP *>(                                 \
                     *(m->successors.begin())));                        \
                                                                        \
        if (m->first.get() == p) {                                      \
            l = m->first;                                               \
        } else {                                                        \
            l = m->second;                                              \
        }                                                               \
    }                                                                   \
                                                                        \
    /* Break the chain in the middle, between L and M and detach Z      \
     * from X. */                                                       \
                                                                        \
    m->unlink_from(l.get());                                            \
    z->unlink_from(x.get());                                            \
                                                                        \
    /* Attach Z to L. */                                                \
                                                                        \
    if (z->first == x) {                                                \
        z->first = l;                                                   \
    } else {                                                            \
        assert(z->second == x);                                         \
        z->second = l;                                                  \
    }                                                                   \
                                                                        \
    z->link();                                                          \
                                                                        \
    retag(z);                                                           \
    update_annotation(z);                                               \
                                                                        \
    /* Attach X to M. */                                                \
                                                                        \
    if (m->first == l) {                                                \
        m->first = x;                                                   \
    } else {                                                            \
        assert(m->second == l);                                         \
        m->second = x;                                                  \
    }                                                                   \
                                                                        \
    m->link();                                                          \
                                                                        \
    retag(m);                                                           \
    update_annotation(m);                                               \
                                                                        \
    return true;                                                        \
}

template<typename T>
bool Polyhedron_join_operation<T>::try_fold()
FOLD_ASSOCIATIVE_OPERATION(Polyhedron_join_operation<T>,
                           Polyhedron_operation<T>)

template bool Polyhedron_join_operation<Polyhedron>::try_fold();
template bool Polyhedron_join_operation<Nef_polyhedron>::try_fold();
template bool Polyhedron_join_operation<Surface_mesh>::try_fold();

template<typename T>
bool Polyhedron_intersection_operation<T>::try_fold()
FOLD_ASSOCIATIVE_OPERATION(Polyhedron_intersection_operation<T>,
                           Polyhedron_operation<T>)

template bool Polyhedron_intersection_operation<Polyhedron>::try_fold();
template bool Polyhedron_intersection_operation<Nef_polyhedron>::try_fold();
template bool Polyhedron_intersection_operation<Surface_mesh>::try_fold();

#define FOLD_DIFFERENCE_OPERATION(OP, J, P)                             \
{                                                                       \
    /* Is this the start of a difference chain? */                      \
                                                                        \
    for (auto it = this->predecessors.begin();                          \
         it != this->predecessors.end();                                \
         it++) {                                                        \
        if (auto p = dynamic_cast<OP *>(*it);                           \
            p && test_link(p)) {                                        \
            return false;                                               \
        }                                                               \
    }                                                                   \
                                                                        \
    /* We can't fold subtraction chains, since the subtraction          \
     * operation is not associative.  Instead, we transform             \
     *                                                                  \
     *     A - B - C - D - ...                                          \
     *                                                                  \
     * to                                                               \
     *                                                                  \
     *     A - (B + C + D + ...).                                       \
     *                                                                  \
     * This transformation alone seems to result in a significant       \
     * speed-up for corefinement-based operations, but we can then      \
     * fold the resulting chain of unions as well.                      \
     *                                                                  \
     * First, trace the chain and measure its length. */                \
                                                                        \
    int n = 1;                                                          \
                                                                        \
    {                                                                   \
        OP *x = this;                                                   \
                                                                        \
        while (true) {                                                  \
            auto p = test_link(x);                                      \
            Operation *q;                                               \
                                                                        \
            if (!p) {                                                   \
                break;                                                  \
            }                                                           \
                                                                        \
            std::tie(x, q) = p.value();                                 \
                                                                        \
            if (x->first.get() == q) {                                  \
                break;                                                  \
            }                                                           \
                                                                        \
            n++;                                                        \
        }                                                               \
    }                                                                   \
                                                                        \
    if (n < 3) {                                                        \
        return false;                                                   \
    }                                                                   \
                                                                        \
    /* +---+     +---+ ----> +---+ ----> ... ----> +---+ ----> +---+    \
     * | A | --> | - |       | - |                 | - |       | - |    \
     * +---+     +---+ < - - +---+ < - - ... < - - +---+ < - - +---+    \
     *             ^           ^                     ^           ^      \
     *             |           |                     |           |      \
     *           +---+       +---+                 +---+       +---+    \
     *           | B |       | C |                 | * |       | * |    \
     *           +---+       +---+                 +---+       +---+ */ \
                                                                        \
    /* Go again, unlinking subtrahends from the difference operation    \
     * and creating union operations in their place.  Stop short of     \
     * the last operation, which we keep as the difference yielding     \
     * the final result. */                                             \
                                                                        \
    {                                                                   \
        OP *x;                                                          \
        std::shared_ptr<P> p;                                           \
                                                                        \
        this->unlink_from(this->first.get());                           \
        this->unlink_from(this->second.get());                          \
                                                                        \
        for (x = this, p = this->second; --n > 0; ) {                   \
            assert(x->predecessors.size() == 0);                        \
            assert(x->successors.size() == 1);                          \
            safely_assert(erase_operation(x));                          \
                                                                        \
            x = dynamic_cast<OP *>(*x->successors.begin());             \
                                                                        \
            assert(x);                                                  \
                                                                        \
            /* Unlink the subtrahend and add it to the union chain. */  \
                                                                        \
            x->unlink_from(x->first.get());                             \
            x->unlink_from(x->second.get());                            \
            p = add_operation<J, P>(p, x->second);                      \
            update_annotation(p.get());                                 \
        }                                                               \
                                                                        \
        /* Repurpose the last link. */                                  \
                                                                        \
        assert(x->predecessors.size() == 0);                            \
                                                                        \
        x->first = this->first;                                         \
        x->second = p;                                                  \
        x->link();                                                      \
                                                                        \
        retag(x);                                                       \
        update_annotation(x);                                           \
    }                                                                   \
                                                                        \
    return true;                                                        \
}

template<typename T>
bool Polyhedron_difference_operation<T>::try_fold()
FOLD_DIFFERENCE_OPERATION(Polyhedron_difference_operation<T>,
                          Polyhedron_join_operation<T>,
                          Polyhedron_operation<T>)

template bool Polyhedron_difference_operation<Polyhedron>::try_fold();
template bool Polyhedron_difference_operation<Nef_polyhedron>::try_fold();
template bool Polyhedron_difference_operation<Surface_mesh>::try_fold();

// Polygons

template<typename T>
bool Polygon_join_operation<T>::try_fold()
FOLD_ASSOCIATIVE_OPERATION(Polygon_join_operation<T>,
                           Polygon_operation<T>)

template bool Polygon_join_operation<Polygon_set>::try_fold();
template bool Polygon_join_operation<Circle_polygon_set>::try_fold();
template bool Polygon_join_operation<Conic_polygon_set>::try_fold();

template<typename T>
bool Polygon_intersection_operation<T>::try_fold()
FOLD_ASSOCIATIVE_OPERATION(Polygon_intersection_operation<T>,
                           Polygon_operation<T>)

template bool Polygon_intersection_operation<Polygon_set>::try_fold();
template bool Polygon_intersection_operation<Circle_polygon_set>::try_fold();
template bool Polygon_intersection_operation<Conic_polygon_set>::try_fold();

template<typename T>
bool Polygon_difference_operation<T>::try_fold()
FOLD_DIFFERENCE_OPERATION(Polygon_difference_operation<T>,
                          Polygon_join_operation<T>,
                          Polygon_operation<T>)

template bool Polygon_difference_operation<Polygon_set>::try_fold();
template bool Polygon_difference_operation<Circle_polygon_set>::try_fold();
template bool Polygon_difference_operation<Conic_polygon_set>::try_fold();
