#ifndef BOUNDING_VOLUMES_H
#define BOUNDING_VOLUMES_H

#include "projection.h"
#include "transformation_types.h"
#include "compose_tag.h"

class Bounding_volume
{
public:
    enum Mode {
        BOUNDARY,
        OPEN,
        CLOSED,
        UNSPECIFIED
    };

protected:
    Mode mode;

public:
    virtual std::string describe() const = 0;
    virtual bool contains(const Point_3 &p) const = 0;
    virtual std::shared_ptr<Bounding_volume> transform(
        const Aff_transformation_3 &T) const = 0;

    virtual bool get_bounds(FT (*v)[2]) const {
        return false;
    }

    std::shared_ptr<Bounding_volume> flush(
        const FT &lambda, const FT &mu, const FT &nu) const;

    Bounding_volume(const Mode m): mode(m) {}
};

template<typename T>
struct compose_tag_helper<std::shared_ptr<T>,
                          std::enable_if_t<
                              std::is_base_of_v<Bounding_volume, T>>> {
    static void compose(std::ostringstream &s, const std::shared_ptr<T> &x) {
        if (x) {
            s << x->describe() << ",";
        }
    }
};

// Primitives

class Bounding_halfspace: public Bounding_volume
{
    const Plane_3 plane;

public:
    Bounding_halfspace(const Plane_3 &pi, const Mode m):
        Bounding_volume(m), plane(pi) {}

    std::string describe() const {
        const char *s = (
            mode == CLOSED
            ? "bounding_halfspace"
            : (mode == OPEN
               ? "bounding_halfspace_interior"
               : "bounding_plane"));

        return compose_tag(s, plane);
    }

    bool contains(const Point_3 &p) const override;

    std::shared_ptr<Bounding_volume> transform(
        const Aff_transformation_3 &T) const override;
};

class Bounding_box: public Bounding_volume
{
    Plane_3 planes[6];

public:
    Bounding_box(const FT &a, const FT &b, const FT &c, const Mode m):
        Bounding_volume(m), planes{Plane_3(-1, 0, 0, a / 2),
                                   Plane_3(1, 0, 0, a / 2),
                                   Plane_3(0, -1, 0, b / 2),
                                   Plane_3(0, 1, 0, b / 2),
                                   Plane_3(0, 0, -1, c / 2),
                                   Plane_3(0, 0, 1, c / 2)} {}

    std::string describe() const {
        const char *s = (
            mode == CLOSED
            ? "bounding_box"
            : (mode == OPEN
               ? "bounding_box_interior"
               : "bounding_box_boundary"));

        return compose_tag(s, planes);
    }

    bool contains(const Point_3 &p) const override;

    std::shared_ptr<Bounding_volume> transform(
        const Aff_transformation_3 &T) const override;

    bool get_bounds(FT (*v)[2]) const override;
};

class Bounding_sphere: public Bounding_volume
{
    const Sphere_3 sphere;

public:
    Bounding_sphere(const FT &r, const Mode m):
        Bounding_volume(m), sphere(Point_3(CGAL::ORIGIN), r * r) {}

    Bounding_sphere(const Sphere_3 &s, const Mode m):
        Bounding_volume(m), sphere(s) {}

    std::string describe() const {
        const char *s = (
            mode == CLOSED
            ? "bounding_sphere"
            : (mode == OPEN
               ? "bounding_sphere_interior"
               : "bounding_sphere_boundary"));

        return compose_tag(
            s, sphere.center(), rational_sqrt(sphere.squared_radius()));
    }

    bool contains(const Point_3 &p) const override;

    std::shared_ptr<Bounding_volume> transform(
        const Aff_transformation_3 &T) const override;

    bool get_bounds(FT (*v)[2]) const override;
};

class Bounding_cylinder: public Bounding_volume
{
    const Point_3 endpoint;
    const Vector_3 axis;
    const FT radius, height;

public:
    Bounding_cylinder(const FT &r, const FT &h, const Mode m):
        Bounding_volume(m),
        endpoint(0, 0, h / -2), axis(0, 0, 1), radius(r), height(h) {}

    Bounding_cylinder(
        const FT &r, const FT &h, const Point_3 &a, const Vector_3 &v,
        const Mode m):
        Bounding_volume(m), endpoint(a), axis(v), radius(r), height(h) {}

    std::string describe() const {
        const char *s = (
            mode == CLOSED
            ? "bounding_cylinder"
            : (mode == OPEN
               ? "bounding_cylinder_interior"
               : "bounding_cylinder_boundary"));

        return compose_tag(s, endpoint, axis, radius, height);
    }

    bool contains(const Point_3 &p) const override;

    std::shared_ptr<Bounding_volume> transform(
        const Aff_transformation_3 &T) const override;

    bool get_bounds(FT (*v)[2]) const override;
};

// Set operations

class Bounding_volume_complement: public Bounding_volume
{
    std::shared_ptr<Bounding_volume> volume;

public:
    Bounding_volume_complement(const std::shared_ptr<Bounding_volume> &p):
        Bounding_volume(UNSPECIFIED), volume(p) {}

    std::string describe() const {
        return compose_tag("complement", volume);
    }

    bool contains(const Point_3 &p) const override {
        return !volume->contains(p);
    }

    std::shared_ptr<Bounding_volume> transform(
        const Aff_transformation_3 &T) const override {
        return std::make_shared<Bounding_volume_complement>(
            volume->transform(T));
    }
};

class Bounding_volume_union: public Bounding_volume
{
    std::vector<std::shared_ptr<Bounding_volume>> volumes;

public:
    Bounding_volume_union(
        std::vector<std::shared_ptr<Bounding_volume>> &&v):
        Bounding_volume(UNSPECIFIED), volumes(std::move(v)) {}

    std::string describe() const {
        return compose_tag("join", volumes);
    }

    bool contains(const Point_3 &p) const override;

    std::shared_ptr<Bounding_volume> transform(
        const Aff_transformation_3 &T) const override;

    bool get_bounds(FT (*v)[2]) const override;
};

class Bounding_volume_intersection: public Bounding_volume
{
    std::vector<std::shared_ptr<Bounding_volume>> volumes;

public:
    Bounding_volume_intersection(
        std::vector<std::shared_ptr<Bounding_volume>> &&v):
        Bounding_volume(UNSPECIFIED), volumes(std::move(v)) {}

    std::string describe() const {
        return compose_tag("intersection", volumes);
    }

    bool contains(const Point_3 &p) const override;

    std::shared_ptr<Bounding_volume> transform(
        const Aff_transformation_3 &T) const override;

    bool get_bounds(FT (*v)[2]) const override;
};

class Bounding_volume_difference: public Bounding_volume
{
    std::vector<std::shared_ptr<Bounding_volume>> volumes;

public:
    Bounding_volume_difference(
        std::vector<std::shared_ptr<Bounding_volume>> &&v):
        Bounding_volume(UNSPECIFIED), volumes(std::move(v)) {}

    std::string describe() const {
        return compose_tag("difference", volumes);
    }

    bool contains(const Point_3 &p) const override;

    std::shared_ptr<Bounding_volume> transform(
        const Aff_transformation_3 &T) const override;

    bool get_bounds(FT (*v)[2]) const override;
};

#endif
