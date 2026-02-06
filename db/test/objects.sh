run() { rungamma --target=test "$@" -c "info objects"; }

objects() {
    match \# Vert. Tri. Edges AABB Name

    while [ $# -gt 0 ]; do
        match "${@:1:6}"
        shift 6
    done

    match ""
}

test_cube() {
    # A cube has 8 vertices, but the 4 + 4 vertices of two faces are
    # duplicated, because the faces are colored, which brings the
    # total up to 16.

    run -c "$(echo load test; cat $(dirname $0)/cube.off)" |
        objects 1 16 12 12 "-1, -1, -1, 1, 1, 1" "test"
}

test_nonconvex() {
    # The difference of two cubes; a non-convex polyhedron.

    run -c "load test <$(dirname $0)/convex.off" |
        objects 1 16 28 24 "-1, -1, -1, 1, 1, 1" "test"
}
