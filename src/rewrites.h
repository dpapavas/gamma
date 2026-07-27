// Copyright 2022, 2026 Dimitris Papavasiliou

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

#ifndef REWRITES_H
#define REWRITES_H

#include <unordered_map>
#include <typeindex>

#include "operation.h"

class Operation_rewriter:
    std::unordered_map<std::type_index, bool (*) (Operation *)> {

public:
    Operation_rewriter();
    bool try_rewrite(Operation *op) const;
};

#endif
