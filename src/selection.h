#ifndef SELECTION_H
#define SELECTION_H

#include <boost/graph/graph_traits.hpp>

#include "compose_tag.h"
#include "polyhedron_types.h"
#include "bounding_volumes.h"
#include "basic_operations.h"

class Face_selector {
public:
    virtual std::string describe() const = 0;
    virtual std::vector<Polyhedron::Facet_handle> apply(
        Polyhedron &mesh) const = 0;
    virtual std::vector<Surface_mesh::Face_index> apply(
        const Surface_mesh &mesh) const = 0;
};

class Vertex_selector {
public:
    virtual std::string describe() const = 0;
    virtual std::vector<Polyhedron::Vertex_handle> apply(
        Polyhedron &mesh) const = 0;
    virtual std::vector<Surface_mesh::Vertex_index> apply(
        const Surface_mesh &mesh) const = 0;
};

class Edge_selector {
public:
    virtual std::string describe() const = 0;
    virtual std::vector<boost::graph_traits<Polyhedron>::edge_descriptor>
    apply(Polyhedron &mesh) const = 0;
    virtual std::vector<boost::graph_traits<Surface_mesh>::edge_descriptor>
    apply(const Surface_mesh &mesh) const = 0;
};

// Bounded

class Bounded_face_selector: public Face_selector {
    std::shared_ptr<Bounding_volume> volume;
    const bool partial;

public:
    Bounded_face_selector(const std::shared_ptr<Bounding_volume> &p, bool q):
        volume(p), partial(q) {}

    std::string describe() const {
        return compose_tag(
            partial ? "faces_partially_in" : "faces_in", volume);
    }

    std::vector<Polyhedron::Facet_handle> apply(
        Polyhedron &mesh) const override;
    std::vector<Surface_mesh::Face_index> apply(
        const Surface_mesh &mesh) const override;
};

class Bounded_vertex_selector: public Vertex_selector {
    std::shared_ptr<Bounding_volume> volume;

public:
    Bounded_vertex_selector(const std::shared_ptr<Bounding_volume> &v):
        volume(v) {}

    std::string describe() const {
        return compose_tag("vertices_in", volume);
    }

    std::vector<Polyhedron::Vertex_handle> apply(
        Polyhedron &mesh) const override;
    std::vector<Surface_mesh::Vertex_index> apply(
        const Surface_mesh &mesh) const override;
};

class Bounded_edge_selector: public Edge_selector {
    std::shared_ptr<Bounding_volume> volume;
    const bool partial;

public:
    Bounded_edge_selector(const std::shared_ptr<Bounding_volume> &p, bool q):
        volume(p), partial(q) {}

    std::string describe() const {
        return compose_tag(
            partial ? "edges_partially_in" : "edges_in", volume);
    }

    std::vector<boost::graph_traits<Polyhedron>::edge_descriptor> apply(
        Polyhedron &mesh) const override;
    std::vector<boost::graph_traits<Surface_mesh>::edge_descriptor> apply(
        const Surface_mesh &mesh) const override;
};

// Relative

class Relative_face_selector: public Face_selector {
    std::shared_ptr<Face_selector> selector;
    const int steps;

public:
    Relative_face_selector(
        const std::shared_ptr<Face_selector> &p, int n):
        selector(p), steps(n) {}

    std::string describe() const {
        return compose_tag(
            steps >= 0 ? "expand" : "contract", selector, std::abs(steps));
    }

    std::vector<Polyhedron::Facet_handle> apply(
        Polyhedron &mesh) const override;
    std::vector<Surface_mesh::Face_index> apply(
        const Surface_mesh &mesh) const override;
};

class Relative_vertex_selector: public Vertex_selector {
    std::shared_ptr<Vertex_selector> selector;
    const int steps;

public:
    Relative_vertex_selector(
        const std::shared_ptr<Vertex_selector> &p, int n):
        selector(p), steps(n) {}

    std::string describe() const {
        return compose_tag(
            steps >= 0 ? "expand" : "contract", selector, std::abs(steps));
    }

    std::vector<Polyhedron::Vertex_handle> apply(
        Polyhedron &mesh) const override;
    std::vector<Surface_mesh::Vertex_index> apply(
        const Surface_mesh &mesh) const override;
};

class Relative_edge_selector: public Edge_selector {
    std::shared_ptr<Edge_selector> selector;
    const int steps;

public:
    Relative_edge_selector(
        const std::shared_ptr<Edge_selector> &p, int n):
        selector(p), steps(n) {}

    std::string describe() const {
        return compose_tag(
            steps >= 0 ? "expand" : "contract", selector, std::abs(steps));
    }

    std::vector<boost::graph_traits<Polyhedron>::edge_descriptor> apply(
        Polyhedron &mesh) const override;
    std::vector<boost::graph_traits<Surface_mesh>::edge_descriptor> apply(
        const Surface_mesh &mesh) const override;
};

// Converting

#define DEFINE_CONVERTING_VERTEX_SELECTOR(FROM)                         \
class FROM ##_to_vertex_selector: public Vertex_selector {              \
    std::shared_ptr<FROM ##_selector> selector;                         \
                                                                        \
public:                                                                 \
    FROM ##_to_vertex_selector(                                         \
        const std::shared_ptr<FROM ##_selector> &p): selector(p) {}     \
                                                                        \
    std::string describe() const {                                      \
        return compose_tag("vertices_in", selector);                    \
    }                                                                   \
                                                                        \
    std::vector<Polyhedron::Vertex_handle> apply(                       \
        Polyhedron &mesh) const override;                               \
    std::vector<Surface_mesh::Vertex_index> apply(                      \
        const Surface_mesh &mesh) const override;                       \
};

DEFINE_CONVERTING_VERTEX_SELECTOR(Face)
DEFINE_CONVERTING_VERTEX_SELECTOR(Edge)

#undef DEFINE_CONVERTING_VERTEX_SELECTOR

#define DEFINE_CONVERTING_FACE_SELECTOR(FROM)                           \
class FROM ##_to_face_selector: public Face_selector {                  \
    std::shared_ptr<FROM ##_selector> selector;                         \
    const bool partial;                                                 \
                                                                        \
public:                                                                 \
    FROM ##_to_face_selector(                                           \
        const std::shared_ptr<FROM ##_selector> &p, bool q):            \
        selector(p), partial(q) {}                                      \
                                                                        \
    std::string describe() const {                                      \
        return compose_tag(                                             \
            partial ? "faces_partially_in" : "faces_in", selector);     \
    }                                                                   \
                                                                        \
    std::vector<Polyhedron::Facet_handle> apply(                        \
        Polyhedron &mesh) const override;                               \
    std::vector<Surface_mesh::Face_index> apply(                        \
        const Surface_mesh &mesh) const override;                       \
};

DEFINE_CONVERTING_FACE_SELECTOR(Vertex)
DEFINE_CONVERTING_FACE_SELECTOR(Edge)

#undef DEFINE_CONVERTING_FACE_SELECTOR

#define DEFINE_CONVERTING_EDGE_SELECTOR(FROM)                           \
class FROM ##_to_edge_selector: public Edge_selector {                  \
    std::shared_ptr<FROM ##_selector> selector;                         \
    const bool partial;                                                 \
                                                                        \
public:                                                                 \
    FROM ##_to_edge_selector(                                           \
        const std::shared_ptr<FROM ##_selector> &p, bool q):            \
        selector(p), partial(q) {}                                      \
                                                                        \
    std::string describe() const {                                      \
        return compose_tag(                                             \
            partial ? "edges_partially_in" : "edges_in", selector);     \
    }                                                                   \
                                                                        \
    std::vector<boost::graph_traits<Polyhedron>::edge_descriptor> apply( \
        Polyhedron &mesh) const override;                               \
    std::vector<boost::graph_traits<Surface_mesh>::edge_descriptor> apply( \
        const Surface_mesh &mesh) const override;                       \
};

DEFINE_CONVERTING_EDGE_SELECTOR(Vertex)
DEFINE_CONVERTING_EDGE_SELECTOR(Face)

#undef DEFINE_CONVERTING_EDGE_SELECTOR

// Boolean set operations

#define DEFINE_SET_OPERATION(OP, WHAT, T, HANDLE, INDEX)        \
class Set_## OP ##_## WHAT ##_selector: public T {              \
    std::vector<std::shared_ptr<T>> selectors;                  \
                                                                \
public:                                                         \
    Set_## OP ##_## WHAT ##_selector(                           \
        std::vector<std::shared_ptr<T>> &&v):                   \
    selectors(std::move(v)) {}                                  \
                                                                \
    std::string describe() const {                              \
        return compose_tag(#OP, selectors);                     \
    }                                                           \
                                                                \
    std::vector<HANDLE> apply(                                  \
        Polyhedron &mesh) const override;                       \
    std::vector<INDEX> apply(                                   \
        const Surface_mesh &mesh) const override;               \
};

DEFINE_SET_OPERATION(union, face, Face_selector,
                     Polyhedron::Facet_handle, Surface_mesh::Face_index)
DEFINE_SET_OPERATION(difference, face, Face_selector,
                     Polyhedron::Facet_handle, Surface_mesh::Face_index)
DEFINE_SET_OPERATION(intersection, face, Face_selector,
                     Polyhedron::Facet_handle, Surface_mesh::Face_index)

DEFINE_SET_OPERATION(union, vertex, Vertex_selector,
                     Polyhedron::Vertex_handle, Surface_mesh::Vertex_index)
DEFINE_SET_OPERATION(difference, vertex, Vertex_selector,
                     Polyhedron::Vertex_handle, Surface_mesh::Vertex_index)
DEFINE_SET_OPERATION(intersection, vertex, Vertex_selector,
                     Polyhedron::Vertex_handle, Surface_mesh::Vertex_index)

DEFINE_SET_OPERATION(union, edge, Edge_selector,
                     boost::graph_traits<Polyhedron>::edge_descriptor,
                     boost::graph_traits<Surface_mesh>::edge_descriptor)
DEFINE_SET_OPERATION(difference, edge, Edge_selector,
                     boost::graph_traits<Polyhedron>::edge_descriptor,
                     boost::graph_traits<Surface_mesh>::edge_descriptor)
DEFINE_SET_OPERATION(intersection, edge, Edge_selector,
                     boost::graph_traits<Polyhedron>::edge_descriptor,
                     boost::graph_traits<Surface_mesh>::edge_descriptor)

#undef DEFINE_SET_OPERATION

#define DEFINE_COMPLEMENT_OPERATION(WHAT, T, HANDLE, INDEX)             \
class Set_complement_## WHAT ##_selector: public T {                    \
    std::shared_ptr<T> selector;                                        \
                                                                        \
public:                                                                 \
    Set_complement_## WHAT ##_selector(const std::shared_ptr<T> &p):    \
        selector(p) {}                                                  \
                                                                        \
    std::string describe() const {                                      \
        return compose_tag("complement", selector);                     \
    }                                                                   \
                                                                        \
    std::vector<HANDLE> apply(                                          \
        Polyhedron &mesh) const override;                               \
    std::vector<INDEX> apply(                                           \
        const Surface_mesh &mesh) const override;                       \
};

DEFINE_COMPLEMENT_OPERATION(face, Face_selector,
                            Polyhedron::Facet_handle, Surface_mesh::Face_index)
DEFINE_COMPLEMENT_OPERATION(vertex, Vertex_selector,
                            Polyhedron::Vertex_handle, Surface_mesh::Vertex_index)
DEFINE_COMPLEMENT_OPERATION(edge, Edge_selector,
                            boost::graph_traits<Polyhedron>::edge_descriptor,
                            boost::graph_traits<Surface_mesh>::edge_descriptor)

#undef DEFINE_COMPLEMENT_OPERATION

// Tag composition

template<typename T>
struct compose_tag_helper<std::shared_ptr<T>,
                          std::enable_if_t<
                              std::is_same_v<Face_selector, T>
                              || std::is_same_v<Vertex_selector, T>
                              || std::is_same_v<Edge_selector, T>>> {
    static void compose(std::ostringstream &s, const std::shared_ptr<T> &x) {
        if (x) {
            s << x->describe() << ",";
        }
    }
};

#endif
