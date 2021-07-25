#ifndef CONIC_POLYGON_TESTS_H
#define CONIC_POLYGON_TESTS_H

static inline std::shared_ptr<
    Polygon_operation<Conic_polygon_set>> TRANSFORM_C(
    std::shared_ptr<Polygon_operation<Circle_polygon_set>> p,
    const Aff_transformation_2 &T)
{
    auto x = std::dynamic_pointer_cast<Polygon_operation<Conic_polygon_set>>(
        TRANSFORM(p, T));

    BOOST_TEST(x);
    return x;
}

bool test_polygon_without_holes(const Conic_polygon &P,
                                const std::string_view &edges);

#endif
