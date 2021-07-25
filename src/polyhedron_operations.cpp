#include <CGAL/exceptions.h>
#include <CGAL/boost/graph/convert_nef_polyhedron_to_polygon_mesh.h>
#include <CGAL/IO/Nef_polyhedron_iostream_3.h>
#include <CGAL/Polygon_mesh_processing/transform.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/clip.h>
#include <CGAL/Subdivision_method_3/subdivision_methods_3.h>
#include <CGAL/convex_hull_3.h>
#include <CGAL/minkowski_sum_3.h>

#include "assertions.h"
#include "iterators.h"
#include "compressed_stream.h"
#include "options.h"
#include "kernel.h"
#include "projection.h"
#include "polyhedron_operations.h"

#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>

template <typename T, typename U>
static inline void test_result(const U &op, const T &P)
{
    if (Flags::warn_mesh_valid && !CGAL::is_valid_polygon_mesh(P)) {
        op->message(
            Operation::WARNING,
            "result of operation % is not a valid polygon mesh");
    }

    bool is_closed = true;
    if ((Flags::warn_mesh_closed
         || Flags::warn_mesh_bounds
         || Flags::warn_mesh_oriented)
        && !CGAL::is_closed(P)) {
        is_closed = false;
        op->message(
            Operation::WARNING, "result of operation % is not closed");
    }

    if (Flags::warn_mesh_manifold) {
        std::vector<typename boost::graph_traits<T>::halfedge_descriptor> v;
        CGAL::Polygon_mesh_processing::non_manifold_vertices(
            P, std::back_inserter(v));

        if (v.size() > 0) {
            std::ostringstream s;

            s << "result of operation % has " << v.size()
              << " non-manifold vertices";

            op->message(Operation::WARNING, s.str());
        }
    }

    T Q(P);

    CGAL::Polygon_mesh_processing::triangulate_faces(CGAL::faces(Q), Q);

    if (Flags::warn_mesh_degenerate) {
        {
            std::vector<typename boost::graph_traits<T>::edge_descriptor> v;
            CGAL::Polygon_mesh_processing::degenerate_edges(
                Q, std::back_inserter(v));

            if (v.size() > 0) {
                std::ostringstream s;

                s << "result of operation % has " << v.size()
                  << " degenerate edges";

                op->message(Operation::WARNING, s.str());
            }
        }

        {
            std::vector<typename boost::graph_traits<T>::face_descriptor> v;
            CGAL::Polygon_mesh_processing::degenerate_faces(
                Q, std::back_inserter(v));

            if (v.size() > 0) {
                std::ostringstream s;

                s << "result of operation % has " << v.size()
                  << " degenerate faces";

                op->message(Operation::WARNING, s.str());
            }
        }
    }

    bool does_self_intersect = false;
    if (Flags::warn_mesh_intersects
        && CGAL::Polygon_mesh_processing::does_self_intersect<CGAL::Parallel_if_available_tag>(Q)) {
        does_self_intersect = true;
        op->message(
            Operation::WARNING, "result of operation % self-intersects");
    }

    // A mesh needs to be closed to bound a volume; disable the test
    // if it isn't.  Also disable it if it is known to self intersect,
    // as the test has undefine behavior in that case.

    if (Flags::warn_mesh_bounds && is_closed && !does_self_intersect
        && !CGAL::Polygon_mesh_processing::does_bound_a_volume(Q)) {
        op->message(
            Operation::WARNING,
            "result of operation % does not bound a volume");
    }

    // Only closed meshes have a specific orientation.

    if (Flags::warn_mesh_oriented && is_closed
        && !CGAL::Polygon_mesh_processing::is_outward_oriented(Q)) {
        op->message(
            Operation::WARNING,
            "result of operation % is not oriented outward");
    }
}

template<>
bool Polyhedron_operation<Polyhedron>::dispatch()
{
    bool p = Operation::dispatch();

    if (polyhedron) {
        annotations.insert({
                {"vertices", std::to_string(polyhedron->size_of_vertices())},
                {"halfedges", std::to_string(polyhedron->size_of_halfedges())},
                {"facets", std::to_string(polyhedron->size_of_facets())}});

        test_result(this, *polyhedron);
    }

    return p;
}

template<>
bool Polyhedron_operation<Nef_polyhedron>::dispatch()
{
    bool p = Operation::dispatch();

    if (polyhedron) {
        annotations.insert({
                {"vertices", std::to_string(polyhedron->number_of_vertices())},
                {"halfedges", std::to_string(polyhedron->number_of_halfedges())},
                {"edges", std::to_string(polyhedron->number_of_edges())},
                {"halffacets", std::to_string(polyhedron->number_of_halffacets())},
                {"facets", std::to_string(polyhedron->number_of_facets())},
                {"volumes", std::to_string(polyhedron->number_of_volumes())}});
    }

    return p;
}

template<>
bool Polyhedron_operation<Surface_mesh>::dispatch()
{
    bool p = Operation::dispatch();

    if (polyhedron) {
        annotations.insert({
                {"vertices", std::to_string(polyhedron->number_of_vertices())},
                {"halfedges", std::to_string(polyhedron->number_of_halfedges())},
                {"edges", std::to_string(polyhedron->number_of_edges())},
                {"facets", std::to_string(polyhedron->number_of_faces())}});

        test_result(this, *polyhedron);
    }

    return p;
}

template<>
bool Polyhedron_operation<Nef_polyhedron>::store()
{
    assert(Flags::store_operations);
    assert(polyhedron);

    compressed_ofstream_wrapper f(Options::store_compression);
    f.open(store_path);

    if (!f.is_open()) {
        return false;
    }

    bool p = false;
    try {
        f << *polyhedron;
    } catch (const CGAL::Failure_exception &e) {
        p = true;
    }

    if (p || !f.good()) {
        std::ostringstream s;
        s << "Could not store polyhedron % to '" << store_path << "'";
        message(ERROR, s.str());
        return false;
    }

    return true;
}

template<>
bool Polyhedron_operation<Nef_polyhedron>::load()
{
    assert(Flags::load_operations);
    assert(!polyhedron);

    compressed_ifstream_wrapper f(Options::store_compression >= 0);
    f.open(store_path);

    if (!f.is_open()) {
        return false;
    }

    polyhedron = std::make_shared<Nef_polyhedron>();
    bool p = false;

    try {
        f >> *polyhedron;
    } catch (const CGAL::Failure_exception &e) {
        p = true;
    }

    if (p || !f.good()) {
        std::ostringstream s;
        s << "Could not load polyhedron % from '" << store_path << "'";
        message(ERROR, s.str());

        return false;
    }

    return true;
}

template<typename T>
bool Polyhedron_operation<T>::store()
{
    using traits = typename boost::graph_traits<T>;

    assert(Flags::store_operations);
    assert(polyhedron);

    compressed_ofstream_wrapper f(Options::store_compression);
    f.open(store_path);

    const T &G = *polyhedron;
    const auto point_map = CGAL::get(CGAL::vertex_point, G);
    std::unordered_map<typename traits::vertex_descriptor,
                       typename traits::vertices_size_type> index_map;
    typename traits::vertices_size_type i = 0;

    if (!f.is_open()) {
        goto error;
    }

    f << CGAL::vertices(G).size() << '\n';

    for (const typename traits::vertex_descriptor v: CGAL::vertices(G)) {
        const auto p = boost::get(point_map, v);
        f << p.x().exact()
          << " " << p.y().exact()
          << " " << p.z().exact()
          << '\n';

        if (!f.good()) {
            goto error;
        }

        index_map[v] = i++;
    }

    f << CGAL::faces(G).size() << '\n';

    for (const auto g: CGAL::faces(G)) {
        f << CGAL::degree(g, G);

        for (const typename traits::vertex_descriptor v:
                 CGAL::vertices_around_face(CGAL::halfedge(g, G), G)) {
            f << " " << index_map[v];
        }

        f << '\n';

        if (!f.good()) {
            goto error;
        }
    }

    return f.good();

error:
    {
        std::ostringstream s;
        s << "Could not store polyhedron % to '" << store_path << "'";
        message(ERROR, s.str());
    }

    f.close();
    std::remove(store_path.c_str());

    return false;
}

template<typename T>
bool Polyhedron_operation<T>::load()
{
    using traits = typename boost::graph_traits<T>;
    typedef typename boost::property_map<T, CGAL::vertex_point_t>::type Vertex_point_map;
    typedef typename boost::property_traits<Vertex_point_map>::value_type Point;

    assert(Flags::load_operations);
    assert(!polyhedron);

    compressed_ifstream_wrapper f(Options::store_compression >= 0);
    f.open(store_path);

    if (!f.is_open()) {
        return false;
    }

    polyhedron = std::make_shared<T>();

    T &G = *polyhedron;
    const auto point_map = CGAL::get(CGAL::vertex_point, G);
    const auto null = boost::graph_traits<T>::null_face();

    std::vector<typename traits::vertex_descriptor> vertices;
    typename traits::vertices_size_type n;
    typename traits::faces_size_type m;

    f >> n;

    if (!f.good()) {
        goto error;
    }

    vertices.reserve(n);

    for (typename traits::vertices_size_type i = 0; i < n; i++) {
        typename traits::vertex_descriptor v = CGAL::add_vertex(G);
        vertices.push_back(v);

        FT::ET x, y, z;
        f >> x >> y >> z;

        if (!f.good()) {
            goto error;
        }

        boost::put(point_map, v, Point(FT(x), FT(y), FT(z)));
    }

    f >> m;

    if (!f.good()) {
        goto error;
    }

    for (typename traits::faces_size_type i = 0; i < m; i++) {
        int k;

        f >> k;

        if (!f.good()) {
            goto error;
        }

        std::vector<typename traits::vertex_descriptor> v;
        v.reserve(k);

        for (int j = 0; j < k; j++) {
            typename traits::vertices_size_type l;

            f >> l;

            if (!f.good()) {
                goto error;
            }

            v.push_back(vertices[l]);
        }

        if (CGAL::Euler::add_face(v, G) == null) {
            goto error;
        }
    }

    return f.good();

error:
    {
        std::ostringstream s;
        s << "Could not load polyhedron % from '" << store_path << "'";
        message(ERROR, s.str());
    }

    return false;
}

template bool Polyhedron_operation<Polyhedron>::load();
template bool Polyhedron_operation<Surface_mesh>::load();
template bool Polyhedron_operation<Polyhedron>::store();
template bool Polyhedron_operation<Surface_mesh>::store();

///////////////////////////
// Conversion operations //
///////////////////////////

// Convert to Polyhedron

template<>
void Polyhedron_convert_operation<Polyhedron, Nef_polyhedron>::evaluate()
{
    assert(!polyhedron);

    polyhedron = std::make_shared<Polyhedron>();
    operand->get_value()->convert_to_polyhedron(*polyhedron);
}

template<>
void Polyhedron_convert_operation<Polyhedron, Surface_mesh>::evaluate()
{
    assert(!polyhedron);

    polyhedron = std::make_shared<Polyhedron>();
    CGAL::copy_face_graph(*operand->get_value(), *polyhedron);
}

// Convert to Nef_polyhedron

template<typename T>
void Polyhedron_convert_operation<Nef_polyhedron, T>::evaluate()
{
    assert(!this->polyhedron);

    T &P = *this->operand->get_value();
    bool p = !CGAL::is_closed(P);

    if (!p) {
        T Q(P);

        CGAL::Polygon_mesh_processing::triangulate_faces(CGAL::faces(Q), Q);
        p = CGAL::Polygon_mesh_processing::is_outward_oriented(Q);
    }

    if (p) {
        this->polyhedron = std::make_shared<Nef_polyhedron>(P);
    } else {
        Nef_polyhedron N(P);
        this->polyhedron = std::make_shared<Nef_polyhedron>(
            N.complement().closure());
    }
}

template void Polyhedron_convert_operation<Nef_polyhedron,
                                           Polyhedron>::evaluate();

template void Polyhedron_convert_operation<Nef_polyhedron,
                                           Surface_mesh>::evaluate();

// Convert to Surface_mesh

template<>
void Polyhedron_convert_operation<Surface_mesh, Polyhedron>::evaluate()
{
    assert(!polyhedron);

    polyhedron = std::make_shared<Surface_mesh>();
    CGAL::copy_face_graph(*operand->get_value(), *polyhedron);
}

template<>
void Polyhedron_convert_operation<Surface_mesh, Nef_polyhedron>::evaluate()
{
    assert(!polyhedron);

    polyhedron = std::make_shared<Surface_mesh>();

    CGAL::convert_nef_polyhedron_to_polygon_mesh(
        *operand->get_value(), *polyhedron);
}

//////////////////////////
// Transform operations //
//////////////////////////

template<>
void Polyhedron_transform_operation<Polyhedron>::evaluate()
{
    assert(!polyhedron);

    polyhedron = std::make_shared<Polyhedron>(*operand->get_value());
    std::transform(polyhedron->points_begin(), polyhedron->points_end(),
                   polyhedron->points_begin(), transformation);

    if (transformation.is_odd()) {
        polyhedron->inside_out();
    }
}

template<>
void Polyhedron_transform_operation<Nef_polyhedron>::evaluate()
{
    assert(!polyhedron);

    polyhedron = std::make_shared<Nef_polyhedron>(*operand->get_value());
    polyhedron->transform(transformation);
}

template<>
void Polyhedron_transform_operation<Surface_mesh>::evaluate()
{
    assert(!polyhedron);

    polyhedron = std::make_shared<Surface_mesh>(*operand->get_value());

    CGAL::Polygon_mesh_processing::transform(transformation, *polyhedron);

    if (transformation.is_odd()) {
        CGAL::Polygon_mesh_processing::reverse_face_orientations(*polyhedron);
    }
}

#define DEFINE_FLUSH_OPERATION(                                         \
    T, PREAMBLE, FOR_BODY, FOR_PREAMBLE, TRANSFORM)                     \
template <>                                                             \
void Polyhedron_flush_operation<T>::evaluate()                          \
{                                                                       \
    assert(!polyhedron);                                                \
                                                                        \
    polyhedron = std::make_shared<T>(*operand->get_value());            \
                                                                        \
    T &P = *polyhedron;                                                 \
    FT x_min, x_max, y_min, y_max, z_min, z_max;                        \
    bool p = true;                                                      \
                                                                        \
    PREAMBLE;                                                           \
                                                                        \
    for (FOR_BODY) {                                                    \
        FOR_PREAMBLE;                                                   \
                                                                        \
        if (p || A.x() < x_min) {                                       \
            x_min = A.x();                                              \
        }                                                               \
                                                                        \
        if (p || A.x() > x_max) {                                       \
            x_max = A.x();                                              \
        }                                                               \
                                                                        \
        if (p || A.y() < y_min) {                                       \
            y_min = A.y();                                              \
        }                                                               \
                                                                        \
        if (p || A.y() > y_max) {                                       \
            y_max = A.y();                                              \
        }                                                               \
                                                                        \
        if (p || A.z() < z_min) {                                       \
            z_min = A.z();                                              \
        }                                                               \
                                                                        \
        if (p || A.z() > z_max) {                                       \
            z_max = A.z();                                              \
        }                                                               \
                                                                        \
        p = false;                                                      \
    }                                                                   \
                                                                        \
                                                                        \
    const Aff_transformation_3 X(1, 0, 0, (coefficients[0][0] * x_min   \
                                           - coefficients[0][1] * x_max), \
                                 0, 1, 0, (coefficients[1][0] * y_min   \
                                           - coefficients[1][1] * y_max), \
                                 0, 0, 1, (coefficients[2][0] * z_min   \
                                           - coefficients[2][1] * z_max)); \
                                                                        \
    TRANSFORM;                                                          \
}

DEFINE_FLUSH_OPERATION(Nef_polyhedron,
                       ,
                       auto v = P.vertices_begin();
                       v != P.vertices_end();
                       ++v,
                       const Point_3 &A = v->point(),
                       P.transform(X))

#define DEFINE_MESH_FLUSH_OPERATION(T)                                  \
DEFINE_FLUSH_OPERATION(T,                                               \
                       auto map = CGAL::get(CGAL::vertex_point, P),     \
                       const auto &v: CGAL::vertices(P),                \
                       const Point_3 &A = boost::get(map, v),           \
                       CGAL::Polygon_mesh_processing::transform(X, P))

DEFINE_MESH_FLUSH_OPERATION(Polyhedron)
DEFINE_MESH_FLUSH_OPERATION(Surface_mesh)

#undef DEFINE_MESH_FLUSH_OPERATION
#undef DEFINE_FLUSH_OPERATION

template<typename T>
bool Polyhedron_flush_operation<T>::fold_operand(
    const Polyhedron_operation<T> *p)
{
    const Polyhedron_flush_operation<T> *f =
        dynamic_cast<const Polyhedron_flush_operation<T> *>(p);

    if (!f) {
        return false;
    }

    const FT (*b)[2] = this->coefficients, (*a)[2] = f->coefficients;

    // This computation can be done in place, since only one of
    // a[i][0], a[i][1] is non-zero at any given time.

    for (int i = 0; i < 3; i++) {
        this->coefficients[i][0] = (a[i][0] * (1 - b[i][1])
                                    + b[i][0] * (1 + a[i][0]));
        this->coefficients[i][1] = (a[i][1] * (1 + b[i][0])
                                    + b[i][1] * (1 - a[i][1]));
    }

    return true;
}

template bool Polyhedron_flush_operation<Polyhedron>::fold_operand(
    const Polyhedron_operation<Polyhedron> *p);
template bool Polyhedron_flush_operation<Nef_polyhedron>::fold_operand(
    const Polyhedron_operation<Nef_polyhedron> *p);
template bool Polyhedron_flush_operation<Surface_mesh>::fold_operand(
    const Polyhedron_operation<Surface_mesh> *p);

////////////////////////////
// Boolean set operations //
////////////////////////////

#define WARN_NEF(OP, PRE) {                                             \
    bool p = Flags::warn_nef && PRE;                                    \
    if (Flags::warn_manifold || p) {                                    \
        if (OP->polyhedron->is_simple()) {                              \
            if (p) {                                                    \
                OP->message(                                            \
                    OP->WARNING,                                        \
                    "operation % could have been implemented "          \
                    "via corefinement");                                \
            }                                                           \
        } else {                                                        \
            if (Flags::warn_manifold) {                                 \
                OP->message(                                            \
                    OP->WARNING, "result of % is not manifold");        \
            }                                                           \
        }                                                               \
    }                                                                   \
}

#define DEFINE_SET_OPERATION(OP, COOP)                                  \
template<>                                                              \
void Polyhedron_## OP ##_operation<Nef_polyhedron>::evaluate()          \
{                                                                       \
    assert(!this->polyhedron);                                          \
                                                                        \
    bool input_corefinable = true;                                      \
    const Nef_polyhedron &N = *this->first->get_value();                \
    const Nef_polyhedron &M = *this->second->get_value();               \
                                                                        \
    if (Flags::warn_nef) {                                              \
        for (const Nef_polyhedron &X: {N, M}) {                         \
            Surface_mesh S;                                             \
                                                                        \
            CGAL::convert_nef_polyhedron_to_polygon_mesh(X, S, true);   \
            if (CGAL::Polygon_mesh_processing::does_self_intersect(S)   \
                || !CGAL::Polygon_mesh_processing::does_bound_a_volume(S)) { \
                input_corefinable = false;                              \
                break;                                                  \
            }                                                           \
        }                                                               \
    }                                                                   \
                                                                        \
    this->polyhedron = std::make_shared<Nef_polyhedron>(N.OP(M));       \
                                                                        \
    WARN_NEF(this, input_corefinable);                                  \
}                                                                       \
                                                                        \
template<typename T>                                                    \
void Polyhedron_## OP ##_operation<T>::evaluate()                       \
{                                                                       \
    assert(!this->polyhedron);                                          \
                                                                        \
    T P = *this->first->get_value();                                    \
    this->polyhedron = std::make_shared<T>(                             \
        *this->second->get_value());                                    \
                                                                        \
    CGAL::Polygon_mesh_processing::triangulate_faces(                   \
        CGAL::faces(*this->polyhedron), *this->polyhedron);             \
    CGAL::Polygon_mesh_processing::triangulate_faces(CGAL::faces(P), P);\
                                                                        \
    if (!COOP(P, *this->polyhedron, *this->polyhedron)) {               \
        CGAL_error_msg("resulting mesh would not be manifold");         \
    }                                                                   \
}                                                                       \
                                                                        \
template void Polyhedron_## OP ##_operation<Polyhedron>::evaluate();    \
template void Polyhedron_## OP ##_operation<Surface_mesh>::evaluate();

DEFINE_SET_OPERATION(join, CGAL::Polygon_mesh_processing::corefine_and_compute_union)
DEFINE_SET_OPERATION(difference, CGAL::Polygon_mesh_processing::corefine_and_compute_difference)
DEFINE_SET_OPERATION(intersection, CGAL::Polygon_mesh_processing::corefine_and_compute_intersection)

#undef DEFINE_SET_OPERATION

void Polyhedron_symmetric_difference_operation::evaluate()
{
    assert(!this->polyhedron);

    this->polyhedron = std::make_shared<Nef_polyhedron>(
        this->first->get_value()->symmetric_difference(
            *this->second->get_value()));
}

template<>
void Polyhedron_complement_operation<Polyhedron>::evaluate()
{
    assert(!polyhedron);

    polyhedron = std::make_shared<Polyhedron>(*operand->get_value());
    polyhedron->inside_out();
}

template<>
void Polyhedron_complement_operation<Nef_polyhedron>::evaluate()
{
    assert(!polyhedron);

    polyhedron = std::make_shared<Nef_polyhedron>(
        operand->get_value()->complement().closure());
}

template<>
void Polyhedron_complement_operation<Surface_mesh>::evaluate()
{
    assert(!polyhedron);

    polyhedron = std::make_shared<Surface_mesh>(*operand->get_value());
    CGAL::Polygon_mesh_processing::reverse_face_orientations(*polyhedron);
}

void Polyhedron_boundary_operation::evaluate()
{
    assert(!polyhedron);

    polyhedron = std::make_shared<Nef_polyhedron>(
        operand->get_value()->boundary());
}

////////////////////////////
// Subdivision operations //
////////////////////////////

template<typename T>
void Loop_subdivision_operation<T>::evaluate()
{
    assert(!this->polyhedron);

    this->polyhedron = std::make_shared<T>(*this->operand->get_value());

    CGAL::Subdivision_method_3::Loop_subdivision(
        *this->polyhedron, CGAL::parameters::number_of_iterations(this->depth));
}

template void Loop_subdivision_operation<Polyhedron>::evaluate();
template void Loop_subdivision_operation<Surface_mesh>::evaluate();

template<typename T>
void Catmull_clark_subdivision_operation<T>::evaluate()
{
    assert(!this->polyhedron);

    this->polyhedron = std::make_shared<T>(*this->operand->get_value());

    CGAL::Subdivision_method_3::CatmullClark_subdivision(
        *this->polyhedron, CGAL::parameters::number_of_iterations(this->depth));
}

template void Catmull_clark_subdivision_operation<Polyhedron>::evaluate();
template void Catmull_clark_subdivision_operation<Surface_mesh>::evaluate();

template<typename T>
void Doo_sabin_subdivision_operation<T>::evaluate()
{
    assert(!this->polyhedron);

    this->polyhedron = std::make_shared<T>(*this->operand->get_value());

    CGAL::Subdivision_method_3::DooSabin_subdivision(
        *this->polyhedron, CGAL::parameters::number_of_iterations(this->depth));
}

template void Doo_sabin_subdivision_operation<Polyhedron>::evaluate();
template void Doo_sabin_subdivision_operation<Surface_mesh>::evaluate();

template<typename T>
void Sqrt_3_subdivision_operation<T>::evaluate()
{
    assert(!this->polyhedron);

    this->polyhedron = std::make_shared<T>(*this->operand->get_value());

    CGAL::Subdivision_method_3::Sqrt3_subdivision(
        *this->polyhedron, CGAL::parameters::number_of_iterations(this->depth));
}

template void Sqrt_3_subdivision_operation<Polyhedron>::evaluate();
template void Sqrt_3_subdivision_operation<Surface_mesh>::evaluate();

/////////////////
// Convex hull //
/////////////////

class Polyhedron_points_iterator: public std::iterator<std::input_iterator_tag,
                                                       Point_3, std::ptrdiff_t,
                                                       Point_3 *, Point_3 &> {
    typedef std::pair<Surface_mesh::Vertex_range::const_iterator,
                      const Surface_mesh *> Surface_mesh_vertex_const_iterator;

protected:
    std::variant<std::vector<Point_3>::const_iterator,
                 Polyhedron::Vertex_const_iterator,
                 Nef_polyhedron::Vertex_const_iterator,
                 Surface_mesh_vertex_const_iterator> inner;

public:
    Polyhedron_points_iterator(const std::vector<Point_3>::const_iterator &it):
        inner(it) {}
    Polyhedron_points_iterator(const Polyhedron::Vertex_const_iterator &it):
        inner(it) {}
    Polyhedron_points_iterator(const Nef_polyhedron::Vertex_const_iterator &it):
        inner(it) {}
    Polyhedron_points_iterator(
        const Surface_mesh::Vertex_range::const_iterator &it,
        const Surface_mesh *p): inner(std::pair(it, p)) {}

    Polyhedron_points_iterator &operator++() {
        std::visit(
            [](auto &&x) {
                if constexpr (std::is_same_v<
                                  std::decay_t<decltype(x)>,
                                  Surface_mesh_vertex_const_iterator>) {
                    ++x.first;
                } else {
                    ++x;
                }
            },
            inner);

        return *this;
    }

    Polyhedron_points_iterator operator++(int) {
        Polyhedron_points_iterator i = *this;

        ++(*this);

        return i;
    }

    bool operator==(Polyhedron_points_iterator other) const {
        return inner == other.inner;
    }

    bool operator!=(Polyhedron_points_iterator other) const {
        return !(inner == other.inner);
    }

    const Point_3 &operator*() const {
        return std::visit(
            [](auto &&x) -> const Point_3 & {
                if constexpr (std::is_same_v<
                                  std::decay_t<decltype(x)>,
                                  std::vector<Point_3>::const_iterator>) {
                    return *x;
                } else if constexpr (std::is_same_v<
                                         std::decay_t<decltype(x)>,
                                         Surface_mesh_vertex_const_iterator>) {
                    return x.second->point(*x.first);
                } else {
                    return x->point();
                }
            }, inner);
    }
};

void Polyhedron_hull_operation::evaluate()
{
    assert(!polyhedron);

    polyhedron = std::make_shared<Polyhedron>();

    iterator_chain<Polyhedron_points_iterator> c;

    for (Polyhedron_operation<Polyhedron> *p: polyhedra) {
        Polyhedron &P = *p->get_value();
        c.push_back(Polyhedron_points_iterator(P.vertices_begin()),
                    Polyhedron_points_iterator(P.vertices_end()));
    }

    for (Polyhedron_operation<Nef_polyhedron> *p: nef_polyhedra) {
        Nef_polyhedron &N = *p->get_value();
        c.push_back(Polyhedron_points_iterator(N.vertices_begin()),
                    Polyhedron_points_iterator(N.vertices_end()));
    }

    for (Polyhedron_operation<Surface_mesh> *p: meshes) {
        Surface_mesh &M = *p->get_value();
        Surface_mesh::Vertex_range r = M.vertices();
        c.push_back(Polyhedron_points_iterator(r.begin(), &M),
                    Polyhedron_points_iterator(r.end(), &M));
    }

    if (points.size() > 0) {
        c.push_back(Polyhedron_points_iterator(points.cbegin()),
                    Polyhedron_points_iterator(points.cend()));
    }

    CGAL::convex_hull_3(c.begin(), c.end(), *polyhedron);
}

///////////////////
// Minkowski sum //
///////////////////

void Polyhedron_minkowski_sum_operation::evaluate()
{
    assert(!polyhedron);

    polyhedron = std::make_shared<Nef_polyhedron>(
        CGAL::minkowski_sum_3(*first->get_value(), *second->get_value()));
}

//////////////////////
// Misc. operations //
//////////////////////

template<>
void Polyhedron_clip_operation<Nef_polyhedron>::evaluate()
{
    assert(!this->polyhedron);

    bool input_corefinable = true;
    const Nef_polyhedron &N = *this->operand->get_value();

    if (Flags::warn_nef) {
        Surface_mesh S;

        CGAL::convert_nef_polyhedron_to_polygon_mesh(N, S, true);
        if (CGAL::Polygon_mesh_processing::does_self_intersect(S)) {
            input_corefinable = false;
        }
    }

    this->polyhedron = std::make_shared<Nef_polyhedron>(
        N.intersection(
            plane, Nef_polyhedron::Intersection_mode::CLOSED_HALFSPACE));

    WARN_NEF(this, input_corefinable);
}

template<typename T>
void Polyhedron_clip_operation<T>::evaluate()
{
    assert(!this->polyhedron);

    this->polyhedron = std::make_shared<T>(*this->operand->get_value());

    CGAL::Polygon_mesh_processing::triangulate_faces(
        CGAL::faces(*this->polyhedron), *this->polyhedron);

    if (!CGAL::Polygon_mesh_processing::clip(
            *this->polyhedron, plane, CGAL::parameters::clip_volume(true))) {
        CGAL_error_msg("resulting mesh would not be manifold");
    }
}

template void Polyhedron_clip_operation<Polyhedron>::evaluate();
template void Polyhedron_clip_operation<Surface_mesh>::evaluate();

////////////////
// Primitives //
////////////////

void Tetrahedron_operation::evaluate()
{
    typedef Polyhedron::Point Point;

    assert(!polyhedron);

    polyhedron = std::make_shared<Polyhedron>();
    polyhedron->make_tetrahedron(
        Point(0, 0, 0),
        Point(0, b, 0),
        Point(a, 0, 0),
        Point(0, 0, c));

    if (a * b * c < 0) {
        polyhedron->inside_out();
    }
}

static Polyhedron::Halfedge_handle make_square_pyramid(
    Polyhedron &P, const FT &a_2, const FT &b_2, const FT &c)
{
    typedef Polyhedron::Point Point;

    auto h = P.make_tetrahedron(
        Point(0, 0, 0),
        Point(0, 0, c),
        Point(0, b_2, 0),
        Point(a_2, 0, 0));

    auto g = P.create_center_vertex(h);
    g->vertex()->point() = Point(-a_2, 0, 0);
    g->opposite()->vertex()->point() = Point(0, -b_2, 0);

    return P.join_facet(h);
}

void Square_pyramid_operation::evaluate()
{
    assert(!polyhedron);

    if (a <= 0 || b <= 0) {
        CGAL_error_msg("cannot make pyramid with non-positive side lengths");
    }

    polyhedron = std::make_shared<Polyhedron>();
    make_square_pyramid(*polyhedron, a / 2, b / 2, c);

    if (c < 0) {
        polyhedron->inside_out();
    }
}

void Octahedron_operation::evaluate()
{
    typedef Polyhedron::Point Point;

    if (a <= 0 || b <= 0) {
        CGAL_error_msg("cannot make octahedron with non-positive side lengths");
    }

    assert(!polyhedron);

    polyhedron = std::make_shared<Polyhedron>();

    auto h = make_square_pyramid(*polyhedron, a / 2, b / 2, c);
    polyhedron->create_center_vertex(h)->vertex()->point() = Point(0, 0, -d);

    if (c < 0) {
        polyhedron->inside_out();
    }
}

void Cuboid_operation::evaluate()
{
    typedef Polyhedron::Point Point;

    if (a <= 0 || b <= 0 || c <= 0) {
        CGAL_error_msg("cannot make cuboid with non-positive side lengths");
    }

    assert(!polyhedron);

    FT k = a / 2, l = b / 2 , m = c / 2;

    polyhedron = std::make_shared<Polyhedron>();

    auto h = polyhedron->make_tetrahedron(
        Point(k, -l, -m),
        Point(-k, -l, m),
        Point(-k, -l, -m),
        Point(-k, l, -m));

    auto g = h->next()->opposite()->next();
    polyhedron->split_edge(h->next());
    polyhedron->split_edge(g->next());
    polyhedron->split_edge(g);
    h->next()->vertex()->point() = Point(k, -l, m);
    g->next()->vertex()->point() = Point(-k, l, m);
    g->opposite()->vertex()->point() = Point(k, l, -m);

    auto f =
        polyhedron->split_facet(g->next(), g->next()->next()->next());

    auto e = polyhedron->split_edge(f);
    e->vertex()->point() = Point(k, l, m);
    polyhedron->split_facet(e, f->next()->next());
}

static void make_icosahedron(Polyhedron &P, const FT &r, const FT &tau)
{
    CGAL::make_icosahedron(P, Point_3(CGAL::ORIGIN), r);

    for (auto it = P.points_begin();
         it != P.points_end();
         it++) {
        *it = project_to_sphere(CGAL::to_double(it->x()),
                                CGAL::to_double(it->y()),
                                CGAL::to_double(it->z()),
                                r, tau);
    }
}

void Icosahedron_operation::evaluate()
{
    assert(!polyhedron);

    polyhedron = std::make_shared<Polyhedron>();
    make_icosahedron(*polyhedron, radius, tolerance);
}

static Polyhedron::Halfedge_handle make_regular_pyramid(
    Polyhedron &P, const int n, const FT &r, const FT &h, const FT &tau)
{
    typedef Polyhedron::Point Point;
    typedef Polyhedron::Halfedge_handle Halfedge_handle;

    Halfedge_handle g;

    const double delta = -2 * std::acos(-1) / n;
    for (auto [i, theta] = std::pair<int, double>(0, std::asin(1) + delta);
         i < n - 1;
         i++, theta += delta) {
        Point_2 A = project_to_circle(
            std::cos(theta), std::sin(theta), r, tau);

        if (i == 0) {
            g = P.make_triangle(
                Point(0, r, 0),
                Point(0, 0, h),
                Point(A.x(), A.y(), 0))->opposite();
        } else {
            g = P.add_vertex_and_facet_to_border(g, g->next());
            g->vertex()->point() = Point(A.x(), A.y(), 0);
            g = g->next()->opposite();
        }
    }

    P.add_facet_to_border(g, g->next()->next());

    return P.fill_hole(g);
}

void Regular_pyramid_operation::evaluate()
{
    assert(!polyhedron);

    if (sides < 3) {
        CGAL_error_msg("cannot make pyramid with less than three base sides");
    }

    if (radius <= 0) {
        CGAL_error_msg("cannot make pyramid with non-positive base radius");
    }

    polyhedron = std::make_shared<Polyhedron>();
    make_regular_pyramid(*polyhedron, sides, radius, height, tolerance);

    if (height < 0) {
        polyhedron->inside_out();
    }
}

void Regular_bipyramid_operation::evaluate()
{
    typedef Polyhedron::Point Point;

    assert(!polyhedron);

    if (sides < 3) {
        CGAL_error_msg("cannot make bipyramid with less than three base sides");
    }

    if (radius <= 0) {
        CGAL_error_msg("cannot make bipyramid with non-positive base radius");
    }

    polyhedron = std::make_shared<Polyhedron>();
    polyhedron->create_center_vertex(
        make_regular_pyramid(
            *polyhedron, sides, radius,
            heights[0], tolerance))->vertex()->point() =
        Point(0, 0, -heights[1]);

    if (heights[0] < 0) {
        polyhedron->inside_out();
    }
}

// Subdivided sphere

template <class T>
class Sphere_mask {
    using traits = typename boost::graph_traits<T>;
    typedef typename boost::property_map<T, CGAL::vertex_point_t>::type Vertex_point_map;
    typedef typename boost::property_traits<Vertex_point_map>::value_type Point;
    typedef typename boost::property_traits<Vertex_point_map>::reference Point_ref;

    T &mesh;
    Vertex_point_map map;
    const FT radius, tolerance;

public:
    Sphere_mask(T &M, const FT &r, const FT &tau):
        mesh(M), map(CGAL::get(CGAL::vertex_point, mesh)),
        radius(r), tolerance(tau) {}

    void edge_node(typename traits::halfedge_descriptor h, Point &P) {
        Point_ref A = boost::get(map, CGAL::target(h, mesh));
        Point_ref B = boost::get(
            map, CGAL::target(CGAL::opposite(h, mesh), mesh));

        P = project_to_sphere(CGAL::to_double((A[0] + B[0]) / 2),
                              CGAL::to_double((A[1] + B[1]) / 2),
                              CGAL::to_double((A[2] + B[2]) / 2),
                              radius, tolerance);
    }

    void vertex_node(typename traits::vertex_descriptor v, Point& P) {
        P = boost::get(map, v);
    }

    void border_node(typename traits::halfedge_descriptor, Point&, Point&) {
        assert_not_reached();
    }
};

void Sphere_operation::evaluate()
{
    assert(!polyhedron);

    polyhedron = std::make_shared<Polyhedron>();
    make_icosahedron(*polyhedron, radius, tolerances[1]);

    Sphere_mask<Polyhedron> mask(*polyhedron, radius, tolerances[1]);

    auto h = polyhedron->halfedges_begin();

    // The icosahedron consists of equilateral triangles, which are
    // split as follows:

    /////////////////////
    //                 //
    //        *        //
    //       / \       //
    //      / a \      //
    //     x.....x     //
    //    / .   . \    //
    //   / a . . a \   //
    //  *-----x-----*  //
    //                 //
    /////////////////////

    // This creates 4 new triangles.  The newly introduced vertices
    // (marked x above) are then displaced radially by

    //   s = r - sqrt(r^2 - l^2 / 4),

    //   Where r the radius,
    //         l the side length of the original triangle

    // All dashed sides are of equal length, as are all dotted sides
    // and the dotted sides are longer, with a length equal to:

    //   a = r / (r - s) * l / 2

    // (This is easy to see, as the value of a before displacing the
    // its endpoints onto the sphere, was just l / 2).  The largest
    // triangle produced is thus equilateral, of length a and the
    // largest triangulation error (taken here as the maximum distance
    // between the sphere and any edge) will be at the center-points of
    // the dotted edges, and given by the formula for s above.

    // We work with the squared half-length, q.

    const double r = CGAL::to_double(radius);
    double q = CGAL::to_double(
        CGAL::squared_distance(h->vertex()->point(),
                               h->opposite()->vertex()->point())) / 4;

    double s = r - std::sqrt(r * r - q);

    while (s > tolerances[0]) {
        CGAL::Subdivision_method_3::PTQ(
            *polyhedron, mask, CGAL::parameters::number_of_iterations(1));

        const double rs = r / (r - s);
        q = rs * rs * q / 4;
        s = r - std::sqrt(r * r - q);
    }
}
