#ifndef EV3PLOTTER_COMMON_DEFINITIONS_H_DEFINED
#define  EV3PLOTTER_COMMON_DEFINITIONS_H_DEFINED

#include <named_type/named_type.hpp>

namespace ev3plotter {
    template <typename T, typename TTag>
    using StrongInt = fluent::NamedType<T, TTag, fluent::Addable, fluent::Subtractable, fluent::Comparable, fluent::Incrementable, fluent::Hashable, fluent::Printable>;
}

#endif // EV3PLOTTER_COMMON_DEFINITIONS_H_DEFINED