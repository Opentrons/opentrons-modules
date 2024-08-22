/*
** gcode_parser - a templated gcode parser for use anywhere
*/
#pragma once

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <iterator>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>

namespace gcode {

/*
 * gcode::gobble_whitespace is a convenience function to strip out any
 * whitespace from an input; the returned iterator is after any whitespace at
 * the head of the input.
 */
template <typename Input, typename Limit>
requires std::forward_iterator<Input> && std::sized_sentinel_for<Limit, Input>
auto gobble_whitespace(const Input& start_from, Limit stop_at) -> Input {
    // this has to be a lambda because std::isspace's prototype does not
    // interact well with template argument deduction, sadly
    return std::find_if_not(
        start_from, stop_at,
        [](const unsigned char c) -> bool { return std::isspace(c); });
}

/*
 *  gcode::prefix_matches is a convenience function for gcode parsers that
 * checks length and matches a char array representing a prefix. If the match
 * fails, the returned iterator is the same as passed in; if it succeeds, it
 * will point to immediately after the prefix in the input.
 */
template <typename Input, typename Limit, typename PrefixArray>
requires std::forward_iterator<Input> &&
    std::sized_sentinel_for<Limit, Input> &&
    std::convertible_to < std::iter_value_t<Input>,
typename PrefixArray::value_type >
    auto prefix_matches(const Input& start_from, Limit stop_at,
                        const PrefixArray& prefix) -> Input {
    if (static_cast<size_t>(stop_at - start_from) < prefix.size()) {
        return start_from;
    }
    if (!std::equal(prefix.cbegin(), prefix.cend(), start_from)) {
        return start_from;
    }
    return start_from + prefix.size();
}

/*
 * gcode::parse_value is a convenience function for parsing a value out of a
 * gcode string. If the match succeeds, the optional part of the pair will be
 * filled in and the iterator part of the pair will point to just past the value
 * (which will therefore be a whitespace). If the match fails, the optional part
 * of the pair will be empty, and the iterator part of the pair will point to
 * the input.
 *
 * This function folds in both the calls to the value-from-string parsing
 * functions and some basic structural verification. Because gcodes and gcode
 * arguments are separated with spaces, and because we're only going to parse
 * strings that are complete and therefore delimited with newlines, any value
 * should be followed by something that passes std::isspace(). If it isn't, then
 * we've got malformed input (e.g. a float value with a decimal point in a
 * context that expects an int).
 */
template <typename ValueType, typename Input, typename Limit,
          size_t working_buf_size = 32>
requires std::forward_iterator<Input> &&
    std::sized_sentinel_for<Limit, Input> && std::integral<ValueType>
auto parse_value(const Input& start_from, Limit stop_at)
    -> std::pair<std::optional<ValueType>, Input> {
    ValueType value = 0;
    Input working = start_from;
    auto [after_ptr, ec] = std::from_chars(&(*working), &(*stop_at), value);
    if (ec != std::errc()) {
        // some error occurred
        return std::make_pair(std::optional<ValueType>(), start_from);
    }
    if (!std::isspace(*after_ptr)) {
        // gcodes should be terminated by some whitespace, either a space
        // separating it from the next gcode or an \r\n terminator sequence
        return std::make_pair(std::optional<ValueType>(), start_from);
    }
    const auto * after = working + (after_ptr - &(*working));
    return std::make_pair(std::optional<ValueType>(value), after);
}

// sadly we need a float-specific version that works completely differently
// because std::from_chars isn't implemented at least in gcc 10, because the
// language specification makes it very hard. working_buf_size will be the size
// of the temporary array allocated ON THE STACK (so keep it nice and small) to
// ensure that the input has a null in it somewhere since we have to use sscanf
// for this.
template <typename ValueType, typename Input, typename Limit,
          size_t working_buf_size = 32>
requires std::forward_iterator<Input> &&
    std::sized_sentinel_for<Limit, Input> && std::floating_point<ValueType>
auto parse_value(const Input& start_from, Limit stop_at)
    -> std::pair<std::optional<ValueType>, Input> {
    ValueType value = 0;
    std::array<std::iter_value_t<Input>, working_buf_size> buf{};
    std::copy(start_from, std::min(stop_at, start_from + working_buf_size),
              buf.begin());
    buf.at(std::min(buf.size() - 1,
                    static_cast<size_t>(stop_at - start_from))) = 0;
    int distance = 0;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    int worked = sscanf(&(*start_from), "%f%n", &value, &distance);
    if (worked != 1) {
        return std::make_pair(std::optional<ValueType>(), start_from);
    }
    auto after = start_from + distance;
    if (after == stop_at || !std::isspace(*after)) {
        return std::make_pair(std::optional<ValueType>(), start_from);
    }
    return std::make_pair(std::optional<ValueType>(value), after);
}

// Gcode argument base required values
template <typename Arg>
concept GCodeArgument = requires(Arg& a) {
    // Needs a flag for whether it's required
    {std::is_same_v<decltype(Arg::required), bool>};
    // Variable within the argument - flag for if it is present
    {std::is_same_v<decltype(a.present), bool>};
};

// Gcode argument that has a prefix
template <typename Arg>
concept GCodeArgumentWithPrefix = requires(Arg& a) {
    // Prefix needs to be iterable
    {std::is_same_v<std::remove_cvref<decltype(*std::cbegin(Arg::prefix))>,
                    char>};
    {std::is_same_v<std::remove_cvref<decltype(*std::cend(Arg::prefix))>,
                    char>};
};

// Gcode argument that contains a variable `value` of any type
template <typename Arg>
concept GCodeArgumentWithValue = requires(Arg& a) {
    // Variable within the argument - can be many types
    {a.value};
};

// Gcode argument that contains an iterable value
template <typename Arg>
concept GCodeArgumentWithIterableValue = requires(Arg& a) {
    {std::begin(a.value)};
    {std::end(a.value)};
};

// Wrap gcode parser functions in this struct to separate the
// declaration of the template arguments...
template <GCodeArgument... Args>
struct SingleParser {
    /**
     * @brief parse_gcode provides a basic parser for parsing a command,
     * which consists of a prefix (or code) followed by an arbitrary
     * number of arguments. The arguments must follow strict ordering,
     * but each argument may be specified as optional and then may be
     * absent in a valid input.
     *
     * The argument values may be of any numeric type (any size int or float),
     * as well as an iterable string (using `std::array<char>`)
     */
    template <std::forward_iterator Input, typename Limit, typename PrefixArray>
    requires std::sized_sentinel_for<Limit, Input> &&
        std::convertible_to < std::iter_value_t<Input>,
    typename PrefixArray::value_type >
        static auto parse_gcode(const Input& start_from, Limit stop_at,
                                const PrefixArray& prefix)
            -> std::pair<std::optional<std::tuple<Args...>>, Input> {
        using ArgRet = std::optional<std::tuple<Args...>>;
        using RT = std::pair<ArgRet, Input>;
        // Check that prefix matches
        auto working = prefix_matches(start_from, stop_at, prefix);
        if (working == start_from) {
            return RT(ArgRet(), start_from);
        }
        working = gobble_whitespace(working, stop_at);
        if constexpr (sizeof...(Args) == 0) {
            // This command JUST needs a matching prefix (aka gcode)
            return RT(ArgRet(), working);
        } else {
            // We need to iterate through the arguments...
            auto parse_ret = parse_arg<Input, Limit, Args...>(working, stop_at);
            if (!parse_ret.second.has_value()) {
                return std::make_pair(std::nullopt, start_from);
            }
            return std::make_pair(ArgRet(parse_ret.first),
                                  parse_ret.second.value());
        }
    }

  private:
    /**
     * @brief parse_arg recurses through all of the argument types specified
     * for a command, parsing each one and only continuing through the list
     * while the input is still possibly valid.
     *
     * The values of each recursive call are concatenated into a single tuple
     * to return to the caller, along with an optional Limit iterator.
     *
     * @return A pair of (1) a tuple of the parsed argument structs and (2) an
     * optional Limit iterator. If (2) has no value, the input was not valid -
     * either a mandatory argument was missing, there was an unexpected arg,
     * or the value of an argument didn't match what was expected.
     */
    template <std::forward_iterator Input, typename Limit,
              GCodeArgument NextArg, typename... Remaining>
    static auto parse_arg(const Input& start_from, Limit stop_at)
        -> std::pair<std::tuple<NextArg, Remaining...>, std::optional<Input>> {
        using TupleRet = std::tuple<NextArg, Remaining...>;
        auto arg = NextArg();
        auto prefix_ret = arg_prefix_present<NextArg>(start_from, stop_at);
        Input working = start_from;
        if (!prefix_ret.second) {
            // This argument isn't present
            if (NextArg::required) {
                // Mark invalid and return up
                return std::make_pair(TupleRet(), std::nullopt);
            }
            // Mark absent and continue to recurse down
            arg.present = false;
        } else {
            // arg_parse_value is overloaded to work even if there's no value
            // required...
            working = arg_parse_value(prefix_ret.first, stop_at, arg);
            if (!arg.present) {
                // Argument prefix without a value - this is not allowed.
                // Mark invalid and return up
                return std::make_pair(TupleRet(), std::nullopt);
            }
        }

        if constexpr (sizeof...(Remaining) > 0) {
            // Recurse down to the next argument
            working = gobble_whitespace(working, stop_at);
            auto ret = parse_arg<Input, Limit, Remaining...>(working, stop_at);
            return std::make_pair(
                std::tuple_cat(std::make_tuple(arg), ret.first), ret.second);
        } else {
            // This was the last argument - return what we have
            return std::make_pair(std::make_tuple(arg),
                                  std::optional<Input>(working));
        }
    }

    // Checks if the prefix of an argument is present
    template <GCodeArgument Arg, std::forward_iterator Input, typename Limit>
    requires GCodeArgumentWithPrefix<Arg>
    static auto arg_prefix_present(const Input& start_from, Limit stop_at)
        -> std::pair<Input, bool> {
        auto working = prefix_matches(start_from, stop_at, Arg::prefix);
        if (working != start_from) {
            return std::make_pair(working, true);
        }
        return std::make_pair(start_from, false);
    }

    // Version when there is no prefix for an argument
    template <GCodeArgument Arg, std::forward_iterator Input, typename Limit>
    static auto arg_prefix_present(const Input& start_from, Limit stop_at)
        -> std::pair<Input, bool> {
        static_cast<void>(stop_at);
        return std::make_pair(start_from, true);
    }

    // Iterable string
    template <GCodeArgument Arg, std::forward_iterator Input, typename Limit>
    requires GCodeArgumentWithValue<Arg> && GCodeArgumentWithIterableValue<Arg>
    static auto arg_parse_value(const Input& start_from, Limit stop_at,
                                Arg& arg) -> Input {
        // using ValueType = decltype(Arg::value);
        const auto max_values =
            std::distance(arg.value.begin(), arg.value.end());
        // Find the length from start_from to the first whitespace
        auto working = start_from;
        for (auto index = working; index != stop_at; index++) {
            if (std::isspace(*index) || (*index == '\0')) {
                working = index;
                break;
            }
        }
        if (working == start_from ||
            std::distance(start_from, working) > max_values) {
            arg.present = false;
            return start_from;
        }
        std::copy(start_from, working, arg.value.begin());
        arg.present = true;
        return working;
    }

    // Numeric value - can use `parse_value`
    template <GCodeArgument Arg, std::forward_iterator Input, typename Limit>
    requires GCodeArgumentWithValue<Arg>
    static auto arg_parse_value(const Input& start_from, Limit stop_at,
                                Arg& arg) -> Input {
        using ValueType = std::decay_t<decltype(arg.value)>;
        auto ret = parse_value<ValueType>(start_from, stop_at);
        if (!ret.first.has_value()) {
            // Couldn't find the value
            arg.present = false;
            return start_from;
        }
        arg.value = ret.first.value();
        arg.present = true;
        return ret.second;
    }

    // No value at all - just mark arg as present
    template <GCodeArgument Arg, std::forward_iterator Input, typename Limit>
    static auto arg_parse_value(const Input& start_from, Limit stop_at,
                                Arg& arg) -> Input {
        static_cast<void>(stop_at);
        arg.present = true;
        return start_from;
    }
};

template <typename... GCodes>
class GroupParser {
  public:
    struct ParseError {};
    using ParseResult = std::variant<std::monostate, ParseError, GCodes...>;

    /*
     * gcode::parse_available is the main interface to the parser. It is capable
     * of handling any data structure that provides forward iterators (checked
     * in the template requirements).
     *
     * It will parse the first gcode available in the iterator passed in and
     * return it, if any. That means it should be called multiple times, passing
     * the iterator returned in .second each time, to iteratively parse out
     * gcodes from the input string.
     *
     * As long as it is only called on an input that should be complete (e.g.
     * ends with \r\n), then any failure to parse a gcode indicates a malformed
     * input (rather than an incomplete input).
     *
     * If invalid data leads the input (as in, a full match pass of all
     * subparsers starting at start_from fails) then the input is rejected, and
     * the returned iterator is set to the end.
     *
     * You use this by creating the class instance templated with the kinds of
     * gcodes you want to use:
     *
     * auto parser = GroupParser<GCode1, GCode2, ..., GCodeN>();
     *
     * This is mostly to make the template interface of parse_available less
     * complex; no state is held by the object created in this way.
     *
     * You then call parse_available using whatever iterators you have:
     *
     * std::array string = "G0 \r\n";
     * auto start = string.cbegin();
     * auto result = parse_available(start, string.cend());
     *
     * The result is a std::variant of all the kinds of gcodes sent as template
     * arguments to the class and std::monostate, which represents the empty
     * state, paired with an iterator pointing to the place that the next parse
     * run should start.
     */
    template <typename Input, typename Limit>
    requires std::forward_iterator<Input> &&
        std::sized_sentinel_for<Limit, Input>
    auto parse_available(Input start_from, Limit stop_at)
        -> std::pair<ParseResult, Input> {
        // Take out all whitespace at the head of the string
        start_from = gobble_whitespace(start_from, stop_at);
        // We need to prep a result to capture in the lambda in the fold
        // expression below because the fold expression can't really condense
        // its results any other way
        auto result = ParseResult(std::monostate());

        // the meat of the function is a template fold expression
        // (https://en.cppreference.com/w/cpp/language/fold), variant 1. This is
        // how you iterate at runtime over a set of template arguments.
        (  // the template-fold needs delimiters around it, so this starts the
           // fold We need something to condense the results of every parse
           // operation into the variant. This lambda definition, which captures
           // our predefined result and our current iterator by reference, does
           // that. It will actually get compiled to a couple different
           // implementations, and its job is to take the std::optional that
           // each gcode parser returns and coalesce the results into the
           // std::variant union type.
            [&result, &start_from](decltype(GCodes::parse(
                start_from, stop_at)) this_result) -> void {
                // the rule here is that we take the first match, so we only
                // store the result if there's nothing in our variant yet, and
                // the parse succeeded.
                if (this_result.first.has_value() &&
                    std::holds_alternative<std::monostate>(result)) {
                    result = *(this_result.first);
                    start_from = this_result.second;
                }
            }  // this ends the lambda definition
               // Once we've defined the lambda, we can call it immediately; the
               // argument we give it is the result of caling GCodes::parse().
               // GCodes:: will get expanded into the specific element of our
               // template parameter pack that we're currently processing The
               // comma is actually operator, which is an operator that returns
               // the thing on its right. It does nothing here other than
               // satisfy the requirement that a tmeplate fold must have an
               // operator.
            (GCodes::parse(start_from, stop_at)),
            ...);  // and our mandatory ... element ends the fold

        if (std::holds_alternative<std::monostate>(result)) {
            // After the fold completes, either result has not been filled in
            // and still holds the monostate it was created with, in which case
            // parsing has failed, which - given that this function requires a
            // fully terminated string - means that either
            // a) only whitespace was left between start_from and stop_at, in
            // which case we're just done or b) things other than whitespace
            // were between start_from and stop_at, in which case whatever was
            // in there was invalid. Either way we're done and should return the
            // end iterator, but we need to decide whether to return a monostate
            // or an error gcode and thus no further parsing should be done, so
            // we can return the end iterator
            if (gobble_whitespace(start_from, stop_at) == stop_at) {
                return std::make_pair(ParseResult(std::monostate()), stop_at);
            }
            return std::make_pair(ParseResult(ParseError()), stop_at);
        }
        // or result has been filled in, we have a gcode to return, and we
        // need to use start_from, which has been modified by whichever
        // lambda invocation it was that succeeded and now points to the
        // location in the input at which the next parse run should start.
        return std::make_pair(result, start_from);
    }
};
}  // namespace gcode
