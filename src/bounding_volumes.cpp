#include "kernel.h"
#include "projection.h"
#include "transformation_types.h"
#include "bounding_volumes.h"

#define UPDATE_BOUNDS(v, A, p) {                \
    for (int i = 0; i < 3; i++) {               \
        if (p || A[i] < v[i][0]) {              \
            v[i][0] = A[i];                     \
        }                                       \
                                                \
        if (p || A[i] > v[i][1]) {              \
            v[i][1] = A[i];                     \
        }                                       \
    }                                           \
                                                \
    p = false;                                  \
}

std::shared_ptr<Bounding_volume> Bounding_volume::flush(
    const FT &lambda, const FT &mu, const FT &nu) const
{
    FT v[3][2];

    if (!get_bounds(v)) {
        return nullptr;
    }

    return transform(
        Aff_transformation_3(1, 0, 0, (CGAL::min(lambda, FT(0)) * v[0][0]
                                       - CGAL::max(lambda, FT(0)) * v[0][1]),
                             0, 1, 0, (CGAL::min(mu, FT(0)) * v[1][0]
                                       - CGAL::max(mu, FT(0)) * v[1][1]),
                             0, 0, 1, (CGAL::min(nu, FT(0)) * v[2][0]
                                       - CGAL::max(nu, FT(0)) * v[2][1])));
}

// Primitives

bool Bounding_halfspace::contains(const Point_3 &p) const
{
    if (mode == CLOSED) {
        return !plane.has_on_positive_side(p);
    } else if (mode == OPEN) {
        return plane.has_on_negative_side(p);
    } else {
        return plane.has_on(p);
    }
}

std::shared_ptr<Bounding_volume> Bounding_halfspace::transform(
    const Aff_transformation_3 &T) const
{
    return std::make_shared<Bounding_halfspace>(plane.transform(T), mode);
}

bool Bounding_box::contains(const Point_3 &p) const
{
    bool q = false;

    for (const Plane_3 &pi: planes) {
        if (mode == CLOSED) {
            if (pi.has_on_negative_side(p)) {
                return false;
            }
        } else if (mode == OPEN) {
            if (!pi.has_on_positive_side(p)) {
                return false;
            }
        } else {
            if (pi.has_on_negative_side(p)) {
                return false;
            }

            q = q || pi.has_on(p);
        }
    }

    return (mode != BOUNDARY) || q;
}

std::shared_ptr<Bounding_volume> Bounding_box::transform(
    const Aff_transformation_3 &T) const
{
    std::shared_ptr<Bounding_box> p = std::make_shared<Bounding_box>(*this);

    for (int i = 0; i < 6; i++) {
        p->planes[i] = p->planes[i].transform(T);
    }

    return p;
}

bool Bounding_box::get_bounds(FT (*v)[2]) const
{
    bool p = true;

    // Calculate box vertices by intersecting triplets of bounding
    // planes.

    for (int i = 0; i < 2; i++) {
        for (int j = 2; j < 4; j++) {
            for (int k = 4; k < 6; k++) {
                const auto x = CGAL::intersection(planes[i], planes[j]);

                const Line_3* l = boost::get<Line_3 >(&*x);
                assert(l);

                const auto y = CGAL::intersection(*l, planes[k]);
                const Point_3 *a = boost::get<Point_3 >(&*y);
                assert(a);

                const Point_3 &A = *a;

                UPDATE_BOUNDS(v, A, p);
            }
        }
    }

    return true;
}

bool Bounding_sphere::contains(const Point_3 &p) const
{
    if (mode == CLOSED) {
        return !sphere.has_on_unbounded_side(p);
    } else if (mode == OPEN) {
        return sphere.has_on_bounded_side(p);
    } else {
        return sphere.has_on(p);
    }
}

std::shared_ptr<Bounding_volume> Bounding_sphere::transform(
    const Aff_transformation_3 &T) const
{
    return std::make_shared<Bounding_sphere>(
        sphere.orthogonal_transform(T), mode);
}

bool Bounding_sphere::get_bounds(FT (*v)[2]) const
{
    const Point_3 C = sphere.center();
    const FT r = rational_sqrt(sphere.squared_radius());

    for (int i = 0; i < 3; i++) {
        v[i][0] = C[i] - r;
        v[i][1] = C[i] + r;
    }

    return true;
}

bool Bounding_cylinder::contains(const Point_3 &p) const
{
    const Vector_3 v(endpoint, p);
    const FT c = axis.squared_length();
    const FT d = v * axis;

    const FT k = c * height;
    const FT l = c * v * v - d * d;
    const FT m = c * c * radius * radius;

    if (mode == CLOSED) {
        return (d >= 0 && d <= k && l <= m);
    } else if (mode == OPEN) {
        return (d > 0 && d < k && l < m);
    } else {
        return (d >= 0 && d <= k && l <= m
                && !(d > 0 && d < k && l < m));
    }
}

std::shared_ptr<Bounding_volume> Bounding_cylinder::transform(
    const Aff_transformation_3 &T) const
{
    return std::make_shared<Bounding_cylinder>(
        radius, height,
        T.transform(endpoint), T.transform(axis),
        mode);
}

bool Bounding_cylinder::get_bounds(FT (*v)[2]) const
{
    const FT r = radius * rational_sqrt(axis.squared_length());
    bool p = false;

    // We only support flushing axis-aligned boudning cylinders, as
    // the general case cannot be implemented with rational
    // arithmetic.

    for (int i = 0; i < 3; i++) {
        if (axis[i] == 0) {
            continue;
        }

        if (p) {
            return false;
        }

        p = true;

        const FT a = endpoint[i];
        const FT b = endpoint[i] + axis[i] * height;

        if (a < b) {
            v[i][0] = a;
            v[i][1] = b;
        } else {
            v[i][0] = b;
            v[i][1] = a;
        }

        for (int j = 1; j < 3; j++) {
            const int k = (i + j) % 3;
            v[k][0] = endpoint[k] - r;
            v[k][1] = endpoint[k] + r;
        }
    }

    return true;
}

// Set operations

bool Bounding_volume_union::contains(const Point_3 &p) const
{
    for (std::size_t i = 0; i < volumes.size(); i++) {
        if (volumes[i]->contains(p)) {
            return true;
        }
    }

    return false;
}

std::shared_ptr<Bounding_volume> Bounding_volume_union::transform(
    const Aff_transformation_3 &T) const
{
    std::vector<std::shared_ptr<Bounding_volume>> v;
    v.reserve(volumes.size());

    for (const auto &x: volumes) {
        v.push_back(x->transform(T));
    }

    return std::make_shared<Bounding_volume_union>(std::move(v));
}

bool Bounding_volume_union::get_bounds(FT (*v)[2]) const
{
    if (!volumes[0]->get_bounds(v)) {
        return false;
    }

    for (std::size_t i = 1; i < volumes.size(); i++) {
        FT w[3][2];

        if (!volumes[i]->get_bounds(w)) {
            return false;
        }

        for (int j = 0; j < 3; j++) {
            if (w[j][0] < v[j][0]) {
                v[j][0] = w[j][0];
            }

            if (w[j][1] > v[j][1]) {
                v[j][1] = w[j][1];
            }
        }
    }

    return true;
}

bool Bounding_volume_intersection::contains(const Point_3 &p) const
{
    for (std::size_t i = 0; i < volumes.size(); i++) {
        if (!volumes[i]->contains(p)) {
            return false;
        }
    }

    return true;
}

std::shared_ptr<Bounding_volume> Bounding_volume_intersection::transform(
    const Aff_transformation_3 &T) const
{
    std::vector<std::shared_ptr<Bounding_volume>> v;
    v.reserve(volumes.size());

    for (const auto &x: volumes) {
        v.push_back(x->transform(T));
    }

    return std::make_shared<Bounding_volume_intersection>(std::move(v));
}

bool Bounding_volume_intersection::get_bounds(FT (*v)[2]) const
{
    if (!volumes[0]->get_bounds(v)) {
        return false;
    }

    for (std::size_t i = 1; i < volumes.size(); i++) {
        FT w[3][2];

        if (!volumes[i]->get_bounds(w)) {
            return false;
        }

        for (int j = 0; j < 3; j++) {
            if (w[j][0] > v[j][0]) {
                v[j][0] = w[j][0];
            }

            if (w[j][1] < v[j][1]) {
                v[j][1] = w[j][1];
            }
        }
    }

    return true;
}

bool Bounding_volume_difference::contains(const Point_3 &p) const
{
    for (std::size_t i = 0; i < volumes.size(); i++) {
        if (volumes[i]->contains(p) != (i == 0)) {
            return false;
        }
    }

    return true;
}

std::shared_ptr<Bounding_volume> Bounding_volume_difference::transform(
    const Aff_transformation_3 &T) const
{
    std::vector<std::shared_ptr<Bounding_volume>> v;
    v.reserve(volumes.size());

    for (const auto &x: volumes) {
        v.push_back(x->transform(T));
    }

    return std::make_shared<Bounding_volume_difference>(std::move(v));
}

#include "polyhedron_types.h"

bool Bounding_volume_difference::get_bounds(FT (*v)[2]) const
{
    Nef_polyhedron N;

    for (std::size_t i = 0; i < volumes.size(); i++) {
        FT w[3][2];

        if (!volumes[i]->get_bounds(w)) {
            return false;
        }

        std::vector<Point_3> u;
        u.reserve(8);

        for (int l = 0; l < 2; l++) {
            for (int k = 0; k < 2; k++) {
                for (int j = 0; j < 2; j++) {
                    u.emplace_back(w[0][j], w[1][k], w[2][l]);
                }
            }
        }

        Polyhedron P;
        CGAL::make_hexahedron(u[0], u[1], u[3], u[2],
                              u[6], u[4], u[5], u[7], P);

        if (i == 0) {
            N = Nef_polyhedron(P);
        } else {
            N -= Nef_polyhedron(P);
        }
    }

    bool p = true;

    for (auto u = N.vertices_begin(); u != N.vertices_end(); ++u) {
        const Point_3 &A = u->point();

        UPDATE_BOUNDS(v, A, p);
    }

    return true;
}

#undef UPDATE_BOUNDS
