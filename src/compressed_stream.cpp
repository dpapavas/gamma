#include <cassert>
#include <iostream>
#include <memory>
#include <zlib.h>

#include "compressed_stream.h"

#define CHUNK_SIZE 16384

class compressed_ostreambuf_wrapper: public std::streambuf
{
private:
    std::streambuf *wrapped;
    std::unique_ptr<unsigned char []> in;
    std::unique_ptr<unsigned char []> out;

    z_stream stream;

    bool deflate_input(bool finish) {
        // We're normally called with a full buffer, but it may also
        // be partially filled, e.g. when flushing/syncing.

        stream.avail_in = pptr() - pbase();
        stream.next_in = in.get();

        // Deflate all available input.

        do {
            stream.avail_out = CHUNK_SIZE;
            stream.next_out = out.get();

            const auto x = deflate(&stream, finish ? Z_FINISH : Z_NO_FLUSH);
            const int n = CHUNK_SIZE - stream.avail_out;
            const bool p = (wrapped->sputn(reinterpret_cast<char *>(
                                               out.get()), n) == n);

            if (!p || x != Z_OK) {
                deflateEnd(&stream);

                // Only throw an exception when called during
                // overflow.

                if (p && x == Z_STREAM_END) {
                    return true;
                } else if (finish) {
                    return false;
                } else {
                    throw std::ios_base::failure("inflation error");
                }
            }
        } while (stream.avail_out == 0);

        assert(stream.avail_in == 0);

        return false;
    }

public:
    compressed_ostreambuf_wrapper(std::streambuf *streambuf, const int level):
        wrapped(streambuf),
        in(std::make_unique<unsigned char []>(CHUNK_SIZE)),
        out(std::make_unique<unsigned char []>(CHUNK_SIZE)) {
        assert(streambuf);

        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.opaque = Z_NULL;

        if (deflateInit(&stream, level) == Z_OK) {
            char *p = reinterpret_cast<char *>(in.get());
            setp(p, p + CHUNK_SIZE);
        } else {
            setp(nullptr, nullptr);
        }
    }

    compressed_ostreambuf_wrapper(const compressed_ostreambuf_wrapper &) = delete;
    compressed_ostreambuf_wrapper &operator=(
        const compressed_ostreambuf_wrapper &) = delete;

    std::streambuf::int_type overflow(
        std::streambuf::int_type c = traits_type::eof()) override {

        if (!pptr()) {
            return traits_type::eof();
        }

        if (deflate_input(false)) {
            setp(nullptr, nullptr);
            return traits_type::eof();
        }

        // Update pointers to reflect a fresh input buffer.

        char *p = reinterpret_cast<char *>(in.get());
        setp(p, p + CHUNK_SIZE);
        if (traits_type::eq_int_type(c, traits_type::eof())) {
            return traits_type::not_eof(c);
        } else {
            return sputc(char_type(c));
        }
    }

    int sync() override {
        overflow();
        return 0;
    }

    ~compressed_ostreambuf_wrapper() {
        deflate_input(true);
    }
};

class compressed_istreambuf_wrapper: public std::streambuf
{
private:
    std::streambuf *wrapped;
    std::unique_ptr<unsigned char []> in;
    std::unique_ptr<unsigned char []> out;

    z_stream stream;
    bool finished;

public:
    compressed_istreambuf_wrapper(std::streambuf *streambuf):
        wrapped(streambuf),
        in(std::make_unique<unsigned char []>(CHUNK_SIZE)),
        out(std::make_unique<unsigned char []>(CHUNK_SIZE)) {
        assert(streambuf);

        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.opaque = Z_NULL;
        stream.avail_in = 0;
        stream.next_in = Z_NULL;

        if (inflateInit(&stream) == Z_OK) {
            char *g = reinterpret_cast<char *>(out.get());
            setg(g, g + CHUNK_SIZE, g + CHUNK_SIZE);
            finished = false;
        } else {
            finished = true;
        }
    }

    ~compressed_istreambuf_wrapper() {
        if (!finished) {
            inflateEnd(&stream);
        }
    }

    compressed_istreambuf_wrapper(const compressed_istreambuf_wrapper &) = delete;
    compressed_istreambuf_wrapper &operator=(
        const compressed_istreambuf_wrapper &) = delete;

    std::streambuf::int_type underflow() override {
        assert(gptr() >= egptr());

        if (finished) {
            return traits_type::eof();
        }

        // We should be called with a fully consumed buffer, so we can
        // reuse all of it.

        stream.avail_out = CHUNK_SIZE;
        stream.next_out = out.get();

        // Inflate unitl the output buffer is full, or the stream
        // exhausted.

        while (stream.avail_out > 0) {
            // Refill the input from file when necessary.

            if (stream.avail_in == 0) {
                stream.avail_in = wrapped->sgetn(
                    reinterpret_cast<char *>(in.get()), CHUNK_SIZE);
                stream.next_in = in.get();
            }

            const auto x = inflate(
                &stream, stream.avail_in == 0 ? Z_FINISH : Z_NO_FLUSH);

            if (x != Z_OK) {
                inflateEnd(&stream);
                finished = true;

                if (x != Z_STREAM_END) {
                    throw std::ios_base::failure("deflation error");
                }

                break;
            }
        }

        // Update pointers to reflect fresh data for input or EOF.

        const int n = CHUNK_SIZE - stream.avail_out;
        char *g = reinterpret_cast<char *>(out.get());
        setg(g, g, g + n);

        return (n == 0
                ? traits_type::eof()
                : traits_type::to_char_type(sgetc()));
    }
};

// The streambuf object returned by std::*stream::rdbuf and the
// filebuf object returned by std::*fstream::rdbuf are maintained
// separately.  We can substitute the former as needed, while keeping
// the latter.

compressed_ofstream_wrapper::compressed_ofstream_wrapper(int level)
{
    if (level >=0) {
        std::ostream::rdbuf(
            static_cast<std::streambuf *>(
                new compressed_ostreambuf_wrapper(rdbuf(), level)));
    }
}

void compressed_ofstream_wrapper::open(const char *filename)
{
    std::ofstream::open(
        filename, (rdbuf() != std::ostream::rdbuf()
                   ? std::ios_base::out | std::ios_base::binary
                   : std::ios_base::out));
}

void compressed_ofstream_wrapper::open(const std::string &filename)
{
    std::ofstream::open(
        filename, (rdbuf() != std::ostream::rdbuf()
                   ? std::ios_base::out | std::ios_base::binary
                   : std::ios_base::out));
}

compressed_ofstream_wrapper::~compressed_ofstream_wrapper()
{
    const std::streambuf *p = std::ostream::rdbuf();

    if (p != rdbuf()) {
        delete p;
    }
}

compressed_ifstream_wrapper::compressed_ifstream_wrapper(bool decompress)
{
    if (decompress) {
        std::istream::rdbuf(
            static_cast<std::streambuf *>(
                new compressed_istreambuf_wrapper(rdbuf())));
    }
}

void compressed_ifstream_wrapper::open(const char *filename)
{
    std::ifstream::open(
        filename, (rdbuf() != std::istream::rdbuf()
                   ? std::ios_base::in | std::ios_base::binary
                   : std::ios_base::in));
}

void compressed_ifstream_wrapper::open(const std::string &filename)
{
    std::ifstream::open(
        filename, (rdbuf() != std::istream::rdbuf()
                   ? std::ios_base::in | std::ios_base::binary
                   : std::ios_base::in));
}

compressed_ifstream_wrapper::~compressed_ifstream_wrapper()
{
    const std::streambuf *p = std::istream::rdbuf();

    if (p != rdbuf()) {
        delete p;
    }
}
