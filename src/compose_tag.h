#ifndef COMPOSE_TAG_H
#define COMPOSE_TAG_H

#include <sstream>
#include <ostream>
#include <tuple>

// Simple values

template<typename T, typename = void>
struct compose_tag_helper {
    static void compose(std::ostringstream &s, const T &x);
};

// Iterables

template<typename T, typename U>
struct compose_tag_helper<std::pair<T, U>> {
    static void compose(std::ostringstream &s, const std::pair<T, U> &p) {
        compose_tag_helper<T>::compose(s, p.first);
        compose_tag_helper<U>::compose(s, p.second);
    }
};

template<typename T>
struct compose_tag_helper<T, std::enable_if_t<std::is_array_v<T>>> {
    static void compose(std::ostringstream &s, const T &v) {
        for (auto x: v) {
            compose_tag_helper<std::decay_t<decltype(x)>>::compose(s, x);
        }
    }
};

template<typename T>
struct compose_tag_helper<T, std::void_t<decltype(std::declval<T>().begin()),
                                         decltype(std::declval<T>().end())>> {
    static void compose(std::ostringstream &s, const T &v) {
        for (auto x: v) {
            compose_tag_helper<std::decay_t<decltype(x)>>::compose(s, x);
        }
    }
};

template<typename... Args>
std::string compose_tag(const char *name, Args &&... args)
{
    std::ostringstream s;

    s << name << "(";
    if constexpr (sizeof...(Args) > 0) {
        const auto p = s.tellp();
        auto t = std::make_tuple(args...);

        (compose_tag_helper<std::remove_cv_t<
                                std::remove_reference_t<Args>>>::compose(
                                    s, args), ...);

        if (s.tellp() > p) {
            s.seekp(-1, std::ios_base::end);
        }

        s << ")";
    } else {
        s << ")";
    }

    return s.str();
}

#endif
