// Copyright 2022 Dimitris Papavasiliou

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

#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Polygon_mesh_processing/compute_normal.h>

#include "kernel.h"
#include "sink_operations.h"

static const std::string make_error_string(const char *message)
{
    return (std::string(message)
            + std::string(" (")
            + std::make_error_code(std::errc(errno)).message()
            + std::string(")"));
}

static inline void write_off_color(std::ostream &s, const CGAL::IO::Color &c)
{
    s << " " << static_cast<int>(c.red())
      << " " << static_cast<int>(c.green())
      << " " << static_cast<int>(c.blue())
      << " " << static_cast<int>(c.alpha());
}

static void write_off(std::ostream &s, const Surface_mesh &mesh)
{
    const auto vertex_colors = mesh.property_map<Surface_mesh::Vertex_index,
                                                 CGAL::IO::Color>("v:color");

    const auto face_colors = mesh.property_map<Surface_mesh::Face_index,
                                               CGAL::IO::Color>("f:color");

    if (vertex_colors) {
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

        if (vertex_colors) {
            write_off_color(s, (*vertex_colors)[v]);
        }

        s << "\n";
    }

    for (Surface_mesh::Face_index f: mesh.faces()) {
        s << mesh.degree(f);

        for(Surface_mesh::Vertex_index v:
                CGAL::vertices_around_face(mesh.halfedge(f), mesh)) {
            s << " " << map[v];
        }

        if (face_colors) {
            write_off_color(s, (*face_colors)[f]);
        }

        s << "\n";
    }
}

void Write_WRL_operation::evaluate()
{
    std::ofstream s;

    s.open(filename);

    if (!s) {
        message(
            Operation::ERROR, make_error_string("could not open output file"));
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
        message(
            Operation::ERROR, make_error_string("could not open output file"));
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
                                         CGAL::IO::Color>("v:color")) {
            M.add_property_map<Surface_mesh::Vertex_index,
                               CGAL::IO::Color>("v:color");
            break;
        }
    }

    for(auto p: operands) {
        if (p->get_value()->property_map<Surface_mesh::Face_index,
                                         CGAL::IO::Color>("f:color")) {
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
        message(
            Operation::ERROR, make_error_string("could not open output file"));
        return;
    }

    write_off(s, M);
}

#if defined(__unix__) && defined(__GNUG__)
#include <ext/stdio_filebuf.h>
#include <sys/socket.h>
#include <sys/un.h>

// This operation writes an output to a UNIX domain socket in OFF
// format.  Its intended use is to send geometry to the debugger for
// inspection.

// We need to:

void Inspect_operation::evaluate()
{
    //   1. create a socket,

    const int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd == -1) {
        message(Operation::WARNING, make_error_string("could not open socket"));
        return;
    }

    struct sockaddr_un addr;

    //   2. connect it to the remote end,

#define NAME "inspector"
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path + 1, NAME);

    if (connect(fd,
                (const struct sockaddr *)&addr,
                offsetof(struct sockaddr_un, sun_path) + sizeof(NAME)) == -1) {
        message(
            Operation::WARNING, make_error_string("could not connect socket"));
        return;
    }
#undef NAME

    //   3. accumulate all operands into a single mesh, as we did for
    //   `Write_OFF_operation`,

    Surface_mesh M;

    for(auto p: operands) {
        if (p->get_value()->property_map<Surface_mesh::Vertex_index,
                                         CGAL::IO::Color>("v:color")) {
            M.add_property_map<Surface_mesh::Vertex_index,
                               CGAL::IO::Color>("v:color");
            break;
        }
    }

    for(auto p: operands) {
        if (p->get_value()->property_map<Surface_mesh::Face_index,
                                         CGAL::IO::Color>("f:color")) {
            M.add_property_map<Surface_mesh::Face_index,
                               CGAL::IO::Color>("f:color");
            break;
        }
    }

    for(auto p: operands) {
        M += *p->get_value();
    }

    //   4. create an ouput stream that will write to the socket's
    //   file descriptor,

    __gnu_cxx::stdio_filebuf<char> buf(fd, std::ios::out);
    std::ostream s(&buf);

    assert(s);

    //   5. write the geometry, preceded by a command to the debugger
    //   to load it with the proper name,

    s << "load " << filename << "\n";
    write_off(s, M);
    s.flush();

    //   6. shutdown the write part of the socket, to signal to the
    //   debugger that we're done transmitting^[This causes the loop
    //   that executes commands from the socket on the debugger's end
    //   to exit, after which the debugger closes the socket.] and
    //   finally

    shutdown(fd, SHUT_WR);

    //   7. wait for the socket to close at the remote end.^[This
    //   ensures that the operation doesn't exit before the debugger
    //   has finished loading the geometry.  This is not important to
    //   use, but it can be convenient to avoid race conditions.  For
    //   instance, when making figures for the documentation, we
    //   follow this load command with a print command.  If loading
    //   hasn't finished yet by the time the print command arrives, we
    //   may print partial figures.]

    char c;
    const int i = read(fd, &c, 1);

    assert(i <= 0);

    if (i < 0) {
        message(
            Operation::WARNING,
            make_error_string("could not wait for remote end"));
    }

    close(fd);
}
#else
void Inspect_operation::evaluate()
{
    message(
        Operation::ERROR, "this operation is not available on your platform");
}
#endif
