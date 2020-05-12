#ifndef EV3PLOTTER_COMMON_DEFINITIONS_H_DEFINED
#define  EV3PLOTTER_COMMON_DEFINITIONS_H_DEFINED

#include <named_type/named_type.hpp>

namespace ev3plotter {
    template <typename T, typename TTag>
    using StrongInt = fluent::NamedType<T, TTag, fluent::Addable, fluent::Subtractable, fluent::Comparable, fluent::Incrementable, fluent::Hashable, fluent::Printable>;

    template <typename T, typename TTag>
    using StrongFloat = fluent::NamedType<T, TTag, fluent::Addable, fluent::Subtractable, fluent::Comparable, fluent::Incrementable, fluent::Hashable, fluent::Printable>;

    
    using raw_pos = StrongInt<int, struct RawPosTag>;
    using normalized_pos = StrongInt<int, struct NormalizedTag>;
    using mm_pos = StrongFloat<float, struct MmPosTag>;
}

#endif // EV3PLOTTER_COMMON_DEFINITIONS_H_DEFINED