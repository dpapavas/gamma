#include <cstdint>
#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>

#include <CGAL/exceptions.h>

#include "options.h"
#include "operation.h"

std::function<void(Operation &)> Operation::hook;

static std::uint32_t rotl32 (std::uint32_t x, unsigned int n) {
    return (x << n) | (x >> (32 - n));
}

static std::string sha1digest(const std::string &message)
{
    const std::uint8_t *s =
        reinterpret_cast<const std::uint8_t *>(message.c_str());
    const int n = message.size();

    std::uint8_t buffer[64];

    std::uint32_t h_0 = 0x67452301;
    std::uint32_t h_1 = 0xefcdab89;
    std::uint32_t h_2 = 0x98badcfe;
    std::uint32_t h_3 = 0x10325476;
    std::uint32_t h_4 = 0xc3d2e1f0;

    for (int j = 0; j < ((n + 9 + 63) / 64) * 64; j += 64) {
        const std::uint8_t *p;

        if (n - j >= 64) {
            // Not the last block; do not pad.

            p = s + j;
        } else {
            int i = 0;

            p = buffer;

            // Copy rest of message and add the terminating 1 bit.

            if (j <= n) {
                memcpy(buffer, s + j, (i = n - j));
                buffer[i++] = 0x80;
            }

            // Pad as needed with zeros and append the message length
            // as 64bit big-endian if it fits (otherwise another block
            // must be added).

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

        // Process the block.

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

    // Output the digest in hex form.

    std::ostringstream h;
    h << std::hex << h_0 << h_1 << h_2 << h_3 << h_4;
    return h.str();
}

void Operation::message(Message_level level, std::string message)
{
    std::string tag;

    if (Options::diagnostics_elide_tags < 0) {
        tag = get_tag();
    } else {
        std::ostringstream s;

        int i = 0;
        for (const char d: get_tag()) {
            if (d == ')') {
                i -= 1;
            }

            if (i <= Options::diagnostics_elide_tags) {
                s.put(d);
            }

            if (d == '(') {
                i += 1;

                if (i == Options::diagnostics_elide_tags + 1) {
                    s.write("...", 3);
                }
            }
        }

        tag = s.str();
    }

    if (Options::diagnostics_shorten_tags >= 0
        && static_cast<int>(tag.size()) > Options::diagnostics_shorten_tags) {
        tag.erase(Options::diagnostics_shorten_tags, std::string::npos);
        tag += "...";
    }

    // Output the file and line number, if available.

    for (int i = 0; i < 2; i++) {
        if (auto f = annotations.find("file"); f != annotations.end()) {
            switch (level) {
            case NOTE: std::cerr << ANSI_COLOR(1, 32); break;
            case WARNING: std::cerr << ANSI_COLOR(1, 33); break;
            case ERROR: std::cerr << ANSI_COLOR(1, 31); break;
            }

            std::cerr << f->second << ANSI_COLOR(0, 37) << ":";
        }

        if (auto l = annotations.find("line"); l != annotations.end()) {
            std::cerr << ANSI_COLOR(1, 37) << l->second << ANSI_COLOR(0, 37)
                      << ": ";
        }

        if (i == 0) {
            std::cerr << "in operation '" << tag << "'\n";
        }
    }

    // Output the message kind.

    switch (level) {
    case NOTE:
        std::cerr << ANSI_COLOR(1, 32) << "note" << ANSI_COLOR(0, 37) << ": ";
        break;
    case WARNING:
        std::cerr << ANSI_COLOR(1, 33) << "warning"
                  << ANSI_COLOR(0, 37) << ": ";
        break;
    case ERROR:
        std::cerr << ANSI_COLOR(1, 31) << "error" << ANSI_COLOR(0, 37) << ": ";
        break;
    }

    // Output the message, splicing in the operation tag, as needed.

    for (auto c = message.cbegin(); c != message.cend(); c++) {
        if (*c != '%') {
            std::cerr.put(*c);
        } else if (c + 1 != message.cend() && *(c + 1) == '%') {
            std::cerr.put(*c++);
        } else {
            std::cerr << '\'' << ANSI_COLOR(1, 37) << tag
                      << ANSI_COLOR(0, 37) << '\'';
        }
    }

    std::cerr << std::endl;

    if (Flags::warn_error && level == WARNING) {
        throw operation_warning_error("previous warning treated as error");
    }
}

std::string Operation::digest()
{
    if (tag_digest.empty()) {
        tag_digest = sha1digest(get_tag());
    }

    return tag_digest;
}

void Operation::select()
{
    selected = true;
    store_path = digest() + (Options::store_compression < 0 ? ".o" : ".zo");

    if (!Flags::load_operations) {
        return;
    }

    std::ifstream f(store_path);
    loadable = f && f.is_open();

    return;
}

bool Operation::dispatch()
{
    if (!Flags::evaluate) {
        return false;
    }

    // Try loading if previously stored.

    if (loadable) {
        if (load()) {
            annotations.insert({"loaded", store_path});

            if (Flags::warn_load) {
                message(WARNING, "Operation % was loaded");
            }

            return false;
        }

        return true;
    }

    // Evaluate.

    auto t_0 = std::chrono::steady_clock::now();
    evaluate();
    float delta = std::chrono::duration_cast<std::chrono::duration<float>>(
        std::chrono::steady_clock::now() - t_0).count();

    {
        std::ostringstream s;
        s.precision(2);
        s << delta << "s";

        annotations.insert({"in", s.str()});
    }

    cost += delta;

    {
        std::ostringstream s;
        s.precision(2);
        s << cost << "s";

        annotations.insert({"cost", s.str()});
    }

    if (Flags::store_operations
        && cost > Options::store_threshold) {
        if (store()) {
            annotations.insert({"stored", store_path});

            if (Flags::warn_store) {
                message(WARNING, "Operation % was stored");
            }
        }
    }

    return false;
}
