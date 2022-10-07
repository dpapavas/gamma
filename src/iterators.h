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

#ifndef ITERATORS_H
#define ITERATORS_H

#include <vector>

template<typename T>
class null_iterator:
    public std::iterator<std::output_iterator_tag, void, void, void, void>
{
public:
    null_iterator &operator=(const T &) {
        return *this;
    }

    null_iterator &operator*() {
        return *this;
    }

    null_iterator &operator++() {
        return *this;
    }

    null_iterator operator++(int) {
        return *this;
    }
};

template<typename I>
class iterator_chain {

    typedef typename I::value_type T;
    typedef std::vector<std::pair<I, I>> Chain;

    Chain parts;

public:
    class iterator: public std::iterator<std::forward_iterator_tag,
                                         T, std::ptrdiff_t, T*, T&> {
        I it;
        typename Chain::iterator chunk, last;

    public:
        iterator() = default;
        explicit iterator(typename Chain::iterator c,
                          typename Chain::iterator l,
                          const I &p):
            it(p), chunk(c), last(l) {}

        iterator &operator++() {
            if (it != chunk->second) {
                it++;
            }

            while (it == chunk->second && chunk != last) {
                chunk++;
                it = chunk->first;
            }

            return *this;
        }

        iterator operator++(int) {
            iterator i = *this;
            (*this)++;

            return i;
        }

        bool operator==(iterator other) const {
            return it == other.it;
        }

        bool operator!=(iterator other) const {
            return !(*this == other);
        }

        const T &operator*() const {
            return *it;
        }
    };

    void push_back(const I &begin, const I &end) {
        parts.emplace_back(begin, end);
    }

    std::size_t size() const {
        return parts.size();
    }

    iterator begin() {
        return iterator(
            parts.begin(), std::prev(parts.end()), parts.front().first);
    }

    iterator end() {
        typename Chain::iterator last = std::prev(parts.end());
        return iterator(last, last, last->second);
    }
};

#endif
