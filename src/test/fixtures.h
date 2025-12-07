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

#ifndef FIXTURES_H
#define FIXTURES_H

#include "basic_operations.h"

// --- program

// # Testing Evaluation Results

// In order to test the results of one or more operations, we first
// need to make sure they get evaluated.  Since only output operations
// and their ancestry remain after the cull (ref: Culling Dead
// Operations), we introduce a special operation, which we attach to
// the operation under test and sink manually.

// Apart for ensuring evaluation, this operation also captures
// information about the operation under test, which we later inspect
// to determine whether everything went according to plan.

template<template<typename> typename OP, typename T>
class Test_sink_operation:
    public Unary_operation<OP<T>, Operation> {
public:
    std::string &tag_ref;
    std::unordered_map<std::string, std::string> &annotations_ref;
    std::unordered_set<std::string> &operations_ref;
    std::shared_ptr<T> &result_ref;

    Test_sink_operation(
        const std::shared_ptr<OP<T>> &x,
        std::string &s,
        std::unordered_map<std::string, std::string> &m,
        std::unordered_set<std::string> &o,
        std::shared_ptr<T> &p):
        Unary_operation<OP<T>, Operation>(x),
        tag_ref(s), annotations_ref(m), operations_ref(o), result_ref(p) {}

    std::string describe() const override {
        // We traverse the graph to assemble the set of all tags, just
        // before they are evaluated.  It is admittedly opportunistic
        // to put this here, but tagging this sink operation, whether
        // during construction, or after rewriting the graph, *is*
        // guaranteed to happen after all other operations have been
        // tagged.  On the other hand, placing it in the constructor
        // would not capture any rewrites and putting it in `evaluate`
        // below, would only capture the tag of our immediate
        // predecessor, ass the rest of the graph would have been
        // destroyed by then.

        operations_ref.clear();

        auto visit = [this](const Operation *op, auto &&visit) {
            if (operations_ref.find(op->tag) != operations_ref.end()) {
                return;
            }

            for (Operation *x: op->predecessors) {
                visit(x, visit);
            }

            operations_ref.insert(op->tag);
        };

        visit(this, visit);

        return compose_tag("test_operation", this->operand);
    }

    void evaluate() override {
        // Evaluation of this operation simply consists in capturing
        // information about the operation under test.

        tag_ref = this->operand->tag;
        annotations_ref = this->operand->annotations;
        result_ref = this->operand->get_value();
    }
};

// ## Test Fixtures

// We frequently need to set flags and options when evaluating tests.
// The following helper functions allow us to do this and to restore
// everything when we're done, so that settings for one test don't
// affect any tests that follow.

struct Main_fixture {
private:
    template<typename T>
    inline static std::unordered_map<T *, T> pushed_values;

public:
    template<typename T>
    void push(T &what) {
        assert(pushed_values<T>.find(&what) == pushed_values<T>.end());
        pushed_values<T>[&what] = what;
    }

    template<typename T>
    void push(T &what, T x) {
        push(what);
        what = x;
    }

    template<typename T>
    void pop(T &what) {
        what = pushed_values<T>.at(&what);
        pushed_values<T>.erase(&what);
    }
};

// This fixture facilitates testing of operation evaluation, through
// the `evaluate` member function.  The tests usually consist of
// checking the resulting geometry for the expected number of
// vertices, edges and faces, or by other, more constructive methods.

// A typical test would look like this:

// ```
// BOOST_AUTO_TEST_CASE(cuboid)
// @{
//     const auto &result = evaluate(...);
//     evaluate_operations();
//     BOOST_TEST(result.tag == "...");
//     test_polyhedron(*result.value, ...);
// @}
// ```

struct Evaluation_fixture: public Main_fixture {
public:
    // We ensure some sane default options below, so that we don't
    // have to set them in every test.

    Evaluation_fixture() {
        push(Tolerances::projection, FT(FT::ET(1, 1'000'000)));
        push(Tolerances::curve, FT(FT::ET(1, 1000)));
    }

    ~Evaluation_fixture() {
        pop(Tolerances::projection);
        pop(Tolerances::curve);
    }

    // This is a simple function that will assemble an expected tag
    // string from fragments.  A typical use would look like:

    // ```
    // BOOST_TEST(result.tag == tag("rectangle(", w, ", ", h));
    // ```

    template<typename... Args>
    std::string tag(Args... args) {
        std::ostringstream s;
        (s << ... << args);
        return s.str();
    }

    // The operation under test is passed to the following function,
    // where it is linked under a special sink operation, which
    // captures evaluation information and returns it to the test for
    // inspection.

    template<template<typename> typename OP, typename T>
    auto evaluate(const std::shared_ptr<OP<T>> &op) {
        struct {
            std::string tag;
            std::unordered_map<std::string, std::string> annotations;
            std::unordered_set<std::string> operations;
            std::shared_ptr<T> value;
        } r;

        sink_operation(
            map_operation(
                make_operation<Test_sink_operation<OP, T>>(
                    op, r.tag, r.annotations, r.operations, r.value)));

        return r;
    }

    template<typename F, typename = std::enable_if_t<std::is_invocable_v<F>>>
    auto evaluate(F &&f) {
        return evaluate(std::invoke(std::forward<F>(f)));
    }
};

// Some tests take to long to evaluate with the tolerances set above,
// so we derive the following simple fixture.

struct Coarse_evaluation_fixture: public Evaluation_fixture {
    Coarse_evaluation_fixture() {
        Tolerances::curve = FT::ET(1, 100);
    }
};

// --- program

#endif
