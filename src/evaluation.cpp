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
#include "rewrites.h"
#include "basic_operations.h"
#include "kernel.h"

// ---

// # Operation Evaluation

// Operation evaluation follows the execution of front end code (ref:
// The Front End), as a result of which a set of operations will have
// been instantiated, forming a graph which describes their
// interdependencies (ref: The Evaluation Graph).  Our aim is then to
// evaluate this graph, meaning to evaluate the operations in a
// suitable order, so as to calculate the geometry desired by the
// user.

// Before proceeding with this evaluation though, it can pay off to
// manipulate the graph first.  This can be beneficial for several
// reasons:

//   1. The code evaluated by the front end can, and generally will,
//   specify several outputs but most of the time, few of them --
//   typically only one -- will be enabled (ref: Outputs).  We can
//   therefore restrict ourselves to a subgraph of the initial graph,
//   discarding all other operations.  Ref: Sunk Operations.

//   2. We might be able to rewrite the graph in such a way that the
//   end result is the same, but the computation is more convenient
//   for some reason.  Ref: Graph Rewriting.

// ## Global Evaluation State

// When enabled, we output certain diagnostic information during
// evaluation.

//   * The *operations list* is a listing showing the evaluated
//   operations, in order.  For example:

//   ```
//   $0 = sphere(1,1/15,1/1000000) (facets: 80, halfedges: 240, vertices: 42)
//   $1 = transform($0,translation(1,0,0)) (facets: 80, halfedges: 240, ...
//   $2 = mesh($1) (facets: 80, edges: 120, halfedges: 240, vertices: 42)
//   $3 = pipe($2)
//   ```

//   * The *operations log* is a more detailed log of the evaluation
//   process, showing events with timing information.  For example:

//   ```
//   1.97e-05: rewrote 0 out of 4 operations_list
//   7.1e-05: $0 = sphere(1,1/15,1/1000000) started
//   0.00201: $0 concluded
//   0.00201: transform($0,translation(1,0,0)) ready
//   0.00201: $1 = transform($0,translation(1,0,0)) started
//   0.00213: $1 concluded
//   0.00214: mesh($1) ready
//   0.00214: $2 = mesh($1) started
//   0.00235: $2 concluded
//   0.00236: pipe($2) ready
//   0.00236: $3 = pipe($2) started
//   0.00241: $3 concluded
//   ```

//   * The *operations graph* is a drawing of the evaluation graph, in
//   the form of Graphviz code.

// The streams used to output the above are define below, along with
// the mutex used to serialize access to those streams during
// multi-threaded evaluation.

static std::mutex dump_mutex;
static std::ostream list_dump(nullptr), log_dump(nullptr),
    graph_dump(nullptr);

// We also define some variables and utility functions for diagnostic
// output.

//   tags := is used to print tags in clearer form; ref: tag abridging.

//   evaluation_sequence := is used to allocate serial numbers for the
//   operation (the numbers following the `$` sign in the operations
//   list).

//   evaluation_start := is used to measure timing for the operations
//   log.

static std::unordered_map<Operation *, std::string> tags;
static int evaluation_sequence;
static std::chrono::time_point<std::chrono::steady_clock> evaluation_start;

static inline float evaluation_timestamp()
{
    return std::chrono::duration_cast<
        std::chrono::duration<float>>(
            std::chrono::steady_clock::now() - evaluation_start).count();
}

static inline std::string maybe_shortened_tag(std::string t)
{
    return (Options::dump_short_tags < 0
            || static_cast<int>(t.size()) < Options::dump_short_tags
            ? t : (t.substr(0, Options::dump_short_tags)
                   + "..."));
}

// The main structures are the following:

//   anchor: ready list
//   ready_list[] := hold pointers to operations that are ready for
//   evaluation, i.e. source operations or operations whose
//   predecessors have already been evaluated.  We keep two lists, the
//   first holding operations that will be evaluated in the main
//   thread (either because they're not thread-safe, or because
//   threading has not been enabled at all), while operations that
//   will be evaluated in worker threads are put in the second.

//   anchor: locked set
//   locked_set := keeps track of operands being accessed by operations
//   currently undergoing evaluation. CGAL's documentation states that
//   "it should be possible to use different objects in different
//   threads at the same time (of the same type or not)", but that "it
//   is not safe to access the same object from different threads at
//   the same time".  Therefore, before accepting an operation for
//   multi-threaded evaluation, we check that none of its predecessors
//   are in this set.

static std::list<Operation *> ready_list[2];
static std::unordered_set<Operation *> locked_set;

static void ready_operation(Operation *op)
{
    const Threadsafe_operation *p;

    if (Options::threads > 0
        && (p = dynamic_cast<Threadsafe_operation *>(op))
        && p->threadsafe) {
        ready_list[1].push_front(op);
    } else {
        ready_list[0].push_front(op);
    }
}

// We also define the necessary synchronization primitives and utility
// functions here:

//   ready_mutex := protects access to the ready lists.  It also
//   potects access to parts of the operation.  For instance, after
//   evaluation the operation is *reset* so that it no longer keeps
//   references to its operands.  This may cause these to be
//   destroyed, along with their graph edges
//   (i.e. successor/predecessor pointers).  This can of course happen
//   on multiple threads concurrently.

//   ready_condition := allows a worker to block when there is no more
//   work it can currently do.  Notifying this condition variable when
//   new operations are put on the ready lists allows these workers to
//   resume work.

static std::mutex ready_mutex;
static std::condition_variable_any ready_condition;

// ## Evaluation Workers

// An operation that is to be evaluated is *dispatched*.  In essence
// this boils down to calling the operation's `dispatch` member
// function, but `try_dispatch_operation` wraps this in an exception
// handler that handles potential failures.  These mostly arise inside
// CGAL code, but we occasionally also throw our own exceptions.

static bool had_failure = false;

static bool try_dispatch_operation(Operation *op)
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
            std::lock_guard<std::mutex> lock(dump_mutex);
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

        {
            std::ostringstream t;

            t << e.filename() << ": " << std::to_string(e.line_number())
              << ": " << s.str();

            op->annotations.insert({"failure", t.str()});
        }

        std::lock_guard<std::mutex> lock(dump_mutex);

        op->message(
            Operation::ERROR, "evaluation of % failed due to a " + s.str());
    } catch (const std::exception &e) {
        s << " (" << e.what() << ")";

        std::lock_guard<std::mutex> lock(dump_mutex);

        op->message(
            Operation::ERROR,
            "evaluation of % failed due to an exception" + s.str());
    }

    if (failed) {
        op->annotations.insert({"failed", std::string()});
    }

    return failed;
}

// This simple class encapsulates a thread of evaluating operations.
// It can be the main thread (with `index` 0), or a separate thread
// working concurrently with it (with positive `index`).

class Worker {
    std::list<Operation *> &ready_list_ref;
    const int index;

    inline static int working;
    std::thread thread;

public:
    Worker(int i): ready_list_ref(ready_list[i > 0]), index(i) {
        if (i > 0) {
            thread = std::thread(&Worker::work, this);
        }
    }

    ~Worker() {
        if (index > 0) {
            assert (thread.joinable());
            thread.join();
        }
    }

    void work();
};

void Worker::work()
{
    // This is the main worker loop.  On each iteration, we

    //   1. look for an operation we can evaluate,
    //   2. dispatch it, and
    //   3. update the graph and ready lists.

    // Phases 1 and 3 above may race with other workers so they're
    // protected by locking the ready mutex.

    std::unique_lock<std::mutex> lock(ready_mutex);

    while (true) {
        // First, we need to find a ready operation, that doesn't
        // refer to any operands that are locked (ref: locked set).

        auto it = std::find_if(
            ready_list_ref.begin(), ready_list_ref.end(),
            [](Operation *op) {
                return Options::threads == 0
                    || std::all_of(
                        op->predecessors.begin(),
                        op->predecessors.end(),
                        [](const auto &x){
                            return locked_set.find(x) == locked_set.end();
                        });
            });

        // If none was found, there are two possibilities:

        //   1. We're done, which means that both ready lists should
        //   be empty, and no work is in progress that might add more
        //   operations when done.  (We can't determine completion by
        //   looking at whether all sunk operations have been
        //   evaluated, because that will not happen if there are
        //   evaluation failures.)

        //   2. We're stalled, because no more operations can be run
        //   concurrently.  We block by waiting on the
        //   `ready_condition` variable, to be notified when more
        //   operations are put on the ready list.

        if (it == ready_list_ref.end()) {
            if (ready_list[0].empty() && ready_list[1].empty() && !working) {
                break;
            }

            ready_condition.wait(lock);
            continue;
        }

        Operation *op = *it;

        // At this point we have found an operation to evaluate.  We
        // increment the `working` counter to signal that we took on
        // work.  Although it keeps track of exactly how many of our
        // workers are busy, we only need it as a boolean, when
        // determining whethert to terminate.

        working++;

        // Before unlocking the mutex, we take the operation off the
        // ready list and lock its operands.

        ready_list_ref.erase(it);

        if (Options::threads > 0) {
            for (auto x: op->predecessors) {
                safely_assert(locked_set.insert(x).second);
            }
        }

        lock.unlock();

        // At this point we can be sure that we have the operation and
        // its operands to ourselves, so holding the mutex is no
        // longer necessary.  We can now dispatch the operation, which
        // essentially boils down to calling `try_dispatch_operation`
        // above to do so.  We also print out diagnostic information
        // to any enabled streams.

        bool failed;

        op->annotations.insert({"thread", std::to_string(index)});

        if (Options::dump_graph
            || Options::dump_list
            || Options::dump_log) {

            // First, we assign an evaluation sequence number to the
            // operation and find its tag.

            dump_mutex.lock();

            auto it = tags.find(op);
            const std::string &k = op->tag;
            const std::string &l = (it == tags.end() ? k : it->second);
            int n = evaluation_sequence++;

            // anchor: tag abridging

            // We replace occurences of our tag in the tag of successor
            // nodes by our evaluation number for clarity.  That is, we
            // turn something like

            // ```
            // $0 = sphere(1)
            // $1 = cuboid(1,1,1)
            // $2 = join(sphere(1),cuboid(1,1,1))
            // ```

            // into

            // ```
            // $0 = sphere(1)
            // $1 = cuboid(1,1,1)
            // $2 = joint($0,$1)
            // ```

            if (Flags::dump_abridged_tags) {
                for (Operation *x: op->successors) {
                    auto &r = tags.insert({x, x->tag}).first->second;
                    const std::string s =
                        std::string("$") + std::to_string(n);

                    for (std::size_t i = r.find(k);
                         i != std::string::npos;
                         i = r.find(k, i)) {
                        r.replace(i, k.size(), s);
                    };
                }
            }

            dump_mutex.unlock();

            // Next we output a message noting the start of
            // evaluation of the operation into the
            // operatonions log, if it has been enabled.

            if (Options::dump_log) {
                std::lock_guard<std::mutex> lock(dump_mutex);

                log_dump << evaluation_timestamp()
                         << ": $" << n << " = "
                         << maybe_shortened_tag(l)
                         << " started" << std::endl;
            }

            // We evaluate the operation, or at least try to, and output
            // post-evaluation messages, as necessary.

            failed = try_dispatch_operation(op);

            if (Options::dump_log) {
                std::lock_guard<std::mutex> lock(dump_mutex);

                log_dump << evaluation_timestamp()
                         << ": $" << n
                         << (failed ? " failed" : " concluded")
                         << std::endl;
            }

            if (Options::dump_list) {
                std::lock_guard<std::mutex> lock(dump_mutex);

                list_dump << "$" << n << " = "
                          << maybe_shortened_tag(l);

                if (Flags::dump_annotations
                    && op->annotations.size() > 0) {
                    list_dump << " (";

                    for (auto it = op->annotations.cbegin(); ;) {
                        list_dump << it->first;

                        if (!it->second.empty()) {
                            list_dump << ": " << it->second;
                        }

                        if (++it == op->annotations.cend()) {
                            break;
                        }

                        list_dump << ", ";
                    }

                    list_dump << ")";
                }

                list_dump << std::endl;
                list_dump.flush();
            }

            // Here we output Graphivz dot source for the
            // evaluation graph.

            if (Options::dump_graph) {
                std::string r = maybe_shortened_tag(l);

                // Any double quotes in the node's tag will need to be
                // escaped.

                for (std::size_t p = r.find('"');
                     p != std::string::npos;
                     p = r.find('"', p + 2)) {
                    r.replace(p, 1, "\\\"");
                };

                // First, we add the node, including its tag
                // and annotations.

                std::lock_guard<std::mutex> lock(dump_mutex);

                graph_dump << "\"" << op->tag_digest << "\" "
                           << "[label=\"<head>$" << n << "|";

                if (Flags::dump_annotations
                    && op->annotations.size() > 0) {
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

                // Then we add edges to its successors.

                if (op->successors.size() > 0) {
                    graph_dump << "\"" << op->tag_digest
                               << "\":head -> {";

                    for (Operation *x: op->successors) {
                        graph_dump << "\"" << x->tag_digest << "\" ";
                    }

                    graph_dump << "}\n" << std::endl;
                }
            }
        } else {
            // If no diagnostic output is enabled, we merely try to
            // evaluate the operation.

            failed = try_dispatch_operation(op);
        }

        if (Flags::warn_unused && op->successors.empty()) {
            std::lock_guard<std::mutex> lock(dump_mutex);

            op->message(
                Operation::WARNING,
                "operation % instantiated but not used");
        }

        lock.lock();

        // We're now done with evaluation and have relocked the mutex.
        // What follows is largely the reverse of the pre-evaluation
        // phase.

        working--;

        if (Options::threads > 0) {
            for (auto x: op->predecessors) {
                safely_assert(locked_set.erase(x) == 1);
            }
        }

        assert(op->selected);

        // We mark the operation as no longer requiring evaluation and
        // also reset the references to its operands, to allow them to
        // be destroyed as soon as they're no longer needed.

        op->selected = false;
        op->reset();

        had_failure = had_failure || failed;

        if (failed || (had_failure  && Flags::warn_fatal_errors)) {
            continue;
        }

        // Since the operation was successfully evaluated, we now go
        // through its successors and check if any of them are now
        // ready, i.e. if this was the last of their operands that
        // still needed to be evaluated.

        for (Operation *x: op->successors) {
            if (!x->selected) {
                continue;
            }

            x->cost += op->cost;

            if (std::any_of(
                    x->predecessors.begin(),
                    x->predecessors.end(),
                    [](const auto &y){
                        return y->selected;
                    })) {
                continue;
            }

            // This predecessory is ready; we put it on the ready list
            // and notify any blocked workers.  We also print
            // diagnositc information.

            ready_operation(x);
            ready_condition.notify_all();

            if (Options::dump_log) {
                std::lock_guard<std::mutex> lock(dump_mutex);
                auto it = tags.find(x);
                const std::string &m =
                    (it == tags.end() ? x->tag : it->second);

                log_dump << evaluation_timestamp()
                         << ": " << maybe_shortened_tag(m)
                         << " ready" << std::endl;
            }
        }
    }

    ready_condition.notify_one();
}

// ## Operation Digests

// Tag digests are simply SHA-1 digests of an operation's tag, used
// for storing and loading operations; ref: Operation Caching.

static std::uint32_t rotl32 (std::uint32_t x, unsigned int n) {
    return (x << n) | (x >> (32 - n));
}

static std::string digest_operation_tag(const std::string &tag)
{
    assert(!tag.empty());

    const std::uint8_t *s = reinterpret_cast<const std::uint8_t *>(tag.c_str());
    const std::size_t n = tag.size();

    std::uint8_t buffer[64];

    std::uint32_t h_0 = 0x67452301;
    std::uint32_t h_1 = 0xefcdab89;
    std::uint32_t h_2 = 0x98badcfe;
    std::uint32_t h_3 = 0x10325476;
    std::uint32_t h_4 = 0xc3d2e1f0;

    // We process the tag in 512-bit (64-byte) chunks.  We need to pad
    // the tag so that it is congruent to -64 (mod 512) bits, i.e. so
    // that it's a 8 bytes shy of being a multiple of 64 bytes in
    // length and then add the length as an 8-byte integer.

    // Instead of doing so before-hand, we start processing the tag in
    // chunks of 64 bytes and continue processing pad chunks when
    // we're through with the tag bits.

    for (std::size_t j = 0; j < ((n + 9 + 63) / 64) * 64; j += 64) {
        const std::uint8_t *p;

        if (j + 64 <= n) {
            // This is not the last chunk, so we need not pad.

            p = s + j;
        } else {
            std::size_t i = 0;

            // We need to pad.  We use a separate 64-byte buffer for
            // the purpose.

            p = buffer;

            // We copy the rest of message, if any, and add the
            // terminating 1 bit, as per standard.

            if (j <= n) {
                memcpy(buffer, s + j, (i = n - j));
                buffer[i++] = 0x80;
            }

            // Now we pad as needed with zeros and append the message
            // length as 64 bit big-endian if it fits.  If it doesn't,
            // we'll get in the next iteration, in a fresh 64 byte
            // chunk.

            if (i <= 56) {
                memset(buffer + i, 0, 56 - i);
                for (auto [k, m] = std::pair<int, std::uint64_t>(63, n * 8);
                     k >= 56;
                     k--, m >>= 8) {
                    buffer[k] = m & 0xff;
                }
            } else {
                memset(buffer + i, 0, 64 - i);
            }
        }

        // We have our 512-bit chunk so now we just process it as
        // required.

        std::uint32_t w[80];

        for (int i = 0; i < 16 ; i++) {
            const std::uint8_t *q = p + i * 4;
            w[i] = q[3] | (q[2] << 8) | (q[1] << 16) | (q[0] << 24);
        }

        for (int i = 16; i < 80 ; i++) {
            w[i] = rotl32(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
        }

        std::uint32_t a = h_0;
        std::uint32_t b = h_1;
        std::uint32_t c = h_2;
        std::uint32_t d = h_3;
        std::uint32_t e = h_4;

        for (int i = 0; i < 80 ; i++) {
            std::uint32_t f, k;

            if (i < 20) {
                f = (b & c) | ((~ b) & d);
                k = 0x5a827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ed9eba1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8f1bbcdc;
            } else {
                f = b ^ c ^ d;
                k = 0xca62c1d6;
            }

            std::uint32_t g = rotl32(a, 5) + f + e + k + w[i];

            e = d;
            d = c;
            c = rotl32(b, 30);
            b = a;
            a = g;
        }

        h_0 = h_0 + a;
        h_1 = h_1 + b;
        h_2 = h_2 + c;
        h_3 = h_3 + d;
        h_4 = h_4 + e;
    }

    // Done; output the digest as a string of hex bytes.

    std::ostringstream h;
    h << std::hex << std::setfill('0')
      << std::setw(8) << h_0
      << std::setw(8) << h_1
      << std::setw(8) << h_2
      << std::setw(8) << h_3
      << std::setw(8) << h_4;

    return h.str();
}

// ## Operation Mapping

// The *operations map* is a mapping from operation tags to shared
// pointers to the corresponding operation.  Each instantiated
// operation is immediately "mapped", by which we mean that we attempt
// to add it to the map, unless it already exists.  If that is the
// case the alreading mapped operation is returned in the place of the
// newly created operation.  This ensures that no operation is
// evaluated more than once.

// Consider the following snippet of front end code:

// ```
// (union
//   (translate (sphere 1) 1 0 0)
//   (translate (sphere 1) 2 0 0))
// ```

// Instead of evaluating the graph

// ```graph
// union -> "translate 1 0 0" -> "sphere a"
// union -> "translate 2 0 0" -> "sphere b"
// ```

// we evaluate

// ```graph
// union -> "translate 1 0 0" -> "sphere"
// union -> "translate 2 0 0" -> "sphere"
// ```

static std::unordered_map<std::string, std::weak_ptr<Operation>> operations_map;

std::shared_ptr<Operation> map_operation(const std::shared_ptr<Operation> &p)
{
    p->tag = p->describe();

    const auto [it, q] = operations_map.insert({p->tag, p});

    if (q) {
        return p;
    }

    std::shared_ptr<Operation> r = it->second.lock();

    if (!r) {
        it->second = p;
        return p;
    }

    if (Flags::warn_duplicate) {
        p->message(Operation::WARNING, "operation % already instantiated");
        r->message(Operation::NOTE, "first instance of operation");
    }

    return r;
}

std::unordered_map<
    std::string, std::weak_ptr<Operation>> &_get_operations_map(void)
{
    return operations_map;
}

// ## Sunk Operations

// Normally, the set of *sunk operations*, consists of sink operations
// (i.e. without any successors), that have been created as a result
// of the instantiation of output operations for enabled outputs (ref:
// Outputs).  If dead operation eliminitation has been disabled, all
// output operations will be sunk.

// Once the front end is finished and its state closed, these are the
// only shared pointers that will be keeping these sink operations
// alive (since they have no successors to point to them and no
// references from front end code).  They will in turn keep, via their
// operand references, their predecessors alive and so by extension
// all of their ancestors, but all other operations will be destroyed.
// This is just as well, as these are the only operations we need to
// evaluate in order to calculate all desired outputs.

// We're using a set instead of a plain list, because output
// operations can be inserted more than once under certain conditions.
// Consider the following snippet:

// ```
// (apply join (list-for ((s (iota 10))) (translate (? sphere 1) s 0 0)))
// ```

// Here, `(? sphere 1)` is a shortcut for `(output (sphere 1))`,
// typically used for simple debugging.  Evaluating the code will
// result in the front end attempting to sink the same sphere operation
// 10 times.

static std::unordered_set<std::shared_ptr<Operation>> sink_operations_set;

void sink_operation(std::shared_ptr<Operation> &&op)
{
    sink_operations_set.emplace(std::move(op));
}

// The following list is created from the culled operations map.  It
// is mostly used to facilitate graph rewriting.

static std::forward_list<std::weak_ptr<Operation>> operations_list;

void insert_operation(const std::shared_ptr<Operation> &op)
{
    operations_list.push_front(op);
}

// ## Graph Evaluation

// This is the main entry-point to the evaluation process.  Most of
// the work is done by setting up evaluation workers and letting them
// go to work, but we first need some preparations.

void evaluate_operations()
{
    evaluation_start = std::chrono::steady_clock::now();

    // If the user selected any dump streams (ref: Global Evaluation
    // State), we open the streams here.  If the user specified a file
    // name, we open that file for output.  If the specified file was
    // `-`, we open the standard output.  If nothing was specfiied, we
    // put together a default file name and use that.

#define SET_UP_DUMP_STREAM(WHAT, EXT)                                   \
    std::filebuf WHAT ##_dump_filebuf;                                  \
                                                                        \
    if (Options::dump_## WHAT) {                                        \
        if (!std::strcmp(Options::dump_## WHAT, "-")) {                 \
            WHAT ##_dump.rdbuf(std::cout.rdbuf());                      \
        } else if (!std::strcmp(Options::dump_## WHAT, "")) {           \
            WHAT ##_dump_filebuf.open(                                  \
                std::string("evaluation") + EXT,                        \
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

    SET_UP_DUMP_STREAM(list, ".list");
    SET_UP_DUMP_STREAM(log, ".log");
    SET_UP_DUMP_STREAM(graph, ".dot");

#undef SET_UP_DUMP_STREAM

    if (Options::dump_graph) {
        graph_dump << "digraph {\n"
                   << "node [shape=record]" << std::endl;
    }

    // ### Culling Dead Operations

    // The operations map has weak pointers to mapped operations and
    // by this time, many of them will have been destroyed, maybe
    // becuase they were created, but never really used by the
    // front end, or because they were not in the ancestor subgraph of
    // an enabled output.  By unlinking the operation from the graph
    // during destruction, we are therefore automatically left with
    // just the subgraph needed to evaluate the ouputs.

    // We also transfer all operations that haven't been destroyed to
    // a list and clear the map.

    // Even though we can potentially create more operations while
    // rewriting the graph below, each such rewrite will change the
    // tag of the whole successor subgraph of the rewritten operation.
    // Using the map to deduplicate newly created operations, would
    // require us to rehash all these operations during every rewrite,
    // as otherwise we might mistakenly substitute the new operation
    // with an existing operation based on a tag which is no longer
    // valid.

    // One might argue that the operation, although rewritten, should
    // still produce the same result as the old operation, on which
    // the tag is based, but still, it's probably best to avoid this
    // as any benefits are bound to be marginal anyway.

    {
        for (auto &[k, x]: operations_map) {
            if (auto p = x.lock(); p) {
                // At this point, the following shared pointers should
                // exist for each operation: ^[This is not strictly
                // correct.  The join operations created by `(union
                // (sphere 1) (sphere 1)))` will hold 2 pointer to the
                // single sphere operation, one for each operand, but
                // there will be only one pointer in the sphere's
                // successors set, so this assertion will fail.  Still
                // that doesn't typically happen in practice, so
                // keeping this around for debug builds is probably
                // useful.]


                //   1. One for each successor, in the form of operand pointers,
                //   2. One for output operations, listed in
                //   `sink_operations_set`.
                //   3. The one locked in `p` above.

                assert(
                    static_cast<std::size_t>(p.use_count())
                    == (p->successors.size()
                        + std::any_of(
                            sink_operations_set.begin(),
                            sink_operations_set.end(),
                            [&](auto const &q){
                                return q == p;
                            })
                        + 1));
                operations_list.push_front(p);
            }
        }

        operations_map.clear();
    }

    // ### Rewriting the Graph

    // We now attempt to rewrite parts of the graph, so as to
    // restructure the calculation in a way that will hopefully allow
    // us to calculate it more efficiently.  Ref: Graph Rewriting.

    // Each rewrite can potentially create opportunities for further
    // rewrites, so we keep making passes until no rewrites are
    // possible.

    Operation_rewriter r;

    for (int i = 0;
         Options::rewrite_pass_limit < 0 || i < Options::rewrite_pass_limit;
         i++) {
        int n = 0, m = 0;

        for (auto &x: operations_list) {
            if (x.expired()) {
                continue;
            }

            m += r.try_rewrite(x.lock().get());
            n++;
        }

        if (Options::dump_log) {
            log_dump << evaluation_timestamp()
                     << ": rewrote " << m << " out of "
                     << n << " operations" << std::endl;
        }

        if (m == 0) {
            break;
        }
    }

    // In changing the graph, rewriting will have changed the tags of
    // the affected operations and of with those, the tags of their
    // entire ancestor subgraphs (since an operation's tag is present
    // in the argument list of all successor operations).

    // Since the tag of the successors will depend on the rewritten
    // operations, we need to topologically sort the successor
    // subgraph of the rewritten operations and update the tags in
    // order.

    // We use the `selected` flag to traverse the graph and take care
    // to reset it when we're done, so that it can be used for further
    // traversals below.

    {
        std::forward_list<Operation *> sorted;

        auto visit = [&sorted](Operation *op, auto &&visit) {
            if (op->selected) {
                return;
            }

            for (Operation *x: op->successors) {
                visit(x, visit);
            }

            op->selected = true;
            sorted.push_front(op);
        };

        for (auto &x: operations_list) {
            if (auto p = x.lock(); p && p->rewritten) {
                visit(p.get(), visit);
            }
        }

        int n = 0;

        for (auto &x: sorted) {
            x->tag = x->describe();
            x->selected = false;
            n++;
        }

        if (Options::dump_log) {
            log_dump << evaluation_timestamp()
                     << ": retagged " << n << " operations" << std::endl;
        }
    }

    // ### Operation Caching

    // While developing front-end code, we tend to make small changes
    // before re-evaluating to inspect the results.  These tend to
    // change only small parts of the graph, with the rest of the
    // operations remaining unchanged.  We can speed up the evaluation
    // considerably, by storing the result of each operation, so that
    // subsequent re-evaluations can load these, instead of computing
    // them from scratch.

    {
        int n = 0, m = 0, l = 0;

        auto visit = [&n, &m, &l](Operation *op, auto &&visit) {
            if (op->selected) {
                return;
            }

            // We use the tag, which is unique for each operation in a
            // digested form (ref: Operation Digests) as the file name
            // and append a suitable extension, depending on whether
            // we will compress the stored files, or not.  This file
            // (in the current directory) is the operation's *store*.
            // When dispatching an operation we will load it from its
            // store if possible, and if not evaluate it and store
            // it. Ref: Operation Dispatch.

            assert(op->tag_digest.empty());

            op->tag_digest = std::move(digest_operation_tag(op->tag));
            op->store_path =
                op->tag_digest
                + (Options::store_compression < 0 ? ".o" : ".zo");

            // If the store file exists and operation loading has not
            // been disabled, we deem the operation loadable.  If that
            // is not the case we need not concern ourselves with the
            // ancestors of this operation and terminate the traversal
            // here (although we do so anyway if dead operation
            // elimination has been disabled).

            if (Flags::load_operations) {
                std::ifstream f(op->store_path);
                op->loadable = f && f.is_open();
            }

            const bool p = Flags::eliminate_dead_operations && op->loadable;

            if (!p) {
                for (Operation *x: op->predecessors) {
                    visit(x, visit);
                }
            }

            // The `selected` flag here marks the node as visited, as
            // usual, but we don't reset it when we're done as before.
            // A set `selected` flag at this point, means this
            // operation needs to be evaluated so we'll reset it in
            // the workers, to mark that the operation has been
            // evaluated.

            op->selected = true;
            n++;

            // Finally we need to insert any ready operations into the
            // ready lists, to kickstart the evaluation process.
            // Ready operations can be either loadable operations, or
            // source operations.

            if (p) {
                // The operation will be loaded from the store, so we
                // need to abridge the to-be-loaded operation's tag
                // here, as it won't happen during evaluation (since
                // its predecessors will never be evaluated).

                if (Flags::dump_abridged_tags) {
                    std::lock_guard<std::mutex> lock(dump_mutex);
                    std::string &r = tags.insert({op, op->tag}).first->second;

                    for (Operation *x: op->predecessors) {
                        const std::string &k = x->tag;

                        for (std::size_t i = r.find(k);
                             i != std::string::npos;
                             i = r.find(k, i)) {
                            r.replace(i, k.size(), "$");
                        };
                    }
                }

                // Since we'll load this operation, we'll never need
                // to refer to its operands and can reset its
                // references.  This will allow the whole ancestor
                // subgraph to be destroyed, unless it's referenced by
                // other operations.

                op->reset();

                ready_operation(op);
                m++;
            } else if (op->predecessors.empty()) {
                // Place all source operations on the ready list to
                // kickstart the evaluation.

                ready_operation(op);
                l++;
            }
        };

        for (auto &x: sink_operations_set) {
            visit(x.get(), visit);
        }

        if (Options::dump_log) {
            log_dump << evaluation_timestamp()
                     << ": selected " << n << " operations, with "
                     << (m + l) << " ready and "
                     << l << " loadable" << std::endl;
        }

        // Since we reset loadable operations above, all remaining
        // operations should be selected at this point.

        assert(std::all_of(
                   operations_list.cbegin(),
                   operations_list.cend(),
                   [](const auto &x){
                       auto p = x.lock();
                       return !p || p->selected;
                   }));
    }

    // We stop here if `--no-evaluate` has been specified.  This can
    // be useful for debugging, or during testing.

    if (!Flags::evaluate) {
        return;
    }

    // After all that work, evaluation is a simple case of creating
    // one worker for single-threaded execution, or multiple workers
    // for multi-threaded execution.

    if (Options::threads == 0) {
        assert(ready_list[1].empty());
        Worker(0).work();
    } else {
        std::list<Worker> workers;

        for (int i = 1; i <= Options::threads; i++) {
            workers.emplace_back(i);
        }

        Worker(0).work();
    }

    if (Options::dump_graph) {
        graph_dump << "}" << std::endl;
    }

    list_dump.rdbuf(nullptr);
    log_dump.rdbuf(nullptr);
    graph_dump.rdbuf(nullptr);

    // At this point, if all went well, all but the sink operations
    // should have been destroyed as we reset the shared pointers to
    // each operatins operands post-evaluation.  We clear the last
    // remaining shared pointers to the sink operations below.

    // If there was a failure, the unevaluated portion of the graph
    // will still be held together by the operand pointers, with
    // everything ultimately hinging on the shared pointers to the
    // sink operations.  Clearing those should again trigger a cascade
    // that will end in freeing the entire graph.

    sink_operations_set.clear();

    // Make sure everthing was cleared.

    assert(
        std::accumulate(
            operations_list.begin(), operations_list.end(), 0,
            [](std::size_t n, const auto &p) {
                n += !p.expired();
                return n;
            }) == 0);

    assert(ready_list[0].empty());
    assert(ready_list[1].empty());

    // During normal operation, we only evaluate once, so it isn't
    // necessary to reset the various structures used during evaluaion.
    // It is nevertheless necessary when running tests, to keep each case
    // isolated, or to simulate multiple runs of the program.

    sink_operations_set.clear();
    tags.clear();

    had_failure = false;
    evaluation_sequence = 0;
}
