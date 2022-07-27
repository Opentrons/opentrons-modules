#pragma once

#include <tuple>
#include <variant>

namespace ot_utils {
namespace utils {

/*
** Turn a tuple holding some types into a variant holding some types.
** Useful type-level transformation for composing supported message
** sets.
**
** Use like
** using SupportedMessages = std::tuple<Message1, Message2>
** using Messages = typename TupleToVariant<SupportedMessages>::type;
**
** then Messages will be a variant.
*/
template <class>
struct TupleToVariant;

template <class... TupleContents>
struct TupleToVariant<std::tuple<TupleContents...>> {
    using type = std::variant<TupleContents...>;
};

/*
** Turns two tuples holding some types into a single variant holding
** the union of the types contained by the tuples. Useful for building
** message sets from reusable components.
**
** Use like
** using CanMessages = std::tuple<CanMessage1, CanMessage2>;
** using InternalMessages = std::tuple<InternalMessage1, InternalMessage2>;
** using TaskMessage = typename TuplesToVariants<CanMessages, InternalMessages>;
**
** in which case TaskMessage will be a std::variant containing CanMessage1 and
*2,
** and InternalMessage1 and 2.
**
*/
template <class, class>
struct TuplesToVariants;

template <class... TupleAContents, class... TupleBContents>
struct TuplesToVariants<std::tuple<TupleAContents...>,
                        std::tuple<TupleBContents...>> {
    using type = std::variant<TupleAContents..., TupleBContents...>;
};

/*
** Concatenates two variants. Useful as the final stage of the tuple-variant
transformation
** to salt in things like monostates. Can be used as
**
** using _CanMessages = std::tuple<CanMessage1, CanMessage2>;
** using _InternalMessages = std::tuple<InternalMessage1, InternalMessage2>;
** using TaskMessage = typename VariantCat<
**           std::variant<std::monostate>,
**           typename TuplesToVariants<
**                _CanMessages, _InternalMessages>::type>::type;
**
** In which case TaskMessage will contain both can messages, both internal
** messages, and std::monostate.

*/
template <class, class>
struct VariantCat;

template <class... VariantAContents, class... VariantBContents>
struct VariantCat<std::variant<VariantAContents...>,
                  std::variant<VariantBContents...>> {
    using type = std::variant<VariantAContents..., VariantBContents...>;
};

};  // namespace utils
};  // namespace ot_utils
