#ifndef SINK_OPERATIONS_H
#define SINK_OPERATIONS_H

#include "basic_operations.h"
#include "polyhedron_operations.h"

class Write_operation:
    public Nary_operation<Polyhedron_operation<Surface_mesh>, Sink_operation> {

protected:
    const std::string filename;

public:
    Write_operation(
        const char *s,
        std::vector<std::shared_ptr<Polyhedron_operation<Surface_mesh>>> &&v):
        Nary_operation<Polyhedron_operation<Surface_mesh>, Sink_operation>(
            std::move(v)), filename(s) {}
};

class Write_OFF_operation: public Write_operation {
public:
    using Write_operation::Write_operation;

    std::string describe() const override {
        return compose_tag("write_off", filename.c_str(), operands);
    }

    void evaluate() override;
};

class Write_STL_operation: public Write_operation {
public:
    using Write_operation::Write_operation;

    std::string describe() const override {
        return compose_tag("write_stl", filename.c_str(), operands);
    }

    void evaluate() override;
};

class Write_WRL_operation: public Write_operation {
public:
    using Write_operation::Write_operation;

    std::string describe() const override {
        return compose_tag("write_wrl", filename.c_str(), operands);
    }

    void evaluate() override;
};

class Pipe_to_geomview_operation: public Write_operation {
public:
    using Write_operation::Write_operation;

    std::string describe() const override {
        if (filename.empty()) {
            return compose_tag("pipe", operands);
        } else {
            return compose_tag("pipe", filename.c_str(), operands);
        }
    }

    void evaluate() override;
};

#endif
