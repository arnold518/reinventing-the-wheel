#pragma once

#include <array>
#include <cstddef>

namespace circuit {
// The widths for which Pin<W> and Wire<W> are instantiated. Add a width here
// and in src/basic/TemplateInstantiations.cpp, nowhere else.
inline constexpr std::array<size_t, 8> kSupportedWidths{1, 2, 3, 4, 5, 8, 16, 32};

constexpr bool isSupportedWidth(size_t width) {
    for (const auto supported : kSupportedWidths) {
        if (supported == width) return true;
    }
    return false;
}

// Runs f<W>() for a run-time width, or unsupported() when the width has no
// instantiation. Both must return the same type.
template<class F, class U>
decltype(auto) dispatchWidth(size_t width, F&& f, U&& unsupported) {
    switch (width) {
        case 1: return f.template operator()<1>();
        case 2: return f.template operator()<2>();
        case 3: return f.template operator()<3>();
        case 4: return f.template operator()<4>();
        case 5: return f.template operator()<5>();
        case 8: return f.template operator()<8>();
        case 16: return f.template operator()<16>();
        case 32: return f.template operator()<32>();
        default: return unsupported();
    }
}
} // namespace circuit
