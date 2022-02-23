/**
 * @file xt1511.cpp
 * @brief Implementation of xt1511 functions
 *
 */
#include "core/xt1511.hpp"

auto xt1511::operator==(const xt1511::XT1511& l, const xt1511::XT1511& r)
    -> bool {
    return ((l.g == r.g) && (l.r == r.r) && (l.b == r.b) && (l.w == r.w));
}
