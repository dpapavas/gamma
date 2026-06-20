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

#include <filesystem>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Polygon_mesh_processing/compute_normal.h>

#include "kernel.h"
#include "sink_operations.h"

// Document: program

// # Output Operations

// The operations below accept one or more surface meshes^[Proper
// support for polygons should be implemented, but for now we simply
// convert them to meshes for output.] as operands and produce no
// result.  Instead, they output the aggregate of their operands,
// either to disk in one of several supported formats, or to the
// debugger.

// Since we're dealing with filesystem access, the following function
// annotates error messages with a description of the problem.

static const std::string make_error_string(const char *message)
{
    return (std::string(message)
            + std::string(" (")
            + std::make_error_code(std::errc(errno)).message()
            + std::string(")"));
}

// ## OFF File Output

// The OFF file format is used both for output to disk and for
// transmission to the debugger.

// The meshes to be output may have color information for all or some
// of their vertices or faces.  These are encoded in property maps of
// type `std::optional<CGAL::IO::Color>>`.  When vertex colors are
// present, we write a so-called COFF variant of the OFF format.

// Since in that case each vertex is required to have a color, we
// designate the RBGA tuple $(0, 0, 0, 0)$ as meaning "no color" and
// output this for vertices with no color value present in the
// property map.  In the Debugger, these are substituted with the
// default color.

// Face colors are optional, so for those, we simply don't write a
// color specification for faces that don't have colors.

static inline void write_off_color(std::ostream &s, const CGAL::IO::Color &c)
{
    s << " " << static_cast<int>(c.red())
      << " " << static_cast<int>(c.green())
      << " " << static_cast<int>(c.blue())
      << " " << static_cast<int>(c.alpha());
}

static void write_off(std::ostream &s, const Surface_mesh &mesh)
{
    const auto vertex_colors =
        mesh.property_map<Surface_mesh::Vertex_index,
                          std::optional<CGAL::IO::Color>>("v:color");

    const auto edge_colors =
        mesh.property_map<Surface_mesh::Edge_index,
                          std::optional<CGAL::IO::Color>>("e:color");

    const auto face_colors =
        mesh.property_map<Surface_mesh::Face_index,
                          std::optional<CGAL::IO::Color>>("f:color");

    // We start by writing the header and counts on two lines.

    if (vertex_colors) {
        s << "COFF\n";
    } else {
        s << "OFF\n";
    }

    // We transfer colored edges to the Debugger as if they were faces
    // of degree 2.  We therefore need to count the colored edges and
    // augment the face count accordingly.

    int m = 0;
    if (edge_colors) {
        for (auto e: mesh.edges()) {
            if ((*edge_colors)[e]) {
                m++;
            }
        }
    }

    s << mesh.number_of_vertices() << " "
      << mesh.number_of_faces() + m << " "
      << mesh.number_of_edges() << "\n"
      << std::setprecision(DBL_DECIMAL_DIG);

    // Next, we output the vertices.  Surface mesh indexes are
    // (seemingly) not necessarily contiguous and compact, so we need
    // to make our own and keep a mapping from the source mesh vertex
    // index to the output vertex index.

    std::unordered_map<Surface_mesh::Vertex_index, std::size_t> map;
    int n = 0;

    for(auto v: mesh.vertices()) {
        const Surface_mesh::Point& P = mesh.point(v);

        map[v] = n++;

        s << CGAL::to_double(P.x())
          << " " << CGAL::to_double(P.y())
          << " " << CGAL::to_double(P.z());

        if (vertex_colors) {
            const std::optional<CGAL::IO::Color> x = (*vertex_colors)[v];

            if (x.has_value()) {
                write_off_color(s, x.value());
            } else {
                write_off_color(s, CGAL::IO::Color(0, 0, 0, 0));
            }
        }

        s << "\n";
    }

    // Now we add the faces as lists of vertex indices, looked up from
    // the map.

    for (auto f: mesh.faces()) {
        s << mesh.degree(f);

        for(Surface_mesh::Vertex_index v:
                CGAL::vertices_around_face(mesh.halfedge(f), mesh)) {
            s << " " << map[v];
        }

        if (face_colors) {
            const std::optional<CGAL::IO::Color> x = (*face_colors)[f];
            if (x.has_value()) {
                write_off_color(s, x.value());
            }
        }

        s << "\n";
    }

    // Finally, we add any colored edges, as faces of degree 2.

    if (edge_colors) {
        for (auto e: mesh.edges()) {
            const std::optional<CGAL::IO::Color> x = (*edge_colors)[e];

            if (!x.has_value()) {
                continue;
            }

            const auto h = mesh.halfedge(e);
            s << 2 << " " << map[mesh.source(h)] << " " << map[mesh.target(h)];

            write_off_color(s, x.value());

            s << "\n";
        }
    }
}

// The function below accumulates all operands into a single mesh for
// output.

static void accumulate_operands(
    Surface_mesh &M, decltype(Write_operation::operands) &operands)
{
    // If any of our operands have colors, we need to add a
    // appropriate property maps to our aggregate mesh.

    for(auto p: operands) {
        if (p->get_value()->property_map<Surface_mesh::Vertex_index,
                                         std::optional<CGAL::IO::Color>>(
                                             "v:color")) {
            M.add_property_map<Surface_mesh::Vertex_index,
                               std::optional<CGAL::IO::Color>>("v:color");
            break;
        }
    }

    for(auto p: operands) {
        if (p->get_value()->property_map<Surface_mesh::Edge_index,
                                         std::optional<CGAL::IO::Color>>(
                                             "e:color")) {
            M.add_property_map<Surface_mesh::Edge_index,
                               std::optional<CGAL::IO::Color>>("e:color");
            break;
        }
    }

    for(auto p: operands) {
        if (p->get_value()->property_map<Surface_mesh::Face_index,
                                         std::optional<CGAL::IO::Color>>(
                                             "f:color")) {
            M.add_property_map<Surface_mesh::Face_index,
                               std::optional<CGAL::IO::Color>>("f:color");
            break;
        }
    }

    for(auto p: operands) {
        M += *p->get_value();
    }
}

// Writing an OFF file to disk is now a simple matter.

void Write_OFF_operation::evaluate()
{
    Surface_mesh M;
    accumulate_operands(M, operands);

    std::ofstream s;

    s.open(filename);

    if (!s) {
        message(
            Operation::ERROR, make_error_string("could not open output file"));
        return;
    }

    write_off(s, M);
}

// ## OFF Output to the Debugger

// This operation writes an output to a UNIX domain socket in OFF
// format.  Its intended use is to send geometry to the Debugger for
// inspection.

#if defined(__unix__) && defined(__GNUG__)
#include <ext/stdio_filebuf.h>
#include <sys/socket.h>
#include <sys/un.h>

// We need to:

void Inspect_operation::evaluate()
{
    //   1. create a socket,

    const int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd == -1) {
        message(Operation::WARNING, make_error_string("could not open socket"));
        return;
    }

    struct sockaddr_un addr = {};

    //   2. connect it to the remote end,

    addr.sun_family = AF_UNIX;

    if (connect(fd,
                (const struct sockaddr *)&addr,
                stpncpy(
                    addr.sun_path + 1, Options::ipc_address,
                    sizeof(addr.sun_path) - 2) - (char *)&addr) == -1) {
        message(
            Operation::WARNING, make_error_string("could not connect socket"));
        return;
    }
#undef NAME

    //   3. accumulate all operands into a single mesh, as we did for
    //   `Write_OFF_operation`,

    Surface_mesh M;
    accumulate_operands(M, operands);

    //   4. create an output stream that will write to the socket's
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
    //   that executes commands from the socket on the Debugger's end
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

// ## WRL File Output

// This function adds preliminary support for output in the VRML 2.0
// WRL file format.

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

// ## STL File Output

// This function adds preliminary support for output in the STL file
// format.

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

    s << "solid " << std::filesystem::path(filename).stem().string() << "\n"
      << std::setprecision(DBL_DECIMAL_DIG);

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
