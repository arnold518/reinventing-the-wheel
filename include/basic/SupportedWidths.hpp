#pragma once

#include "basic/PackedLogic.hpp"
#include <cstddef>
#include <utility>

namespace circuit {
// A pin or wire carries 1 to 64 bits: one packed four-state word. Which widths
// a component family offers is that family's own contract choice.
inline constexpr size_t kMaxSignalWidth = PackedLogic::kMaxWidth;

constexpr bool isSupportedWidth(size_t width) {
    return width >= 1 && width <= kMaxSignalWidth;
}

namespace detail {
template<class F, class U, size_t... I>
decltype(auto) dispatchWidthTable(size_t width, F& f, U& unsupported, std::index_sequence<I...>) {
    using Result = decltype(f.template operator()<1>());
    if (!isSupportedWidth(width)) return static_cast<Result>(unsupported());
    using Entry = Result (*)(F&);
    static constexpr Entry table[] = {+[](F& g) -> Result { return g.template operator()<I + 1>(); }...};
    return table[width - 1](f);
}
} // namespace detail

// Runs f<W>() for a run-time width of 1 to 64 bits, or unsupported() for any
// other width. Both must return the same type.
template<class F, class U>
decltype(auto) dispatchWidth(size_t width, F&& f, U&& unsupported) {
    return detail::dispatchWidthTable(width, f, unsupported, std::make_index_sequence<kMaxSignalWidth>{});
}
} // namespace circuit
