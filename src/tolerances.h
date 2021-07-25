#ifndef TOLERANCES_H
#define TOLERANCES_H

struct Tolerances {
    inline static FT curve = FT::ET(1);
    inline static FT projection = FT::ET(1, 1000000);
    inline static FT sine = FT::ET(1, 1000000);
};

#endif
