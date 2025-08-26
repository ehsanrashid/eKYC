#pragma once

#include <cstdint>

enum class StepResult { SUCCESS, FAILED };

enum MessageType : std::int8_t {
    MT_ORDER,
    MT_CANCEL,
};
