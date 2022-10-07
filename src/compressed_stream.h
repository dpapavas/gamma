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
