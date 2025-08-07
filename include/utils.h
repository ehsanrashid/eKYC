#pragma once

#include <sstream>
#include <string>

namespace ekyc_utils {

/**
 * Simple SQL injection protection - escape single quotes
 */
inline std::string escape_sql_string(const std::string& input) {
    std::string escaped = input;
    size_t pos = 0;
    while ((pos = escaped.find("'", pos)) != std::string::npos) {
        escaped.replace(pos, 1, "''");
        pos += 2;
    }
    return escaped;
}

/**
 * Build SQL INSERT statement with escaped parameters
 */
inline std::string build_insert_sql(const std::string& table,
                                    const std::vector<std::string>& columns,
                                    const std::vector<std::string>& values) {
    std::ostringstream oss;
    oss << "INSERT INTO " << table << " (";

    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << columns[i];
    }

    oss << ") VALUES (";

    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "'" << escape_sql_string(values[i]) << "'";
    }

    oss << ")";
    return oss.str();
}

/**
 * Build SQL SELECT statement with WHERE clause
 */
inline std::string build_select_sql(const std::string& table,
                                    const std::vector<std::string>& columns,
                                    const std::string& where_column,
                                    const std::string& where_value,
                                    const std::string& and_column = "",
                                    const std::string& and_value = "") {
    std::ostringstream oss;
    oss << "SELECT ";

    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << columns[i];
    }

    oss << " FROM " << table << " WHERE " << where_column << " = '"
        << escape_sql_string(where_value) << "'";

    if (!and_column.empty() && !and_value.empty()) {
        oss << " AND " << and_column << " = '" << escape_sql_string(and_value)
            << "'";
    }

    return oss.str();
}

}  // namespace ekyc_utils