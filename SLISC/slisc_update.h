#pragma once
#include "basic.h"

namespace slisc {
    // return the size of a tuple
    template <typename... Ts>
    constexpr Long size(const std::tuple<Ts...>&) {
        return (Long)sizeof...(Ts);
    }
}
