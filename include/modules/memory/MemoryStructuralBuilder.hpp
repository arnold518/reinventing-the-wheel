#pragma once

#include "components/StructuralNetBuilder.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/utility/Constant.hpp"
#include <map>
#include <stdexcept>
#include <vector>

namespace circuit::memory_detail {

// Kept so existing memory code can keep its short name.
using circuit::detail::StructuralWiring;

inline size_t arrayAddressBits(size_t depth) {
    // The narrowest supported array has at most 512 words. Validate before
    // shifts or constructing dynamic pin lists, even for standalone blocks.
    if (depth == 0 || depth > 512) throw std::invalid_argument("Invalid structural memory depth");
    size_t bits = 0;
    while ((size_t{1} << bits) < depth) ++bits;
    return bits;
}

} // namespace circuit::memory_detail
