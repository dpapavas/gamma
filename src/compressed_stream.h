#ifndef COMPRESSED_STREAM_H
#define COMPRESSED_STREAM_H

#include <fstream>

class compressed_ofstream_wrapper: public std::ofstream
{
public:
    compressed_ofstream_wrapper(int level);

    void open(const char *filename);
    void open(const std::string &filename);

    void compress(int level);
    ~compressed_ofstream_wrapper();
};

class compressed_ifstream_wrapper: public std::ifstream
{
public:
    compressed_ifstream_wrapper(bool decompress);

    void open(const char *filename);
    void open(const std::string &filename);

    void decompress(bool enable);
    ~compressed_ifstream_wrapper();
};

#endif
