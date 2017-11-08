//
// Created by valdemar on 25.10.17.
//

#pragma once

#include <string>

#define X_PRIMITIVE_TYPES_LIST \
    X(begin, b) \
    X(end, e) \
    X(circle, c) \
    X(rectangle, r) \
    X(line, l) \
    X(message, m) \
    X(unit, u) \
    X(area, a) \

enum class PrimitiveType {
#define X(type, s) type,
    X_PRIMITIVE_TYPES_LIST
#undef X
    types_count
};

inline PrimitiveType primitive_type_from_char(const char str) {
#define X(type, shortType) if (str == #shortType[0]) return PrimitiveType::type;
    X_PRIMITIVE_TYPES_LIST
#undef X
    return PrimitiveType::types_count;
}