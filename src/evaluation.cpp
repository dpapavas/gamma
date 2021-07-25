#include <cstring>
#include <chrono>
#include <list>
#include <iostream>
#include <fstream>

#include <thread>
#include <mutex>
#include <condition_variable>

#include "assertions.h"
#include "options.h"
#include "basic_operations.h"
#include "kernel.h"

namespace {
    // Debugging information dump streams.

    std::mutex dump_mutex;
    std::ostream operations_dump(nullptr), log_dump(nullptr),
        graph_dump(nullptr);

    std::unordered_map<Operation *, std::string> tags;
    int evaluation_sequence;
    std::chrono::time_point<std::chrono::steady_clock> evaluation_start;

    inline float evaluation_timestamp()
    {
        return std::chrono::duration_cast<
            std::chrono::duration<float>>(
                std::chrono::steady_clock::now() - evaluation_start).count();
    }

    inline std::string maybe_shortened_tag(std::string t)
    {
        return (Options::dump_short_tags < 0
                || static_cast<int>(t.size()) < Options::dump_short_tags
                ? t : (t.substr(0, Options::dump_short_tags)
                       + "..."));
    }

    // Ready queue

    std::list<Operation *> ready[2];
    std::recursive_mutex ready_mutex;
    std::condition_variable_any ready_condition;

    inline void ready_operation(Operation *op)
    {
        const Threadsafe_operation *p;

        ready[Options::threads > 0
              && (p = dynamic_cast<Threadsafe_operation *>(op))
              && p->threadsafe].push_front(op);
    }

    bool had_failure = false;

    bool try_dispatch_operation(Operation *op)
    {
        bool failed = true;
        std::ostringstream s;

        try {
            try {
                failed = op->dispatch();
            } catch (const CGAL::Warning_exception &e) {
                s << "CGAL warning"; throw;
            } catch (const CGAL::Error_exception &e) {
                s << "CGAL error"; throw;
            } catch (const CGAL::Precondition_exception &e) {
                s << "CGAL precondition violation"; throw;
            } catch (const CGAL::Postcondition_exception &e) {
                s << "CGAL postcondition violation"; throw;
            } catch (const CGAL::Assertion_exception &e) {
                s << "CGAL assertion violation"; throw;
            } catch(const operation_warning_error &e) {
                op->message(Operation::ERROR, e.what());
            }
        } catch(const CGAL::Failure_exception &e) {
            std::string t;

            if (const std::string &p = e.expression(); !p.empty()) {
                s << " '" << p << "'";
            }

            if (const std::string &m = e.message(); !m.empty()) {
                s << " (" << m << ")";
            }

            op->message(Operation::ERROR, "evaluation of % failed due to a " + s.str());

            {
                std::ostringstream t;

                t << e.filename() << ": " << std::to_string(e.line_number())
                  << ": " << s.str();

                op->annotations.insert({"failure", t.str()});
            }
        } catch (const std::exception &e) {
            s << " (" << e.what() << ")";

            op->message(
                Operation::ERROR, "evaluation of % failed due to an exception" + s.str());
        }

        return failed;
    }

    void dispatch_operation(Operation *op)
    {
        bool failed;

        if (Options::dump_graph
            || Options::dump_operations
            || Options::dump_log) {

            // Assign an evaluation sequence number to the operation
            // and find its tag.

            dump_mutex.lock();

            auto it = tags.find(op);
            const std::string &k = op->get_tag();
            const std::string &l = (it == tags.end() ? k : it->second);
            int n = evaluation_sequence++;

            // Replace tags of dependencies by their evaluation
            // identifier for clarity. Replace in reverse order to
            // ensure proper replacement, otherwise we would end up
            // with
            //
            // $0 = rectangle(...)
            // $1 = extrusion($0,...)
            // $2 = mesh(extrusion($0,...))
            //
            // instead of
            //
            // $2 = mesh($1)

            if (Flags::dump_abridged_tags) {
                for (Operation *x: op->successors) {
                    if (!x->selected) {
                        continue;
                    }

                    std::string &r = tags.insert({x, x->get_tag()}).first->second;
                    const std::string s = std::string("$") + std::to_string(n);

                    for (size_t i = r.find(k);
                         i != std::string::npos;
                         i = r.find(k, i)) {
                        r.replace(i, k.size(), s);
                    };
                }
            }

            dump_mutex.unlock();

            // Output pre-evaluation dumps.

            if (Options::dump_log) {
                std::lock_guard<std::mutex> lock(dump_mutex);

                log_dump << evaluation_timestamp() << ": $" << n << " = "
                         << maybe_shortened_tag(l) << " started" << std::endl;
            }

            if (Options::dump_operations) {
                std::lock_guard<std::mutex> lock(dump_mutex);

                operations_dump << "$" << n << " = " << maybe_shortened_tag(l);
                operations_dump.flush();
            }

            // Evaluate.

            failed = try_dispatch_operation(op);

            if (failed) {
                op->annotations.insert({"failed", std::string()});
            }

            // Output post-evaluation dumps.

            if (Options::dump_log) {
                std::lock_guard<std::mutex> lock(dump_mutex);

                log_dump << evaluation_timestamp()
                         << ": $" << n
                         << (failed ? " failed" : " concluded")
                         << std::endl;
            }

            if (Options::dump_operations) {
                std::lock_guard<std::mutex> lock(dump_mutex);

                if (Flags::dump_annotations && op->annotations.size() > 0) {
                    operations_dump << " (";

                    for (auto it = op->annotations.cbegin(); ;) {
                        operations_dump << it->first;

                        if (!it->second.empty()) {
                            operations_dump << ": " << it->second;
                        }

                        if (++it == op->annotations.cend()) {
                            break;
                        }

                        operations_dump << ", ";
                    }

                    operations_dump << ")";
                }

                operations_dump << std::endl;
            }

            // Output Graphivz dot source for the evaluation graph.

            if (Options::dump_graph) {
                std::string r = maybe_shortened_tag(l);

                // Escape double quotes.

                for (size_t p = r.find('"');
                     p != std::string::npos;
                     p = r.find('"', p + 2)) {
                    r.replace(p, 1, "\\\"");
                };

                // Add the node.

                std::lock_guard<std::mutex> lock(dump_mutex);

                graph_dump << "\"" << op->digest() << "\" "
                           << "[label=\"<head>$" << n << "|";

                if (Flags::dump_annotations && op->annotations.size() > 0) {
                    graph_dump << "{" << r << "|";

                    for (auto it = op->annotations.cbegin(); ;) {
                        graph_dump << it->first;

                        if (!it->second.empty()) {
                            graph_dump << ": " << it->second;
                        }

                        if (++it == op->annotations.cend()) {
                            break;
                        }

                        graph_dump << ", ";
                    }

                    graph_dump << "\\l}";
                } else {
                    graph_dump << r;
                }

                graph_dump << "\"]" << std::endl;

                // Add its edges.

                if (op->successors.size() > 0) {

                    graph_dump << "\"" << op->digest() << "\":head -> {";

                    for (Operation *x: op->successors) {
                        if (!x->selected) {
                            continue;
                        }

                        graph_dump << "\"" << x->digest() << "\" ";
                    }

                    graph_dump << "}\n" << std::endl;
                }
            }
        } else {
            failed = try_dispatch_operation(op);
        }

        // Update the successors and ready list.

        {
            std::lock_guard<std::recursive_mutex> lock(ready_mutex);

            // We use the ready mutex to guard against multiple
            // workers racing to update the same successor and/or
            // the ready queue.

            if (!failed) {
                if (Flags::warn_unused
                    && op->successors.empty()
                    && !dynamic_cast<Sink_operation *>(op)) {
                    op->message(
                        Operation::WARNING,
                        "operation % instantiated but not used");
                }

                for (Operation *x: op->successors) {
                    if (!x->selected) {
                        continue;
                    }

                    x->cost += op->cost;
                    x->predecessors.erase(op);

                    if (x->predecessors.empty()) {
                        ready_operation(x);

                        if (Options::dump_log) {
                            std::lock_guard<std::mutex> lock(dump_mutex);
                            auto it = tags.find(x);
                            const std::string &m =
                                (it == tags.end() ? x->get_tag() : it->second);

                            log_dump << evaluation_timestamp()
                                     << ": " << maybe_shortened_tag(m)
                                     << " ready" << std::endl;
                        }
                    }
                }
            } else {
                had_failure = true;
            }
        }
    }

    class Worker {
        int index;
        std::thread thread;

        // These guard transitions of 'pending', i.e. work assignment and
        // conclusion.

        std::mutex pending_mutex;
        std::condition_variable pending_condition;

        Operation *pending;         // Work under, or pending for, evaluation.
        bool draining;              // Worker is winding down.

        void work();

    public:
        Worker(int i): index(i), pending(nullptr), draining(false) {};

        void start() {
            thread = std::thread(&Worker::work, this);
        }

        void stop();
        bool evaluate(Operation *op);
    };
}

void Worker::stop()
{
    assert (thread.joinable());

    {
        std::lock_guard<std::mutex> lock(pending_mutex);
        draining = true;
        pending_condition.notify_one();
    }

    thread.join();
}

bool Worker::evaluate(Operation *op)
{
    std::lock_guard<std::mutex> lock(pending_mutex);

    if (pending) {
        return false;
    }

    assert(!pending);
    if (op) {
        pending = op;
        pending_condition.notify_one();
    }

    return true;
}

void Worker::work()
{
    std::unique_lock<std::mutex> lock(pending_mutex);

    while (true) {
        while (!(draining || pending)) {
            pending_condition.wait(lock);
        }

        if (draining) {
            break;
        }

        pending->annotations.insert({"thread", std::to_string(index)});

        lock.unlock();
        dispatch_operation(pending);
        lock.lock();

        pending = nullptr;
        ready_condition.notify_one();
    }
}

////////////////
// Operations //
////////////////

#include "polygon_operations.h"
#include "polyhedron_operations.h"
#include "circle_polygon_types.h"
#include "conic_polygon_types.h"
#include "evaluation.h"

static std::unordered_map<std::string, std::shared_ptr<Operation>> operations;
static const char *unit_name;

std::unordered_map<std::string, std::shared_ptr<Operation>> &_get_operations()
{
    return operations;
}

template<typename T>
static bool try_fold(Operation *op)
{
    auto t = dynamic_cast<T *>(op);
    return t && t->try_fold();
}

static void select_operation(Operation *op)
{
    if (op->selected) {
        return;
    }

    op->select();

    if (Flags::eliminate_dead_operations && op->loadable) {
        // Abridge the to-be-loaded operation's tag here, as it won't
        // happen during evaluation (since its predecessors will
        // never be evaluated).

        if (Flags::dump_abridged_tags) {
            std::lock_guard<std::mutex> lock(dump_mutex);
            std::string &r = tags.insert({op, op->get_tag()}).first->second;

            for (Operation *x: op->predecessors) {
                const std::string &k = x->get_tag();

                for (size_t i = r.find(k);
                     i != std::string::npos;
                     i = r.find(k, i)) {
                    r.replace(i, k.size(), "$");
                };
            }
        }

        // Modify the graph to turn the operation into a source
        // operation, to be placed on the ready list.

        for (Operation *x: op->predecessors) {
            x->successors.erase(op);
        }

        op->predecessors.clear();
    }

    if (op->predecessors.empty()) {
        ready_operation(op);
    } else {
        for (Operation *x: op->predecessors) {
            select_operation(x);
        }
    }
}

static void rewrite_operations()
{
    for (int n = 0; (Options::rewrite_pass_limit < 0
                     || n < Options::rewrite_pass_limit) ; n++) {

        bool p = false;
        for (auto &[k, x]: operations) {
            if ((Flags::fold_transformations
                 // Fold 2D transformations.

                 && (try_fold<Polygon_transform_operation<Polygon_set>>(x.get())
                     || try_fold<Polygon_transform_operation<Circle_polygon_set>>(x.get())
                     || try_fold<Polygon_transform_operation<Conic_polygon_set>>(x.get())

                     // Fold 3D transformation.

                     || try_fold<Polyhedron_transform_operation<Polyhedron>>(x.get())
                     || try_fold<Polyhedron_transform_operation<Nef_polyhedron>>(x.get())
                     || try_fold<Polyhedron_transform_operation<Surface_mesh>>(x.get())))

                ||

                (Flags::fold_flushes
                 // Fold polygon flushing.

                 && (try_fold<Polygon_flush_operation>(x.get())

                     // Fold polyhedron flushing.

                     || try_fold<Polyhedron_flush_operation<Polyhedron>>(x.get())
                     || try_fold<Polyhedron_flush_operation<Nef_polyhedron>>(x.get())
                     || try_fold<Polyhedron_flush_operation<Surface_mesh>>(x.get())))

                ||

                (Flags::fold_booleans
                 // Fold polyhedron joins.

                 && (try_fold<Polyhedron_join_operation<Polyhedron>>(x.get())
                     || try_fold<Polyhedron_join_operation<Nef_polyhedron>>(x.get())
                     || try_fold<Polyhedron_join_operation<Surface_mesh>>(x.get())

                     // Fold polyhedron differences.

                     || try_fold<Polyhedron_difference_operation<Polyhedron>>(x.get())
                     || try_fold<Polyhedron_difference_operation<Nef_polyhedron>>(x.get())
                     || try_fold<Polyhedron_difference_operation<Surface_mesh>>(x.get())

                     // Fold polyhedron intersections.

                     || try_fold<Polyhedron_intersection_operation<Polyhedron>>(x.get())
                     || try_fold<Polyhedron_intersection_operation<Nef_polyhedron>>(x.get())
                     || try_fold<Polyhedron_intersection_operation<Surface_mesh>>(x.get())

                     // Fold polygon joins.

                     || try_fold<Polygon_join_operation<Polygon_set>>(x.get())
                     || try_fold<Polygon_join_operation<Circle_polygon_set>>(x.get())
                     || try_fold<Polygon_join_operation<Conic_polygon_set>>(x.get())

                     // Fold polygon differences.

                     || try_fold<Polygon_difference_operation<Polygon_set>>(x.get())
                     || try_fold<Polygon_difference_operation<Circle_polygon_set>>(x.get())
                     || try_fold<Polygon_difference_operation<Conic_polygon_set>>(x.get())

                     // Fold polygon intersections.

                     || try_fold<Polygon_intersection_operation<Polygon_set>>(x.get())
                     || try_fold<Polygon_intersection_operation<Circle_polygon_set>>(x.get())
                     || try_fold<Polygon_intersection_operation<Conic_polygon_set>>(x.get())))) {
                p = true;
                break;
            }
        }

        if (!p) {
            break;
        }
    }
}

void begin_unit(const char *name)
{
    operations.clear();
    tags.clear();

    for (auto &r: ready) {
        r.clear();
    }

    had_failure = false;
    evaluation_sequence = 0;
    unit_name = name;
}

void evaluate_unit()
{
    // Open any dump files.

#define SET_UP_DUMP_STREAM(WHAT, EXT)                                   \
    std::filebuf WHAT ##_dump_filebuf;                                  \
                                                                        \
    if (Options::dump_## WHAT) {                                        \
        if (!std::strcmp(Options::dump_## WHAT, "-")) {                 \
            WHAT ##_dump.rdbuf(std::cout.rdbuf());                      \
        } else if (!std::strcmp(Options::dump_## WHAT, "")) {           \
            WHAT ##_dump_filebuf.open(                                  \
                std::string(unit_name ? unit_name : "a") + EXT,         \
                std::ios::out);                                         \
                                                                        \
            WHAT ##_dump.rdbuf(&WHAT ##_dump_filebuf);                  \
        } else {                                                        \
            WHAT ##_dump_filebuf.open(                                  \
                Options::dump_## WHAT, std::ios::out);                  \
            WHAT ##_dump.rdbuf(&WHAT ##_dump_filebuf);                  \
        }                                                               \
                                                                        \
        WHAT ##_dump.precision(3);                                      \
    }

    SET_UP_DUMP_STREAM(operations, ".list");
    SET_UP_DUMP_STREAM(log, ".log");
    SET_UP_DUMP_STREAM(graph, ".dot");

    if (Options::dump_graph) {
        graph_dump << "digraph {\n"
                   << "node [shape=record]" << std::endl;
    }

#undef SET_UP_DUMP_STREAM

    // Rewrite and select operations.

    rewrite_operations();

    for (auto &[k, x]: operations) {
        if (Operation *p = x.get();
            !Flags::eliminate_dead_operations
            || dynamic_cast<Sink_operation *>(p)) {
            select_operation(p);
        }
    }

    // Start the evaluation.

    evaluation_start = std::chrono::steady_clock::now();

    // Avoid spawning any threads if single-threaded operation is
    // requested.

    if (Options::threads == 0) {
        assert(ready[1].empty());
        while (!ready[0].empty() && (!had_failure || !Flags::warn_fatal_errors)) {
            dispatch_operation(ready[0].back());
            ready[0].pop_back();
        }
    } else {
        std::list<Worker> workers;

        for (int i = 0; i < Options::threads; i++) {
            workers.emplace_back(i);
        }

        for (Worker &w: workers) {
            w.start();
        }

        {
            std::unique_lock<std::recursive_mutex> lock(ready_mutex);

            while (!had_failure || !Flags::warn_fatal_errors) {
                int j = 0;

                // Allocate operations from the threadsafe ready queue
                // and count workers left idle.

                for (Worker &w: workers) {
                    if (!ready[1].empty()) {
                        if (w.evaluate(ready[1].back())) {
                            ready[1].pop_back();
                        }
                    } else {
                        j += w.evaluate(nullptr);
                    }
                }

                assert(j == 0 || ready[1].empty());

                // Dispatch thread-unsafe operations in the main
                // thread, when no thread-safe operations are
                // available.

                if (!ready[0].empty() && j > 0) {
                    dispatch_operation(ready[0].back());
                    ready[0].pop_back();
                }

                // If the ready queues are empty and all workers are
                // idle, we're done, otherwise we're either stalled,
                // or saturated and we need to wait for some more
                // workers to finish.

                const bool p = ready[1].empty() && ready[0].empty();

                if (p && j == Options::threads) {
                    break;
                }

                if (p || j == 0) {
                    ready_condition.wait(lock);
                }
            }
        }

        for (Worker &w: workers) {
            w.stop();
        }
    }

    if (Options::dump_graph) {
        graph_dump << "}" << std::endl;
    }

    operations_dump.rdbuf(nullptr);
    log_dump.rdbuf(nullptr);
    graph_dump.rdbuf(nullptr);
}

std::shared_ptr<Operation> find_operation(const std::string &k)
{
    auto p = operations.find(k);

    if (p != operations.end()) {
        return p->second;
    }

    return std::shared_ptr<Operation>(nullptr);
}

void rehash_operation(const std::string &k)
{
    auto n = operations.extract(k);
    assert(n);
    n.key() = n.mapped()->get_tag();
    operations.insert(std::move(n));
}

void insert_operation(const std::shared_ptr<Operation> p)
{
    // We should always insert at this point.

    safely_assert(operations.insert({p->get_tag(), p}).second);
}

bool erase_operation(const Operation *p)
{
    return operations.erase(p->get_tag()) > 0;
}
