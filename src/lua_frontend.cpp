#include <filesystem>

#include <lualib.h>
#include <lauxlib.h>

#include "assertions.h"
#include "options.h"
#include "kernel.h"
#include "transformations.h"
#include "macros.h"
#include "boxed_operations.h"
#include "frontend.h"

template<typename T>
static const char *type_name;

FT checkrational(lua_State *L, int i)
{
    if (lua_isnumber(L, i)) {
        return FT(lua_tonumber(L, i));
    } else if (lua_isstring(L, i)) {
        try {
            return FT(FT::ET(lua_tostring(L, i)));
        } catch (const std::invalid_argument &e) {
            luaL_argerror(L, i, "could not convert argument to number");
            assert_not_reached();
        }
    } else {
        luaL_argerror(L, i, "expected number or string");
        assert_not_reached();
    }
}

//////////////
// Settings //
//////////////

template<FT &T>
static int set_tolerance(lua_State *L)
{
    lua_pushnumber(L, CGAL::to_double(T));

    if (!lua_isnoneornil(L, 1)) {
        T = checkrational(L, 1);
    }

    return 1;
}

////////////////
// Operations //
////////////////

template<typename T, typename... Args>
static int tolua(lua_State *L, Args &&... args)
{
    if constexpr (std::is_same_v<T, FT> || std::is_same_v<T, double>) {
        lua_pushnumber(L, CGAL::to_double(std::forward<Args>(args)...));
    } else {
        *static_cast<void **>(lua_newuserdata(L, sizeof(void *))) =
            static_cast<void *>(new T(std::forward<Args>(args)...));

        luaL_setmetatable(L, type_name<T>);
    }

    return 1;
}

template<typename T>
static const T &fromlua(lua_State *L, int i)
{
    return *static_cast<T *>(
        *static_cast<void **>(
            luaL_checkudata(L, i, type_name<T>)));
}

template<typename T>
static int collect(lua_State *L)
{
    delete static_cast<T *>(*static_cast<void **>(lua_touserdata(L, 1)));

    return 0;
}

template<typename T>
static int tostring(lua_State *L)
{
    const T &X = fromlua<T>(L, 1);

    std::ostringstream s;

    if constexpr (std::is_same_v<T, Boxed_polygon>
                  || std::is_same_v<T, Boxed_polyhedron>) {
        std::visit(
            [&s](auto &&x) {
                compose_tag_helper<std::decay_t<decltype(x)>>::compose(s, x);
            },
            fromlua<T>(L, 1));
    } else {
        compose_tag_helper<T>::compose(s, X);
    }

    std::string t = s.str();
    t.pop_back();

    lua_pushstring(L, t.c_str());

    return 1;
}

///////////////////////
// Geometric objects //
///////////////////////

static int point(lua_State *L)
{
    const FT x = checkrational(L, 1);
    const FT y = checkrational(L, 2);

    if (lua_gettop(L) > 2) {
        const FT z = checkrational(L, 3);

        tolua<Point_3>(L, Point_3(x, y, z));
    } else {
        tolua<Point_2>(L, Point_2(x, y));
    }

    return 1;
}

static int plane(lua_State *L)
{
    const FT a = checkrational(L, 1);
    const FT b = checkrational(L, 2);
    const FT c = checkrational(L, 3);
    const FT d = checkrational(L, 4);

    tolua<Plane_3>(L, Plane_3(a, b, c, d));

    return 1;
}

/////////////////////
// Transformations //
/////////////////////

static int translation(lua_State *L)
{
    const FT x = checkrational(L, 1);
    const FT y = checkrational(L, 2);

    if (lua_gettop(L) > 2) {
        const FT z = checkrational(L, 3);

        tolua<Aff_transformation_3>(L, TRANSLATION_3(x, y, z));
    } else {
        tolua<Aff_transformation_2>(L, TRANSLATION_2(x, y));
    }

    return 1;
}

static int rotation(lua_State *L)
{
    double theta = luaL_checknumber(L, 1);

    switch (lua_gettop(L)) {
        case 1:
            tolua<Aff_transformation_2>(L, basic_rotation(theta));
            break;

        case 2:
            if (lua_isinteger(L, 2)) {
                const int i = lua_tointeger(L, 2);

                if (i < 0 || i > 2) {
                    luaL_argerror(L, 2, "expected 0, 1, or 2");
                }

                tolua<Aff_transformation_3>(L, basic_rotation(theta, i));
            } else if (lua_istable(L, 2)) {
                double v[3];

                for (int i = 0; i < 3 ; i++) {
                    lua_geti(L, 2, i + 1);
                    v[i] = lua_tonumber(L, -1);
                }

                lua_pop(L, 3);

                tolua<Aff_transformation_3>(L, axis_angle_rotation(theta, v));
            } else {
                luaL_argerror(L, 2, "expected integer or table");
            }
            break;
    }

    return 1;
}

static int scaling(lua_State *L)
{
    const FT x = checkrational(L, 1);
    const FT y = checkrational(L, 2);

    if (lua_gettop(L) > 2) {
        const FT z = checkrational(L, 3);

        tolua<Aff_transformation_3>(L, SCALING_3(x, y, z));
    } else {
        tolua<Aff_transformation_2>(L, SCALING_2(x, y));
    }

    return 1;
}

static int transformation_2_mul(lua_State *L)
{
    const Aff_transformation_2 &A =
        fromlua<Aff_transformation_2>(L, 1);

    if (luaL_testudata(L, 2, "transformation_2d")) {
        const Aff_transformation_2 &B =
            fromlua<Aff_transformation_2>(L, 2);
        tolua<Aff_transformation_2>(L, A * B);
    } else if (luaL_testudata(L, 2, "point_2d")) {
        const Point_2 &B = fromlua<Point_2>(L, 2);
        tolua<Point_2>(L, A.transform(B));
    } else if (luaL_testudata(L, 2, "polygon")) {
        std::visit(
            [&L, &A](auto &&x) {
                auto p = TRANSFORM(x, A);

                // Circle polygons may transform into circle or conic
                // polygons, so in that case, we have to determine the
                // result type at runtime.

                if constexpr (
                    std::is_same_v<decltype(p), std::shared_ptr<Operation>>) {
                    tolua<Boxed_polygon>(L, std::move(make_boxed_polygon(p)));
                } else {
                    tolua<Boxed_polygon>(L, p);
                }
            },
            fromlua<Boxed_polygon>(L, 2));
    } else {
        std::ostringstream s;
        s << "attempt to transform " << luaL_typename(L, 2) << " value";
        std::string t = s.str();

        return luaL_argerror(L, 2, t.c_str());
    }

    return 1;
}

static int transformation_3_mul(lua_State *L)
{
    const Aff_transformation_3 &A =
        fromlua<Aff_transformation_3>(L, 1);

    if (luaL_testudata(L, 2, "transformation_3d")) {
        const Aff_transformation_3 &B =
            fromlua<Aff_transformation_3>(L, 2);
        tolua<Aff_transformation_3>(L, A * B);
    } else if (luaL_testudata(L, 2, "point_3d")) {
        const Point_3 &B = fromlua<Point_3>(L, 2);
        tolua<Point_3>(L, A.transform(B));
    } else if (luaL_testudata(L, 2, "bounding_volume")) {
        tolua<std::shared_ptr<Bounding_volume>>(
            L, TRANSFORM(fromlua<std::shared_ptr<Bounding_volume>>(L, 2), A));
    } else if (luaL_testudata(L, 2, "polyhedron")) {
        std::visit(
            [&L, &A](auto &&x) {
                tolua<Boxed_polyhedron>(L, TRANSFORM(x, A));
            },
            fromlua<Boxed_polyhedron>(L, 2));
    } else if (luaL_testudata(L, 2, "polygon")) {
        std::visit(
            [&L, &A](auto &&x) {
                tolua<Boxed_polyhedron>(L, TRANSFORM(x, A));
            },
            fromlua<Boxed_polygon>(L, 2));
    } else {
        std::ostringstream s;
        s << "attempt to transform " << luaL_typename(L, 2) << " value";
        std::string t = s.str();

        return luaL_argerror(L, 2, t.c_str());
    }

    return 1;
}

static int transformation_apply(lua_State *L)
{
    if (luaL_testudata(L, 1, "transformation_2d")) {
        transformation_2_mul(L);
    } else if (luaL_testudata(L, 1, "transformation_3d")) {
        transformation_3_mul(L);
    } else {
        std::ostringstream s;
        s << "attempt to apply a " << luaL_typename(L, 1)
          << " value as transformation";
        std::string t = s.str();

        return luaL_argerror(L, 1, t.c_str());
    }

    return 1;
}

///////////////
// Selection //
///////////////

#define DEFINE_BOUNDING_VOLUME_SET_METAMETHOD(NAME, OP)                 \
static int bounding_volume_ ##NAME(lua_State *L)                        \
{                                                                       \
    return tolua<std::shared_ptr<Bounding_volume>>(                     \
        L, OP({fromlua<std::shared_ptr<Bounding_volume>>(L, 1),         \
               fromlua<std::shared_ptr<Bounding_volume>>(L, 2)}));      \
}

DEFINE_BOUNDING_VOLUME_SET_METAMETHOD(add, JOIN)
DEFINE_BOUNDING_VOLUME_SET_METAMETHOD(sub, DIFFERENCE)
DEFINE_BOUNDING_VOLUME_SET_METAMETHOD(mul, INTERSECTION)

#undef DEFINE_BOUNDING_VOLUME_SET_METAMETHOD

static int bounding_volume_bnot(lua_State *L)
{
    return tolua<std::shared_ptr<Bounding_volume>>(
        L, COMPLEMENT(fromlua<std::shared_ptr<Bounding_volume>>(L, 1)));
}

#define DEFINE_SELECTOR_SET_METAMETHOD(NAME, OP)                \
template<typename T>                                            \
static int selector_ ##NAME(lua_State *L)                       \
{                                                               \
    return tolua<std::shared_ptr<T>>(                           \
        L, OP({fromlua<std::shared_ptr<T>>(L, 1),               \
                fromlua<std::shared_ptr<T>>(L, 2)}));           \
}

DEFINE_SELECTOR_SET_METAMETHOD(add, JOIN)
DEFINE_SELECTOR_SET_METAMETHOD(sub, DIFFERENCE)
DEFINE_SELECTOR_SET_METAMETHOD(mul, INTERSECTION)

#undef DEFINE_SELECTOR_SET_METAMETHOD

template<typename T>
static int selector_bnot(lua_State *L)
{
    return tolua<std::shared_ptr<T>>(
        L, COMPLEMENT(fromlua<std::shared_ptr<T>>(L, 1)));
}

template<int I>
static int relative_selection(lua_State *L)
{
    const int n = I * luaL_checkinteger(L, 2);

    if (luaL_testudata(L, 1, "face_selector")) {
        tolua<std::shared_ptr<Face_selector>>(
            L, RELATIVE_SELECTION(
                fromlua<std::shared_ptr<Face_selector>>(L, 1), n));
    } else if (luaL_testudata(L, 1, "vertex_selector")) {
        tolua<std::shared_ptr<Vertex_selector>>(
            L, RELATIVE_SELECTION(
                fromlua<std::shared_ptr<Vertex_selector>>(L, 1), n));
    } else {
        luaL_argerror(L, 1, "invalid type, expected selector");
    }

    return 1;
}

static int vertices_in(lua_State *L)
{
    if (luaL_testudata(L, 1, "face_selector")) {
        tolua<std::shared_ptr<Vertex_selector>>(
            L, VERTICES_IN(fromlua<std::shared_ptr<Face_selector>>(L, 1)));
    } else if (luaL_testudata(L, 1, "edge_selector")) {
        tolua<std::shared_ptr<Vertex_selector>>(
            L, VERTICES_IN(fromlua<std::shared_ptr<Edge_selector>>(L, 1)));
    } else if (luaL_testudata(L, 1, "bounding_volume")) {
        tolua<std::shared_ptr<Vertex_selector>>(
            L, VERTICES_IN(fromlua<std::shared_ptr<Bounding_volume>>(L, 1)));
    } else {
        luaL_argerror(
            L, 1, "invalid type, expected selector or bounding voume");
    }

    return 1;
}

static int faces_in(lua_State *L)
{
    if (luaL_testudata(L, 1, "vertex_selector")) {
        tolua<std::shared_ptr<Face_selector>>(
            L, FACES_IN(fromlua<std::shared_ptr<Vertex_selector>>(L, 1)));
    } else if (luaL_testudata(L, 1, "edge_selector")) {
        tolua<std::shared_ptr<Face_selector>>(
            L, FACES_IN(fromlua<std::shared_ptr<Edge_selector>>(L, 1)));
    } else if (luaL_testudata(L, 1, "bounding_volume")) {
        tolua<std::shared_ptr<Face_selector>>(
            L, FACES_IN(fromlua<std::shared_ptr<Bounding_volume>>(L, 1)));
    } else {
        luaL_argerror(
            L, 1, "invalid type, expected selector or bounding voume");
    }

    return 1;
}

static int faces_partially_in(lua_State *L)
{
    if (luaL_testudata(L, 1, "vertex_selector")) {
        tolua<std::shared_ptr<Face_selector>>(
            L, FACES_PARTIALLY_IN(
                fromlua<std::shared_ptr<Vertex_selector>>(L, 1)));
    } else if (luaL_testudata(L, 1, "edge_selector")) {
        tolua<std::shared_ptr<Face_selector>>(
            L, FACES_PARTIALLY_IN(
                fromlua<std::shared_ptr<Edge_selector>>(L, 1)));
    } else if (luaL_testudata(L, 1, "bounding_volume")) {
        tolua<std::shared_ptr<Face_selector>>(
            L, FACES_PARTIALLY_IN(
                fromlua<std::shared_ptr<Bounding_volume>>(L, 1)));
    } else {
        luaL_argerror(
            L, 1, "invalid type, expected selector or bounding voume");
    }

    return 1;
}

static int edges_in(lua_State *L)
{
    if (luaL_testudata(L, 1, "vertex_selector")) {
        tolua<std::shared_ptr<Edge_selector>>(
            L, EDGES_IN(fromlua<std::shared_ptr<Vertex_selector>>(L, 1)));
    } else if (luaL_testudata(L, 1, "face_selector")) {
        tolua<std::shared_ptr<Edge_selector>>(
            L, EDGES_IN(fromlua<std::shared_ptr<Face_selector>>(L, 1)));
    } else if (luaL_testudata(L, 1, "bounding_volume")) {
        tolua<std::shared_ptr<Edge_selector>>(
            L, EDGES_IN(fromlua<std::shared_ptr<Bounding_volume>>(L, 1)));
    } else {
        luaL_argerror(
            L, 1, "invalid type, expected selector or bounding voume");
    }

    return 1;
}

static int edges_partially_in(lua_State *L)
{
    if (luaL_testudata(L, 1, "vertex_selector")) {
        tolua<std::shared_ptr<Edge_selector>>(
            L, EDGES_PARTIALLY_IN(
                fromlua<std::shared_ptr<Vertex_selector>>(L, 1)));
    } else if (luaL_testudata(L, 1, "face_selector")) {
        tolua<std::shared_ptr<Edge_selector>>(
            L, EDGES_PARTIALLY_IN(
                fromlua<std::shared_ptr<Face_selector>>(L, 1)));
    } else if (luaL_testudata(L, 1, "bounding_volume")) {
        tolua<std::shared_ptr<Edge_selector>>(
            L, EDGES_PARTIALLY_IN(
                fromlua<std::shared_ptr<Bounding_volume>>(L, 1)));
    } else {
        luaL_argerror(
            L, 1, "invalid type, expected selector or bounding voume");
    }

    return 1;
}

// Generic operation taking any number of FT arguments.

template<int I, typename A, typename T, typename... Types>
static void primitive_args(lua_State *L, A &t)
{
    if constexpr(std::is_same_v<T, int>) {
        std::get<I>(t) = luaL_checkinteger(L, I + 1);
    } else if constexpr(std::is_same_v<T, FT>) {
        std::get<I>(t) = checkrational(L, I + 1);
    } else {
        std::get<I>(t) = fromlua<T>(L, I + 1);
    }

    if constexpr (sizeof...(Types) > 0) {
        primitive_args<I + 1, A, Types...>(L, t);
    }
}

template<auto F, typename R, typename... Types>
static int primitive(lua_State *L)
{
    std::tuple<Types...> t;

    primitive_args<0, decltype(t), Types...>(L, t);

    return tolua<R>(L, std::apply(F, t));
}

template <std::size_t>
using FT_alias = FT;

template<auto F, typename R, std::size_t... Is>
static inline int primitive_helper(lua_State *L, std::index_sequence<Is...>)
{
    return primitive<F, R, FT_alias<Is>...>(L);
}

template<auto F, typename R, std::size_t N>
static inline int primitive(lua_State *L)
{
    return primitive_helper<F, R>(L, std::make_index_sequence<N>{});
}

template <Operation::Message_level M>
static int print(lua_State *L)
{
    const int n = lua_gettop(L);
    for (int i = 1; i <= n; i++) {
        std::size_t m;
        const char *s = luaL_tolstring(L, i, &m);
        print_message(M, s, m);
    }

    return 0;
}

//////////////
// Polygons //
//////////////

static int ngon(lua_State *L)
{
    std::vector<Point_2> v;
    const int h = lua_gettop(L);

    for (int i = 1; i <= h; i++) {
        v.push_back(fromlua<Point_2>(L, i));
    }

    tolua<Boxed_polygon>(L, POLYGON(std::move(v)));

    return 1;
}

static int octahedron(lua_State *L)
{
    if (lua_isnoneornil(L, 4)) {
        tolua<Boxed_polyhedron>(
            L, OCTAHEDRON(checkrational(L, 1),
                          checkrational(L, 2),
                          checkrational(L, 3)));
    } else {
        tolua<Boxed_polyhedron>(
            L, OCTAHEDRON(checkrational(L, 1),
                          checkrational(L, 2),
                          checkrational(L, 3),
                          checkrational(L, 4)));
    }

    return 1;
}

static int regular_bipyramid(lua_State *L)
{
    if (lua_isnoneornil(L, 4)) {
        tolua<Boxed_polyhedron>(
            L, REGULAR_BIPYRAMID(luaL_checkinteger(L, 1),
                                 checkrational(L, 2),
                                 checkrational(L, 3)));
    } else {
        tolua<Boxed_polyhedron>(
            L, REGULAR_BIPYRAMID(luaL_checkinteger(L, 1),
                                 checkrational(L, 2),
                                 checkrational(L, 3),
                                 checkrational(L, 4)));
    }

    return 1;
}

#define DEFINE_POLYHEDRON_SET_METAMETHOD(NAME)  \
static int polyhedron_ ##NAME(lua_State *L)     \
{

// Clip when one of the operands is a plane. (The Nef option branch
// can't be incorporated in the macros, since it's made at run-time.)

#define MAYBE_CLIP(MAYBE_FLIP) {                                        \
    int i;                                                              \
    for (i = 0; i < 2 && !luaL_testudata(L, i + 1, "plane"); i++);      \
                                                                        \
    if (i < 2) {                                                        \
        const Plane_3 &Pi = fromlua<Plane_3>(L, i + 1)MAYBE_FLIP;       \
                                                                        \
        tolua<Boxed_polyhedron>(                                        \
            L,                                                          \
            std::visit(                                                 \
                PERFORM_POLYHEDRON_CLIP_OPERATION(Pi),                  \
                fromlua<Boxed_polyhedron>(L, 2 - i)));                  \
                                                                        \
        return 1;                                                       \
    }                                                                   \
}

// Perform the usual set operation otherwise.

#define SET_OPERATION(OP)                                               \
    tolua<Boxed_polyhedron>(                                            \
        L,                                                              \
        std::visit(                                                     \
            PERFORM_POLYHEDRON_SET_OPERATION(OP),                       \
            fromlua<Boxed_polyhedron>(L, 1),                            \
            fromlua<Boxed_polyhedron>(L, 2)));                          \
                                                                        \
    return 1;                                                           \
}

DEFINE_POLYHEDRON_SET_METAMETHOD(add) SET_OPERATION(JOIN)
DEFINE_POLYHEDRON_SET_METAMETHOD(sub) MAYBE_CLIP(.opposite()) SET_OPERATION(DIFFERENCE)
DEFINE_POLYHEDRON_SET_METAMETHOD(mul) MAYBE_CLIP() SET_OPERATION(INTERSECTION)

#undef DEFINE_POLYHEDRON_SET_METAMETHOD
#undef MAYBE_CLIP
#undef SET_OPERATION

#define DEFINE_POLYGON_SET_METAMETHOD(NAME, OP)                 \
static int polygon_ ##NAME(lua_State *L)                        \
{                                                               \
    std::visit(                                                 \
        [&L](auto &&x, auto &&y) {                              \
            tolua<Boxed_polygon>(L, OP(x, y));                  \
        },                                                      \
        fromlua<Boxed_polygon>(L, 1),                           \
        fromlua<Boxed_polygon>(L, 2));                          \
                                                                \
    return 1;                                                   \
}

DEFINE_POLYGON_SET_METAMETHOD(add, JOIN)
DEFINE_POLYGON_SET_METAMETHOD(sub, DIFFERENCE)
DEFINE_POLYGON_SET_METAMETHOD(mul, INTERSECTION)

#undef DEFINE_POLYGON_SET_METAMETHOD

#define DEFINE_SET_OPERATION(NAME, OP)                          \
static int NAME ##_2(lua_State *L)                              \
{                                                               \
    if (luaL_testudata(L, 1, "polygon")) {                      \
        polygon_## OP(L);                                       \
    } else if (luaL_testudata(L, 1, "polyhedron")) {            \
        polyhedron_## OP(L);                                    \
    } else {                                                    \
        luaL_argerror(L, 1, "expected polyhedron or polygon");  \
    }                                                           \
                                                                \
    return 1;                                                   \
}                                                               \
                                                                \
static int NAME ##_many(lua_State *L)                           \
{                                                               \
    int n = lua_gettop(L);                                      \
                                                                \
    if (n == 1) {                                               \
        return 1;                                               \
    }                                                           \
                                                                \
    if (n == 2) {                                               \
        return NAME ##_2(L);                                    \
    }                                                           \
                                                                \
    while (n-- > 1) {                                           \
        NAME ##_2(L);                                           \
        lua_replace(L, 1);                                      \
        lua_remove(L, 2);                                       \
    }                                                           \
                                                                \
    return 1;                                                   \
}

DEFINE_SET_OPERATION(join, add)
DEFINE_SET_OPERATION(difference, sub)
DEFINE_SET_OPERATION(intersection, mul)

#undef DEFINE_SET_OPERATION

static int clip(lua_State *L)
{
    luaL_checkudata(L, 1, "polyhedron");
    luaL_checkudata(L, 2, "plane");

    return polyhedron_mul(L);
}

static int minkowski_sum(lua_State *L)
{
    if (luaL_testudata(L, 1, "polygon")) {
        std::visit(
            [&L](auto &&x, auto &&y) {
                tolua<Boxed_polygon>(L, MINKOWSKI_SUM(x, y));
            },
            fromlua<Boxed_polygon>(L, 1),
            fromlua<Boxed_polygon>(L, 2));
    } else if (luaL_testudata(L, 1, "polyhedron")) {
        std::visit(
            [&L](auto &&x, auto &&y) {
                tolua<Boxed_polyhedron>(L, MINKOWSKI_SUM(x, y));
            },
            fromlua<Boxed_polyhedron>(L, 1),
            fromlua<Boxed_polyhedron>(L, 2));
    } else {
        luaL_argerror(L, 1, "expected polyhedron or polygon");
    }

    return 1;
}

static int flush(lua_State *L)
{
    const FT lambda = checkrational(L, 2);
    const FT mu = checkrational(L, 3);

    if (luaL_testudata(L, 1, "polygon")) {
        std::visit(
            [&L, &lambda, &mu](auto &&x) {
                tolua<Boxed_polygon>(L, FLUSH(x, lambda, mu));
            },
            fromlua<Boxed_polygon>(L, 1));
    } else if (luaL_testudata(L, 1, "polyhedron")) {
        const FT nu = checkrational(L, 4);

        std::visit(
            [&L, &lambda, &mu, &nu](auto &&x) {
                tolua<Boxed_polyhedron>(L, FLUSH(x, lambda, mu, nu));
            },
            fromlua<Boxed_polyhedron>(L, 1));
    } else if (luaL_testudata(L, 1, "bounding_volume")) {
        const FT nu = checkrational(L, 4);

        if (const auto p =
            fromlua<std::shared_ptr<Bounding_volume>>(L, 1)->flush(
                lambda, mu, nu)) {
            tolua<std::shared_ptr<Bounding_volume>>(L, p);
        } else {
            luaL_argerror(L, 1, "cannot flush this bounding volume");
        }
    } else {
        luaL_argerror(L, 1, "expected polyhedron or polygon");
    }

    return 1;
}

#define DEFINE_FLUSH_OPERATION(WHERE, LAMBDA, MU, NU)                   \
static int flush_## WHERE(lua_State *L)                                 \
{                                                                       \
    lua_pushnumber(L, LAMBDA);                                          \
    lua_pushnumber(L, MU);                                              \
    lua_pushnumber(L, NU);                                              \
                                                                        \
    return flush(L);                                                    \
}

DEFINE_FLUSH_OPERATION(west, -1, 0, 0)
DEFINE_FLUSH_OPERATION(east, 1, 0, 0)

DEFINE_FLUSH_OPERATION(south, 0, -1, 0)
DEFINE_FLUSH_OPERATION(north, 0, 1, 0)

DEFINE_FLUSH_OPERATION(bottom, 0, 0, -1)
DEFINE_FLUSH_OPERATION(top, 0, 0, 1)

#undef DEFINE_FLUSH_OPERATION

///////////////
// Polyhedra //
///////////////

static int output(lua_State *L)
{
    const char *s = lua_tostring(L, 1);
    const int j = lua_gettop(L);
    int i = 1 + (s != nullptr);

    std::vector<Boxed_polyhedron> v;

    v.reserve(j - i + 1);
    for (; i <= j; i++) {
        v.push_back(fromlua<Boxed_polyhedron>(L, i));
    }

    add_output_operations(s ? s : "", v);

    return 1;
}

static int offset(lua_State *L)
{
    const FT delta = checkrational(L, 2);

    std::visit(
        [&L, &delta](auto &&x) {
            tolua<Boxed_polygon>(L, OFFSET(x, delta));
        }, fromlua<Boxed_polygon>(L, 1));

    return 1;
}

static int extrusion(lua_State *L)
{
    const int h = lua_gettop(L);
    std::vector<Aff_transformation_3> v;

    if (h < 2) {
        return luaL_error(L, "can't make extrusion from given arguments");
    }

    v.reserve(h - 1);
    for (int i = 2; i <= h; i++) {
        v.push_back(
            fromlua<Aff_transformation_3>(L, i));
    }

    std::visit(
        [&L, &v](auto &&x) {
            tolua<Boxed_polyhedron>(L, EXTRUSION(x, std::move(v)));
        }, fromlua<Boxed_polygon>(L, 1));

    return 1;
}

static int hull(lua_State *L)
{
    const int n = lua_gettop(L);

    if (luaL_testudata(L, 1, "polyhedron")
        || luaL_testudata(L, 1, "point_3d")) {
        std::shared_ptr<Polyhedron_hull_operation> p =
            std::make_shared<Polyhedron_hull_operation>();

        for (int i = 1; i <= n; i++) {
            if (luaL_testudata(L, i, "point_3d")) {
                p->push_back(fromlua<Point_3>(L, i));
            } else if (luaL_testudata(L, i, "polyhedron")) {
                std::visit(
                    [&p](auto &&x) {
                        p->push_back(x);
                    },
                    fromlua<Boxed_polyhedron>(L, i));
            } else {
                luaL_argerror(L, i, nullptr);
            }
        }

        tolua<Boxed_polyhedron>(L, HULL(p));
    } else {
        std::shared_ptr<Polygon_hull_operation> p =
            std::make_shared<Polygon_hull_operation>();

        for (int i = 1; i <= n; i++) {
            if (luaL_testudata(L, i, "point_2d")) {
                p->push_back(fromlua<Point_2>(L, i));
            } else if (luaL_testudata(L, i, "polygon")) {
                std::visit(
                    [&p](auto &&x) {
                        p->push_back(CONVERT_TO<Polygon_set>(x));
                    },
                    fromlua<Boxed_polygon>(L, i));
            } else {
                luaL_argerror(L, i, nullptr);
            }
        }

        tolua<Boxed_polygon>(L, HULL(p));
    }

    return 1;
}

#define DEFINE_SUBDIVISION_OPERATION(SUFFIX, OP)                        \
static int subdivide_ ##SUFFIX(lua_State *L)                            \
{                                                                       \
    const int m = luaL_checkinteger(L, 2);                              \
                                                                        \
    return std::visit(                                                  \
        [&L, &m](auto &&x) {                                            \
            return tolua<Boxed_polyhedron>(L, OP(x, m));                \
        },                                                              \
        fromlua<Boxed_polyhedron>(L, 1));                               \
}

DEFINE_SUBDIVISION_OPERATION(loop, LOOP)
DEFINE_SUBDIVISION_OPERATION(catmull_clark, CATMULL_CLARK)
DEFINE_SUBDIVISION_OPERATION(doo_sabin, DOO_SABIN)
DEFINE_SUBDIVISION_OPERATION(sqrt_3, SQRT_3)

#undef DEFINE_SUBDIVISION_OPERATION

static int color_selection(lua_State *L)
{
    std::variant<std::shared_ptr<Face_selector>,
                 std::shared_ptr<Vertex_selector>> p;

    if (luaL_testudata(L, 2, "face_selector")) {
        p = fromlua<std::shared_ptr<Face_selector>>(L, 2);
    } else if (luaL_testudata(L, 2, "vertex_selector")) {
        p = fromlua<std::shared_ptr<Vertex_selector>>(L, 2);
    } else {
        luaL_argerror(L, 2, "expected face or vertex selector");
    }

    FT v[4] = {0, 0, 0, 1};

    if (!lua_isnoneornil(L, 3)) {
        int n = luaL_checkinteger(L, 3);

        for (int i = 0; i < 3; i++) {
            v[i] = (n >> i) & 1;
        }
    }

    return std::visit(
        [&L, &v](auto &&x, auto &&y) {
            return tolua<Boxed_polyhedron>(
                L, COLOR_SELECTION(x, y, v[0], v[1], v[2], v[3]));
        },
        fromlua<Boxed_polyhedron>(L, 1), p);
}

#define DEFINE_SIMPLE_MESH_OPERATION(NAME, OP, T)               \
static int NAME(lua_State *L)                                   \
{                                                               \
    std::shared_ptr<T> p;                                       \
    FT l;                                                       \
                                                                \
    if (luaL_testudata(L, 2, type_name<std::shared_ptr<T>>)) {  \
        p = fromlua<std::shared_ptr<T>>(L, 2);                  \
        l = checkrational(L, 3);                                \
    } else {                                                    \
        l = checkrational(L, 2);                                \
    }                                                           \
                                                                \
    return std::visit(                                          \
        [&L, &p, &l](auto &&x) {                                \
            return tolua<Boxed_polyhedron>(L, OP(x, p, l));     \
        },                                                      \
        fromlua<Boxed_polyhedron>(L, 1));                       \
}

DEFINE_SIMPLE_MESH_OPERATION(perturb, PERTURB, Vertex_selector)
DEFINE_SIMPLE_MESH_OPERATION(refine, REFINE, Face_selector)

#undef DEFINE_SIMPLE_MESH_OPERATION

static int remesh(lua_State *L)
{
    std::shared_ptr<Face_selector> p;
    std::shared_ptr<Edge_selector> q;
    FT l;
    int n, k = 2;

    if (luaL_testudata(L, k, "face_selector")) {
        p = fromlua<std::shared_ptr<Face_selector>>(L, k++);
    }

    if (luaL_testudata(L, k, "edge_selector")) {
        q = fromlua<std::shared_ptr<Edge_selector>>(L, k++);
    }

    l = checkrational(L, k++);
    n = luaL_optinteger(L, k, 1);

    return std::visit(
        [&L, &p, &q, &l, &n](auto &&x) {
            return tolua<Boxed_polyhedron>(L, REMESH(x, p, q, l, n));
        },
        fromlua<Boxed_polyhedron>(L, 1));
}

static int fair(lua_State *L)
{
    const auto &p = fromlua<std::shared_ptr<Vertex_selector>>(L, 2);
    const int n = luaL_optinteger(L, 3, 1);

    if (n < 0 || n > 2) {
        luaL_argerror(L, 2, "expected 0, 1, or 2");
    }

    return std::visit(
        [&L, &p, &n](auto &&x) {
            return tolua<Boxed_polyhedron>(L, FAIR(x, p, n));
        },
        fromlua<Boxed_polyhedron>(L, 1));
}

static int smooth_shape(lua_State *L)
{
    std::shared_ptr<Face_selector> p;
    std::shared_ptr<Vertex_selector> q;
    int n = 2;

    if (luaL_testudata(L, n, "face_selector")) {
        p = fromlua<std::shared_ptr<Face_selector>>(L, n++);
    }

    if (luaL_testudata(L, n, "vertex_selector")) {
        q = fromlua<std::shared_ptr<Vertex_selector>>(L, n++);
    }

    const FT t = checkrational(L, n++);
    const int m = luaL_optinteger(L, n, 1);

    return std::visit(
        [&L, &p, &q, &t, &m](auto &&x) {
            return tolua<Boxed_polyhedron>(L, SMOOTH_SHAPE(x, p, q, t, m));
        },
        fromlua<Boxed_polyhedron>(L, 1));
}

static int deflate(lua_State *L)
{
    std::shared_ptr<Vertex_selector> p;
    int n = 2;

    if (luaL_testudata(L, 2, "vertex_selector")) {
        p = fromlua<std::shared_ptr<Vertex_selector>>(L, n++);
    }

    const int m = luaL_checkinteger(L, n++);

    FT w_H = FT::ET(1, 10);
    if (!lua_isnoneornil(L, n)) {
        w_H = checkrational(L, n++);
    }

    FT w_M;
    if (!lua_isnoneornil(L, n)) {
        w_M = checkrational(L, n++);
    }

    return std::visit(
        [&L, &p, &m, &w_H, &w_M](auto &&x) {
            return tolua<Boxed_polyhedron>(L, DEFLATE(x, p, m, w_H, w_M));
        },
        fromlua<Boxed_polyhedron>(L, 1));
}

static int deform(lua_State *L)
{
    const bool p = luaL_testudata(L, 3, "vertex_selector");
    std::vector<std::pair<std::shared_ptr<Vertex_selector>,
                          Aff_transformation_3>> v;

    int i;
    for (i = p ? 3 : 2; luaL_testudata(L, i, "vertex_selector"); i += 2) {
        v.push_back(
            std::pair(
                fromlua<std::shared_ptr<Vertex_selector>>(L, i),
                fromlua<Aff_transformation_3>(L, i + 1)));
    }

    const FT tau = checkrational(L, i);
    unsigned int n;

    if (lua_isinteger(L, i + 1)) {
        const int m = lua_tointeger(L, i + 1);

        if (m < 0) {
            luaL_argerror(L, i + 1, "expected non-negative integer");
        }

        n = static_cast<unsigned int>(m);
    } else {
        n = (tau == 0) ? 10 : std::numeric_limits<unsigned int>::max();
    }

    if (p) {
        const std::shared_ptr<Vertex_selector> &q =
            fromlua<std::shared_ptr<Vertex_selector>>(L, 2);

        return std::visit(
            [&L, &q, &v, &tau, &n](auto &&x) {
                return tolua<Boxed_polyhedron>(
                    L, DEFORM(x, q, std::move(v), tau, n));
            },
            fromlua<Boxed_polyhedron>(L, 1));
    } else {
        return std::visit(
            [&L, &v, &tau, &n](auto &&x) {
                return tolua<Boxed_polyhedron>(
                    L, DEFORM(x, std::move(v), tau, n));
            },
            fromlua<Boxed_polyhedron>(L, 1));
    }
}

static int corefine(lua_State *L)
{
    if (luaL_testudata(L, 2, "plane")) {
        const auto &pi = fromlua<Plane_3>(L, 2);

        return std::visit(
            [&L, &pi](auto &&x) {
                return tolua<Boxed_polyhedron>(L, COREFINE(x, pi));
            },
            fromlua<Boxed_polyhedron>(L, 1));
    } else {
        return std::visit(
            [&L](auto &&x, auto &&y) {
                return tolua<Boxed_polyhedron>(L, COREFINE(x, y));
            },
            fromlua<Boxed_polyhedron>(L, 1),
            fromlua<Boxed_polyhedron>(L, 2));
    }
}

static int complement(lua_State *L)
{
    if (luaL_testudata(L, 1, "bounding_volume")) {
        return bounding_volume_bnot(L);
    }

    if (luaL_testudata(L, 1, "vertex_selector")) {
        return selector_bnot<Vertex_selector>(L);
    }

    if (luaL_testudata(L, 1, "face_selector")) {
        return selector_bnot<Face_selector>(L);
    }

    if (luaL_testudata(L, 1, "edge_selector")) {
        return selector_bnot<Edge_selector>(L);
    }

    if (luaL_testudata(L, 1, "polygon")) {
        std::visit(
            [&L](auto &&x) {
                tolua<Boxed_polygon>(L, COMPLEMENT(x));
            },
            fromlua<Boxed_polygon>(L, 1));

        return 1;
    }

    if (luaL_testudata(L, 1, "polyhedron")) {
        std::visit(
            [&L](auto &&x) {
                tolua<Boxed_polyhedron>(L, COMPLEMENT(x));
            },
            fromlua<Boxed_polyhedron>(L, 1));

        return 1;
    }

    luaL_argerror(L, 1, nullptr);

    return 1;
}

#define PUSH_METATABLE(NAME, T)                  \
    type_name<T> = NAME;                        \
    luaL_newmetatable(L, type_name<T>);         \
    SET_KEY("__gc", collect<T>);                \
    SET_KEY("__tostring", tostring<T>);

#define SET_KEY(KEY, OP) {                      \
        lua_pushcfunction(L, OP);               \
        lua_setfield(L, -2, KEY);               \
    }

static int open_base(lua_State *L)
{
    const luaL_Reg ops[] = {
        {"set_projection_tolerance", set_tolerance<Tolerances::projection>},
        {"set_curve_tolerance", set_tolerance<Tolerances::curve>},
        {"set_sine_tolerance", set_tolerance<Tolerances::sine>},

        {"point", point},
        {"plane", plane},

        {"print_note", print<Operation::NOTE>},
        {"print_warning", print<Operation::WARNING>},
        {"print_error", print<Operation::ERROR>},
        {"output", output},

        {nullptr, nullptr}};

    lua_pushglobaltable(L);
    luaL_setfuncs(L, ops, 0);

    return 1;
}

static int open_transformation(lua_State *L)
{
    const luaL_Reg ops[] = {
        {"scaling", scaling},
        {"translation", translation},
        {"rotation", rotation},

        {"apply", transformation_apply},

        {"flush", flush},
        {"flush_south", flush_south},
        {"flush_north", flush_north},
        {"flush_east", flush_east},
        {"flush_west", flush_west},
        {"flush_bottom", flush_bottom},
        {"flush_top", flush_top},

        {nullptr, nullptr}};

    luaL_newlib(L, ops);

    return 1;
}

static int open_volumes(lua_State *L)
{
    const luaL_Reg ops[] = {
        {"plane",
         primitive<BOUNDING_PLANE<>, std::shared_ptr<Bounding_volume>, 4>},
        {"halfspace-interior",
         primitive<BOUNDING_HALFSPACE_INTERIOR<>,
         std::shared_ptr<Bounding_volume>, 4>},
        {"halfspace",
         primitive<BOUNDING_HALFSPACE<>, std::shared_ptr<Bounding_volume>, 4>},

        {"box",
         primitive<BOUNDING_BOX<>, std::shared_ptr<Bounding_volume>, 3>},
        {"box-boundary",
         primitive<BOUNDING_BOX_BOUNDARY<>,
         std::shared_ptr<Bounding_volume>, 3>},
        {"box-interior",
         primitive<BOUNDING_BOX_INTERIOR<>,
         std::shared_ptr<Bounding_volume>, 3>},

        {"sphere",
         primitive<BOUNDING_SPHERE<>, std::shared_ptr<Bounding_volume>, 1>},
        {"sphere-boundary",
         primitive<BOUNDING_SPHERE_BOUNDARY<>,
         std::shared_ptr<Bounding_volume>, 1>},
        {"sphere-interior",
         primitive<BOUNDING_SPHERE_INTERIOR<>,
         std::shared_ptr<Bounding_volume>, 1>},

        {"cylinder",
         primitive<BOUNDING_CYLINDER<>, std::shared_ptr<Bounding_volume>, 2>},
        {"cylinder-boundary",
         primitive<BOUNDING_CYLINDER_BOUNDARY<>,
         std::shared_ptr<Bounding_volume>, 2>},
        {"cylinder-interior",
         primitive<BOUNDING_CYLINDER_INTERIOR<>,
         std::shared_ptr<Bounding_volume>, 2>},

        {nullptr, nullptr}};

    luaL_newlib(L, ops);

    return 1;
}

static int open_selection(lua_State *L)
{
    const luaL_Reg ops[] = {
        {"vertices_in", vertices_in},
        {"faces_in", faces_in},
        {"faces_partially_in", faces_partially_in},
        {"edges_in", edges_in},
        {"edges_partially_in", edges_partially_in},

        {"expand_selection", relative_selection<1>},
        {"contract_selection", relative_selection<-1>},

        {nullptr, nullptr}};

    luaL_newlib(L, ops);

    return 1;
}

static int open_polygons(lua_State *L)
{
    const luaL_Reg ops[] = {
        {"simple", ngon},
        {"regular", primitive<REGULAR_POLYGON<>, Boxed_polygon, int, FT>},
        {"isosceles_triangle",
         primitive<ISOSCELES_TRIANGLE<>, Boxed_polygon, 2>},
        {"right_triangle",
         primitive<RIGHT_TRIANGLE<>, Boxed_polygon, 2>},
        {"rectangle", primitive<RECTANGLE<>, Boxed_polygon, 2>},
        {"circle", primitive<CIRCLE<>, Boxed_polygon, 1>},
        {"circular_sector", primitive<CIRCULAR_SECTOR<>, Boxed_polygon, 2>},
        {"circular_segment", primitive<CIRCULAR_SEGMENT<>, Boxed_polygon, 2>},
        {"ellipse", primitive<ELLIPSE<>, Boxed_polygon, 2>},
        {"elliptic_sector", primitive<ELLIPTIC_SECTOR<>, Boxed_polygon, 3>},

        {nullptr, nullptr}};

    luaL_newlib(L, ops);

    return 1;
}

static int open_polyhedra(lua_State *L)
{
    const luaL_Reg ops[] = {
        {"tetrahedron", primitive<TETRAHEDRON<>, Boxed_polyhedron, 3>},
        {"square_pyramid", primitive<SQUARE_PYRAMID<>, Boxed_polyhedron, 3>},
        {"octahedron", octahedron},
        {"regular_pyramid", primitive<REGULAR_PYRAMID<>, Boxed_polyhedron, int, FT, FT>},
        {"regular_bipyramid", regular_bipyramid},
        {"cuboid", primitive<CUBOID<>, Boxed_polyhedron, 3>},
        {"icosahedron", primitive<ICOSAHEDRON<>, Boxed_polyhedron, 1>},
        {"sphere", primitive<SPHERE<>, Boxed_polyhedron, 1>},
        {"cylinder", primitive<CYLINDER<>, Boxed_polyhedron, 2>},

        {nullptr, nullptr}};

    luaL_newlib(L, ops);

    return 1;
}

static int open_operations(lua_State *L)
{
    const luaL_Reg ops[] = {
        {"offset", offset},
        {"extrusion", extrusion},
        {"union", join_many},
        {"difference", difference_many},
        {"intersection", intersection_many},
        {"complement", complement},
        {"clip", clip},
        {"minkowski_sum", minkowski_sum},
        {"hull", hull},

        {"color_selection", color_selection},
        {"refine", refine},
        {"perturb", perturb},
        {"remesh", remesh},
        {"fair", fair},
        {"deform", deform},
        {"smooth_shape", smooth_shape},
        {"deflate", deflate},
        {"corefine", corefine},

        {"subdivide_catmull_clark", subdivide_catmull_clark},
        {"subdivide_doo_sabin", subdivide_doo_sabin},
        {"subdivide_loop", subdivide_loop},
        {"subdivide_sqrt_3", subdivide_sqrt_3},

        {nullptr, nullptr}};

    luaL_newlib(L, ops);

    return 1;
}

static const char *filename;

static void pushstack(lua_State *L)
{
    lua_Debug ar;
    int i, h;

    h = lua_gettop(L);

    /* Print the Lua stack. */

    lua_pushliteral(L, "\n\n");

    for (i = 0; lua_getstack(L, i, &ar); i++) {
        lua_getinfo(L, "Snl", &ar);

        if (!std::strcmp(ar.what, "C")) {
            lua_pushfstring(L, "#%d in a %sC function%s\n",
                            i, ANSI_COLOR(0, 33), ANSI_COLOR(0, 37));
        } else if (!std::strcmp(ar.what, "main")) {
            lua_pushfstring(L, "#%d in the %smain chunk%s, %s%s:%d%s\n",
                            i,
                            ANSI_COLOR(0, 33), ANSI_COLOR(0, 37),
                            ANSI_COLOR(1, 37), ar.short_src,
                            ar.currentline, ANSI_COLOR(0,));
        } else if (!std::strcmp(ar.what, "Lua")) {
            lua_pushfstring(L, "#%d in function '%s%s%s', %s%s:%d%s\n",
                            i,
                            ANSI_COLOR(0, 33), ar.name, ANSI_COLOR(0, 37),
                            ANSI_COLOR(1, 37), ar.short_src,
                            ar.currentline, ANSI_COLOR(0,));
        }
    }

    if (i == 0) {
        lua_pushstring(L, "No activation records.\n");
    }

    lua_pushfstring(L, ANSI_COLOR(0,));
    lua_concat(L, lua_gettop(L) - h);
}

static int error_handler(lua_State *L)
{
    lua_Debug ar;
    const char *s = lua_tostring(L, 1);

    // Search the stack for the frame that matches the error message.

    for (int i = 0; lua_getstack(L, i, &ar) ; i++) {
        lua_getinfo(L, "Sln", &ar);
        if (ar.what[0] == 'C') {
            continue;
        }

        const int n = std::strlen(ar.short_src);
        std::string t = std::to_string(ar.currentline);

        if (!std::strncmp(s, ar.short_src, n)
            && s[n] == ':'
            && !std::strncmp(s + n + 1, t.c_str(), t.size())
            && s[n + t.size() + 1] == ':') {
            luaL_Buffer b;

            // Print a line with the function name, if available, plus the error
            // message, all with colorized source locations.

            luaL_buffinit(L, &b);

            // The colorized filename

            luaL_addstring(&b, ANSI_COLOR(1, 31));
            luaL_addstring(&b, ar.short_src);
            luaL_addstring(&b, ANSI_COLOR(0, 37));

            luaL_addchar(&b, ':');

            // The colorized linenumber

            luaL_addstring(&b, ANSI_COLOR(1, 37));
            luaL_addstring(&b, t.c_str());
            luaL_addstring(&b, ANSI_COLOR(0, 37));
            luaL_addchar(&b, ':');

            luaL_pushresult(&b);

            if (ar.name) {
                // The function name

                if (!ar.namewhat[0]) {
                    lua_pushstring(L, " in function: '");
                } else {
                    lua_pushstring(L, " in ");
                    lua_pushstring(L, ar.namewhat);
                    lua_pushstring(L, " function: '");
                }

                lua_pushstring(L, ar.name);
                lua_pushstring(L, "'\n");
                lua_pushvalue(L, 2);
            }

            // The rest of the error message

            lua_pushstring(L, ANSI_COLOR(1, 31));
            lua_pushstring(L, " error");
            lua_pushstring(L, ANSI_COLOR(0, 37));
            lua_pushstring(L, s + n + t.size() + 1);
        }
    }

    // The above approach might fail, for instance in case of syntax errors,
    // where there is no stack.  In this case, try to produce similar output
    // based on the current filename and error message.

    if (lua_gettop(L) == 1 && filename) {
        luaL_Buffer b;
        const int n = std::strlen(filename);

        if (s && !std::strncmp(s, filename, n) && s[n] == ':') {
            luaL_buffinit(L, &b);

            // The colorized filename

            luaL_addstring(&b, ANSI_COLOR(1, 31));
            luaL_addlstring(&b, s, n);
            luaL_addstring(&b, ANSI_COLOR(0, 37));

            s += n;
            luaL_addchar(&b, *s++);

            // The colorized linenumber

            const char *t = s;
            while (std::isdigit(*t)) {
                t++;
            }

            if (t > s) {
                luaL_addstring(&b, ANSI_COLOR(1, 37));
                luaL_addlstring(&b, s, t - s);
                luaL_addstring(&b, ANSI_COLOR(0, 37));
            }

            // The rest of the message

            luaL_addstring(&b, t);
            luaL_pushresult(&b);
        }
    }

    pushstack(L);
    lua_concat(L, lua_gettop(L) - 1);

    return 1;
}

int run_lua(const char *input, char **first, char **last)
{
    lua_State *L;

    L = luaL_newstate();
    assert(L);

    luaL_requiref(L, "package", luaopen_package, 0);
    assert(lua_gettop(L) == 1);
    assert(lua_istable(L, -1));

    // Set the package path, if necessary.

    if (!Options::include_directories.empty()) {

        {
            luaL_Buffer b;

            luaL_buffinit(L, &b);

            for (std::string x: Options::include_directories) {
                while (x.back() == '/') {
                    x.pop_back();
                }

                luaL_addstring(&b, (x + "/?.lua;").c_str());
            }


            lua_getfield(L, 1, "path");
            assert(lua_isstring(L, -1));
            luaL_addvalue(&b);


            luaL_pushresult(&b);
        }

        lua_setfield(L, 1, "path");
    }

    // Install module loaders.

    lua_getfield(L, 1, "preload");
    assert(lua_istable(L, -1));
    SET_KEY("gamma.base", open_base);
    SET_KEY("gamma.transformation", open_transformation);
    SET_KEY("gamma.volumes", open_volumes);
    SET_KEY("gamma.selection.core", open_selection);
    SET_KEY("gamma.polygons", open_polygons);
    SET_KEY("gamma.polyhedra", open_polyhedra);
    SET_KEY("gamma.operations.core", open_operations);

    // Define types and initialize base modules.

    PUSH_METATABLE("point_2d", Point_2);
    PUSH_METATABLE("point_3d", Point_3);
    PUSH_METATABLE("plane", Plane_3);

    PUSH_METATABLE("transformation_2d", Aff_transformation_2);
    SET_KEY("__mul", transformation_2_mul);

    PUSH_METATABLE("transformation_3d", Aff_transformation_3);
    SET_KEY("__mul", transformation_3_mul);

    PUSH_METATABLE("bounding_volume", std::shared_ptr<Bounding_volume>);
    SET_KEY("__add", bounding_volume_add);
    SET_KEY("__sub", bounding_volume_sub);
    SET_KEY("__mul", bounding_volume_mul);
    SET_KEY("__bnot", bounding_volume_bnot);

    PUSH_METATABLE("vertex_selector", std::shared_ptr<Vertex_selector>);
    SET_KEY("__add", selector_add<Vertex_selector>);
    SET_KEY("__sub", selector_sub<Vertex_selector>);
    SET_KEY("__mul", selector_mul<Vertex_selector>);
    SET_KEY("__bnot", selector_bnot<Vertex_selector>);

    PUSH_METATABLE("face_selector", std::shared_ptr<Face_selector>);
    SET_KEY("__add", selector_add<Face_selector>);
    SET_KEY("__sub", selector_sub<Face_selector>);
    SET_KEY("__mul", selector_mul<Face_selector>);
    SET_KEY("__bnot", selector_bnot<Face_selector>);

    PUSH_METATABLE("edge_selector", std::shared_ptr<Edge_selector>);
    SET_KEY("__add", selector_add<Edge_selector>);
    SET_KEY("__sub", selector_sub<Edge_selector>);
    SET_KEY("__mul", selector_mul<Edge_selector>);
    SET_KEY("__bnot", selector_bnot<Edge_selector>);

    PUSH_METATABLE("polygon", Boxed_polygon);
    SET_KEY("__add", polygon_add);
    SET_KEY("__sub", polygon_sub);
    SET_KEY("__mul", polygon_mul);

    PUSH_METATABLE("polyhedron", Boxed_polyhedron);
    SET_KEY("__add", polyhedron_add);
    SET_KEY("__sub", polyhedron_sub);
    SET_KEY("__mul", polyhedron_mul);

    luaL_requiref(L, "_G", luaopen_base, 1);
    luaL_requiref(L, "base", open_base, 1);

    for (const auto &x: std::initializer_list<luaL_Reg>{
        {LUA_COLIBNAME, luaopen_coroutine},
        {LUA_TABLIBNAME, luaopen_table},
        {LUA_IOLIBNAME, luaopen_io},
        {LUA_OSLIBNAME, luaopen_os},
        {LUA_STRLIBNAME, luaopen_string},
        {LUA_MATHLIBNAME, luaopen_math},
        {LUA_UTF8LIBNAME, luaopen_utf8},
        {LUA_DBLIBNAME, luaopen_debug}}) {
        luaL_requiref(L, x.name, x.func, 0);
    }

    lua_settop(L, 0);
    lua_pushcfunction(L, error_handler);

    // Annotate operations with source location information.

    assert(Operation::hook == nullptr);
    Operation::hook = [&L](Operation &op) {
        lua_Debug ar;
        for (int i = 0; lua_getstack(L, i, &ar) ; i++) {
            lua_getinfo(L, "Sl", &ar);
            if (ar.what[0] == 'C') {
                continue;
            }

            std::string s(ar.short_src);

            op.annotations.insert({"file", s});
            op.annotations.insert({"line", std::to_string(ar.currentline)});

            return;
        }
    };

    int result;

    // Define variables.

    filename = "<command line>";

    assert(lua_gettop(L) == 1);
    lua_pushglobaltable(L);

    for (const auto &x: Options::definitions) {
        if (x.second.empty()) {
            lua_pushboolean(L, 1);
        } else {
            const auto s = "return " + x.second;

            // In case of syntax errors, print the error via the handler and
            // exit.

            if (((result = luaL_loadbuffer(
                      L, s.c_str(), s.size(), "=<command line>")) != LUA_OK)
                || ((result = lua_pcall(L, 0, 1, 1)) != LUA_OK)) {

                goto handle_error;
            }
        }

        lua_setfield(L, -2, x.first.c_str());
    }

    lua_settop(L, 1);

    // Load the script.

    if (!std::strcmp(input, "-")) {
        filename = "stdin";
        result = luaL_loadfile(L, nullptr);
    } else {
        filename = input;
        result = luaL_loadfile(L, input);
    }

    if (result != LUA_OK) {
        goto handle_error;
    }

    // Push arguments and execute the script.

    {
        lua_pushstring(L, input);
        int n = 1;

        for (char **i = first ; i < last ; i++, n++) {
            lua_pushstring(L, *i);
        }

        result = lua_pcall(L, n, 0, 1);
    }

handle_error:
    switch (result) {
    case LUA_ERRERR:
        std::cerr << "An error occured while executing the error handler.\n"
                  << std::endl;

        break;

    case LUA_ERRMEM:
        std::cerr << "A memory allocation error occured.\n"
                  << std::endl;

        break;

#if LUA_VERSION_NUM < 504
    case LUA_ERRGCMM:
        std::cerr << "An error occured while collecting garbage.\n"
                  << std::endl;

        break;
#endif

    case LUA_ERRSYNTAX:
    case LUA_ERRFILE: {
        // A loading failure; call the error handler to colorize the output.

        const int n = lua_gettop(L);
        assert (n > 1);

        if (n > 2) {
            lua_replace(L, 2);
            lua_settop(L, 2);
        }

        lua_call(L, 1, 1);
        std::cerr << lua_tostring(L, -1) << std::endl;
    }
        break;

    case LUA_ERRRUN:
        std::cerr << lua_tostring(L, -1) << std::endl;
    }

    lua_close(L);

    Operation::hook = nullptr;

    return result;
}

#undef SET_META_KEY
#undef PUSH_METATABLE
