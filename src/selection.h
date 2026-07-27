// Copyright 2022, 2026 Dimitris Papavasiliou

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
        Surface_mesh &mesh) const = 0;

    virtual std::vector<Polyhedron::Facet_handle> apply(
        Polyhedron &mesh,
        std::unordered_map<std::string, std::string> &annotations) const {
        return apply(mesh);
    };

    virtual std::vector<Surface_mesh::Face_index> apply(
        Surface_mesh &mesh,
        std::unordered_map<std::string, std::string> &annotations) const {
        return apply(mesh);
    };
};

class Edge_selector {
public:
    virtual std::string describe() const = 0;
    virtual std::vector<boost::graph_traits<Polyhedron>::edge_descriptor>
    apply(Polyhedron &mesh) const = 0;
    virtual std::vector<boost::graph_traits<Surface_mesh>::edge_descriptor>
    apply(Surface_mesh &mesh) const = 0;

    virtual std::vector<boost::graph_traits<Polyhedron>::edge_descriptor>
    apply(
        Polyhedron &mesh,
        std::unordered_map<std::string, std::string> &annotations) const {
        return apply(mesh);
    };

    virtual std::vector<boost::graph_traits<Surface_mesh>::edge_descriptor>
    apply(
        Surface_mesh &mesh,
        std::unordered_map<std::string, std::string> &annotations) const {
        return apply(mesh);
    };
};

class Vertex_selector {
public:
    virtual std::string describe() const = 0;
    virtual std::vector<Polyhedron::Vertex_handle> apply(
        Polyhedron &mesh) const = 0;
    virtual std::vector<Surface_mesh::Vertex_index> apply(
        Surface_mesh &mesh) const = 0;

    virtual std::vector<Polyhedron::Vertex_handle> apply(
        Polyhedron &mesh,
        std::unordered_map<std::string, std::string> &annotations) const {
        return apply(mesh);
    };

    virtual std::vector<Surface_mesh::Vertex_index> apply(
        Surface_mesh &mesh,
        std::unordered_map<std::string, std::string> &annotations) const {
        return apply(mesh);
    };
};

class Annotating_face_selector: public Face_selector {
public:
    using Face_selector::apply;

    std::vector<Polyhedron::Facet_handle> apply(
        Polyhedron &mesh) const override {
        std::unordered_map<std::string, std::string> map;
        return apply(mesh, map);
    };
    std::vector<Surface_mesh::Face_index> apply(
        Surface_mesh &mesh) const override {
        std::unordered_map<std::string, std::string> map;
        return apply(mesh, map);
    };
};

class Annotating_edge_selector: public Edge_selector {
public:
    using Edge_selector::apply;

    std::vector<boost::graph_traits<Polyhedron>::edge_descriptor> apply(
        Polyhedron &mesh) const override {
        std::unordered_map<std::string, std::string> map;
        return apply(mesh, map);
    };
    std::vector<boost::graph_traits<Surface_mesh>::edge_descriptor> apply(
        Surface_mesh &mesh)
        const override {
        std::unordered_map<std::string, std::string> map;
        return apply(mesh, map);
    };
};

class Annotating_vertex_selector: public Vertex_selector {
public:
    using Vertex_selector::apply;

    std::vector<Polyhedron::Vertex_handle> apply(
        Polyhedron &mesh) const override {
        std::unordered_map<std::string, std::string> map;
        return apply(mesh, map);
    };
    std::vector<Surface_mesh::Vertex_index> apply(
        Surface_mesh &mesh) const override {
        std::unordered_map<std::string, std::string> map;
        return apply(mesh, map);
    };
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
        Surface_mesh &mesh) const override;
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
        Surface_mesh &mesh) const override;
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
        Surface_mesh &mesh) const override;
};

// Feature-based

template<typename P>
class Sharp_edge_selector: public Annotating_edge_selector {
    P parameter;

public:
    Sharp_edge_selector(const P &p): parameter(p) {}

    std::string describe() const {
        const char *s = (
            std::is_same_v<P, FT>
            ? "edges_by_sharpness_angle"
            : "edges_by_sharpness_mode");

        return compose_tag(s, parameter);
    }

    std::vector<boost::graph_traits<Polyhedron>::edge_descriptor> apply(
        Polyhedron &mesh,
        std::unordered_map<std::string, std::string> &annotations)
        const override;
    std::vector<boost::graph_traits<Surface_mesh>::edge_descriptor> apply(
        Surface_mesh &mesh,
        std::unordered_map<std::string, std::string> &annotations)
        const override;
};

template<typename P>
class Sharp_patch_face_selector: public Annotating_face_selector {
    P parameter;
    std::vector<int> patches;
    std::shared_ptr<Face_selector> selector;

public:
    Sharp_patch_face_selector(const P &p, const std::vector<int> &is):
        parameter(p), patches(is) {
        std::sort(patches.begin(), patches.end());
        for (auto it = patches.begin();
             it != patches.end() && *it < 1;
             it = patches.erase(it));
    }

    Sharp_patch_face_selector(
        const P &p, const std::shared_ptr<Face_selector> &q):
        parameter(p), selector(q) {}

    std::string describe() const {
        const char *s = (
            std::is_same_v<P, FT>
            ? "faces_by_sharpness_angle"
            : "faces_by_sharpness_mode");

        return (
            selector
            ? compose_tag(s, parameter, selector)
            : compose_tag(s, parameter, patches));
    }

    std::vector<Polyhedron::Facet_handle> apply(
        Polyhedron &mesh,
        std::unordered_map<std::string, std::string> &annotations)
        const override;
    std::vector<Surface_mesh::Face_index> apply(
        Surface_mesh &mesh,
        std::unordered_map<std::string, std::string> &annotations)
        const override;
};

// By intersection

class Intersecting_face_selector: public Face_selector {
public:
    std::variant<Segment_3, Ray_3, Line_3, Plane_3> query;

    template<typename Q>
    Intersecting_face_selector(const Q &x): query(x) {}

    std::string describe() const {
        return std::visit(
            [] (auto &&x) {
                return compose_tag("faces_through", x);
            }, query);
    }

    std::vector<Polyhedron::Facet_handle> apply(
        Polyhedron &mesh) const override;
    std::vector<Surface_mesh::Face_index> apply(
        Surface_mesh &mesh) const override;
};

class Intersecting_edge_selector: public Edge_selector {
public:
    std::variant<Segment_3, Ray_3, Line_3, Plane_3> query;

    template<typename Q>
    Intersecting_edge_selector(const Q &x): query(x) {}

    std::string describe() const {
        return std::visit(
            [] (auto &&x) {
                return compose_tag("edges_through", x);
            }, query);
    }

    std::vector<boost::graph_traits<Polyhedron>::edge_descriptor> apply(
        Polyhedron &mesh) const override;
    std::vector<boost::graph_traits<Surface_mesh>::edge_descriptor> apply(
        Surface_mesh &mesh) const override;
};

// Relative

class Relative_face_selector: public Annotating_face_selector {
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
        Polyhedron &mesh,
        std::unordered_map<std::string, std::string> &annotations)
        const override;
    std::vector<Surface_mesh::Face_index> apply(
        Surface_mesh &mesh,
        std::unordered_map<std::string, std::string> &annotations)
        const override;
};

class Relative_vertex_selector: public Annotating_vertex_selector {
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
        Polyhedron &mesh,
        std::unordered_map<std::string, std::string> &annotations)
        const override;
    std::vector<Surface_mesh::Vertex_index> apply(
        Surface_mesh &mesh,
        std::unordered_map<std::string, std::string> &annotations)
        const override;
};

class Relative_edge_selector: public Annotating_edge_selector {
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
        Polyhedron &mesh,
        std::unordered_map<std::string, std::string> &annotations)
        const override;
    std::vector<boost::graph_traits<Surface_mesh>::edge_descriptor> apply(
        Surface_mesh &mesh,
        std::unordered_map<std::string, std::string> &annotations)
        const override;
};

// Converting

#define DEFINE_CONVERTING_VERTEX_SELECTOR(FROM)                         \
class FROM ##_to_vertex_selector: public Annotating_vertex_selector {   \
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
        Polyhedron &mesh,                                               \
        std::unordered_map<std::string, std::string> &annotations)      \
const override;                                                         \
    std::vector<Surface_mesh::Vertex_index> apply(                      \
        Surface_mesh &mesh,                                             \
        std::unordered_map<std::string, std::string> &annotations)      \
        const override;                                                 \
};

DEFINE_CONVERTING_VERTEX_SELECTOR(Face)
DEFINE_CONVERTING_VERTEX_SELECTOR(Edge)

#undef DEFINE_CONVERTING_VERTEX_SELECTOR

#define DEFINE_CONVERTING_FACE_SELECTOR(FROM)                           \
class FROM ##_to_face_selector: public Annotating_face_selector {       \
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
        Polyhedron &mesh,                                               \
        std::unordered_map<std::string, std::string> &annotations)      \
const override;                                                         \
    std::vector<Surface_mesh::Face_index> apply(                        \
        Surface_mesh &mesh,                                             \
        std::unordered_map<std::string, std::string> &annotations)      \
        const override;                                                 \
};

DEFINE_CONVERTING_FACE_SELECTOR(Vertex)
DEFINE_CONVERTING_FACE_SELECTOR(Edge)

#undef DEFINE_CONVERTING_FACE_SELECTOR

#define DEFINE_CONVERTING_EDGE_SELECTOR(FROM)                           \
class FROM ##_to_edge_selector: public Annotating_edge_selector {       \
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
        Polyhedron &mesh,                                               \
        std::unordered_map<std::string, std::string> &annotations)      \
        const override;                                                 \
    std::vector<boost::graph_traits<Surface_mesh>::edge_descriptor> apply( \
        Surface_mesh &mesh,                                             \
        std::unordered_map<std::string, std::string> &annotations)      \
        const override;                                                 \
};

DEFINE_CONVERTING_EDGE_SELECTOR(Vertex)
DEFINE_CONVERTING_EDGE_SELECTOR(Face)

#undef DEFINE_CONVERTING_EDGE_SELECTOR

// Boolean set operations

#define DEFINE_SET_OPERATION(OP, WHAT, WHICH)                           \
class Set_## OP ##_## WHAT ##_selector:                                 \
    public Annotating_## WHAT ##_selector {                             \
    std::vector<std::shared_ptr<WHICH ##_selector>> selectors;          \
                                                                        \
public:                                                                 \
    Set_## OP ##_## WHAT ##_selector(                                   \
        std::vector<std::shared_ptr<WHICH ##_selector>> &&v):           \
    selectors(std::move(v)) {}                                          \
                                                                        \
    std::string describe() const {                                      \
        return compose_tag(#OP, selectors);                             \
    }                                                                   \
                                                                        \
    std::vector<boost::graph_traits<Polyhedron>::WHAT ##_descriptor>    \
        apply(                                                          \
        Polyhedron &mesh,                                               \
        std::unordered_map<std::string, std::string> &annotations)      \
        const override;                                                 \
    std::vector<boost::graph_traits<Surface_mesh>::WHAT ##_descriptor>  \
        apply(                                                          \
        Surface_mesh &mesh,                                             \
        std::unordered_map<std::string, std::string> &annotations)      \
        const override;                                                 \
};

DEFINE_SET_OPERATION(union, face, Face)
DEFINE_SET_OPERATION(difference, face, Face)
DEFINE_SET_OPERATION(intersection, face, Face)

DEFINE_SET_OPERATION(union, vertex, Vertex)
DEFINE_SET_OPERATION(difference, vertex, Vertex)
DEFINE_SET_OPERATION(intersection, vertex, Vertex)

DEFINE_SET_OPERATION(union, edge, Edge)
DEFINE_SET_OPERATION(difference, edge, Edge)
DEFINE_SET_OPERATION(intersection, edge, Edge)

#undef DEFINE_SET_OPERATION

#define DEFINE_COMPLEMENT_OPERATION(WHAT, WHICH)                        \
class Set_complement_## WHAT ##_selector:                               \
    public Annotating_## WHAT ##_selector {                             \
    std::shared_ptr<WHICH ##_selector> selector;                        \
                                                                        \
public:                                                                 \
    Set_complement_## WHAT ##_selector(                                 \
        const std::shared_ptr<WHICH ##_selector> &p):                   \
        selector(p) {}                                                  \
                                                                        \
    std::string describe() const {                                      \
        return compose_tag("complement", selector);                     \
    }                                                                   \
                                                                        \
    std::vector<boost::graph_traits<Polyhedron>::WHAT ##_descriptor>    \
        apply(                                                          \
        Polyhedron &mesh,                                               \
        std::unordered_map<std::string, std::string> &annotations)      \
        const override;                                                 \
    std::vector<boost::graph_traits<Surface_mesh>::WHAT ##_descriptor>  \
        apply(                                                          \
        Surface_mesh &mesh,                                             \
        std::unordered_map<std::string, std::string> &annotations)      \
        const override;                                                 \
};

DEFINE_COMPLEMENT_OPERATION(face, Face)
DEFINE_COMPLEMENT_OPERATION(vertex, Vertex)
DEFINE_COMPLEMENT_OPERATION(edge, Edge)

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

template<typename T, typename F, typename S>
std::vector<S> sort_face_patches(
    const T &mesh, std::unordered_map<F, S> &map, std::size_t i_0, std::size_t n);

#endif
