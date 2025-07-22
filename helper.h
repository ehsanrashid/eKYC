#pragma once

#include <iostream>
#include <sstream>
#include <string>

inline bool string_to_bool(const std::string& str) {
    std::istringstream is(str);
    bool b;
    is >> std::boolalpha >> b;  // enables reading "true"/"false"
    if (is.fail()) {
        // Try as integer
        is.clear();
        is.str(str);
        int i;
        is >> i;
        return i != 0;
    }
    return b;
}