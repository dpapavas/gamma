#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Polygon_mesh_processing/compute_normal.h>

#include "kernel.h"
#include "sink_operations.h"

static inline void write_off_color(std::ostream &s, const CGAL::IO::Color &c)
{
    s << " " << static_cast<int>(c.red())
      << " " << static_cast<int>(c.green())
      << " " << static_cast<int>(c.blue())
      << " " << static_cast<int>(c.alpha());
}

static void write_off(std::ostream &s, const Surface_mesh &mesh)
{
    const auto [vertex_colors, has_vertex_colors] =
        mesh.property_map<Surface_mesh::Vertex_index,
                          CGAL::IO::Color>("v:color");

    const auto [face_colors, has_face_colors] =
        mesh.property_map<Surface_mesh::Face_index,
                          CGAL::IO::Color>("f:color");

    if (has_vertex_colors) {
        s << "COFF\n";
    } else {
        s << "OFF\n";
    }

    s << mesh.number_of_vertices() << " "
      << mesh.number_of_faces() << " "
      << mesh.number_of_edges() << "\n" << std::scientific;

    // Surface mesh indexes are (seemingly) not necessarily contiguous
    // and compact, so we need to make our own.

    std::unordered_map<Surface_mesh::Vertex_index, std::size_t> map;
    int n = 0;

    for(Surface_mesh::Vertex_index v: mesh.vertices()) {
        const Surface_mesh::Point& P = mesh.point(v);

        map[v] = n++;

        s << CGAL::to_double(P.x())
          << " " << CGAL::to_double(P.y())
          << " " << CGAL::to_double(P.z());

        if (has_vertex_colors) {
            write_off_color(s, vertex_colors[v]);
        }

        s << "\n";
    }

    for (Surface_mesh::Face_index f: mesh.faces()) {
        s << mesh.degree(f);

        for(Surface_mesh::Vertex_index v:
                CGAL::vertices_around_face(mesh.halfedge(f), mesh)) {
            s << " " << map[v];
        }

        if (has_face_colors) {
            write_off_color(s, face_colors[f]);
        }

        s << "\n";
    }
}

void Write_WRL_operation::evaluate()
{
    std::ofstream s;

    s.open(filename);

    if (!s) {
        message(Operation::ERROR, "could not open output file");
        return;
    }

    s << "#VRML V2.0 utf8\n";

    for(auto p: operands) {
        Surface_mesh &M = *p->get_value();

        s << "\n"
          << "Shape {\n"
          << "    appearance Appearance {\n"
          << "        material Material {\n"
          << "            diffuseColor 0.6 0.6 0.6\n"
          << "        }\n"
          << "    }\n\n"
          << "    geometry IndexedFaceSet {\n"
          << "        convex FALSE\n"
          << "        solid  FALSE\n"
          << "        coord  Coordinate {\n"
          << "            point [\n"
          << std::fixed;

        // In the spirit of write_off.

        std::unordered_map<Surface_mesh::Vertex_index, std::size_t> map;
        int n = 0;

        for(Surface_mesh::Vertex_index v: M.vertices()) {
            const Surface_mesh::Point& P = M.point(v);

            map[v] = n++;

            s << "                " << CGAL::to_double(P.x())
              << " " << CGAL::to_double(P.y())
              << " " << CGAL::to_double(P.z());

            s << ",\n";
        }

        s << "            ]\n"
          << "        }\n"
          << "        coordIndex [\n";

        for (Surface_mesh::Face_index f: M.faces()) {
            s << "            ";

            for(Surface_mesh::Vertex_index v:
                    CGAL::vertices_around_face(M.halfedge(f), M)) {
                s << map[v] << ", ";
            }

            s << "-1, \n";
        }

        s << "        ]\n"
          << "    }\n"
          << "}\n";
    }
}

void Write_STL_operation::evaluate()
{
    Surface_mesh M;

    // STL doesn't support colors, so there's no point in copying the
    // relevant maps.

    for(auto p: operands) {
        M += *p->get_value();
    }

    CGAL::Polygon_mesh_processing::triangulate_faces(M.faces(), M);

    std::ofstream s;

    s.open(filename);

    if (!s) {
        message(Operation::ERROR, "could not open output file");
        return;
    }

    s << "solid foo\n" << std::scientific;

    for (Surface_mesh::Face_index f: M.faces()) {
        Vector_3 u = CGAL::Polygon_mesh_processing::compute_face_normal(f, M);

        assert(M.degree(f) == 3);

        s << "  facet normal "
          << " " << CGAL::to_double(u.x())
          << " " << CGAL::to_double(u.y())
          << " " << CGAL::to_double(u.z())
          << "\n    outer loop\n";

        for(Surface_mesh::Vertex_index v:
                CGAL::vertices_around_face(M.halfedge(f), M)) {
            const Surface_mesh::Point& P = M.point(v);

            s << "      vertex "
              << " " << CGAL::to_double(P.x())
              << " " << CGAL::to_double(P.y())
              << " " << CGAL::to_double(P.z())
              << "\n";
        }

        s << "    endloop\n"
          << "  endfacet\n";
    }

    s << "endsolid foo" << std::endl;
}

void Write_OFF_operation::evaluate()
{
    Surface_mesh M;

    for(auto p: operands) {
        if (p->get_value()->property_map<Surface_mesh::Vertex_index,
                                         CGAL::IO::Color>("v:color").second) {
            M.add_property_map<Surface_mesh::Vertex_index,
                               CGAL::IO::Color>("v:color");
            break;
        }
    }

    for(auto p: operands) {
        if (p->get_value()->property_map<Surface_mesh::Face_index,
                                         CGAL::IO::Color>("f:color").second) {
            M.add_property_map<Surface_mesh::Face_index,
                               CGAL::IO::Color>("f:color");
            break;
        }
    }

    for(auto p: operands) {
        M += *p->get_value();
    }

    std::ofstream s;

    s.open(filename);

    if (!s) {
        message(Operation::ERROR, "could not open output file");
        return;
    }

    write_off(s, M);
}

#if defined(__unix__) && defined(__GNUG__)
#include <ext/stdio_filebuf.h>
#include <fcntl.h>

void Pipe_to_geomview_operation::evaluate()
{
    int fd = open(
        ("/tmp/geomview/" + (filename.empty() ? "output" : filename)).c_str(),
        O_WRONLY | O_NONBLOCK);

    if (fd < 0) {
        message(Operation::WARNING, "could not open pipe");
        return;
    }

    __gnu_cxx::stdio_filebuf<char> buf(fd, std::ios::out);
    std::ostream s(&buf);

    if (!s) {
        message(Operation::WARNING, "could not open pipe");
        return;
    }

    Surface_mesh M;

    for(auto p: operands) {
        if (p->get_value()->property_map<Surface_mesh::Vertex_index,
                                         CGAL::IO::Color>("v:color").second) {
            M.add_property_map<Surface_mesh::Vertex_index,
                               CGAL::IO::Color>("v:color");
            break;
        }
    }

    for(auto p: operands) {
        if (p->get_value()->property_map<Surface_mesh::Face_index,
                                         CGAL::IO::Color>("f:color").second) {
            M.add_property_map<Surface_mesh::Face_index,
                               CGAL::IO::Color>("f:color");
            break;
        }
    }

    for(auto p: operands) {
        M += *p->get_value();
    }

    s << "(geometry " << (filename.empty() ? "output" : filename) << " {\n";

    write_off(s, M);

    s << "appearance {+concave}\n"
      << "})\n"
      << "(camera Camera {})\n"
      << std::endl;
}
#else
void Pipe_to_geomview_operation::evaluate()
{
    message(
        Operation::ERROR, "this operation is not available on your platform");
}
#endif
