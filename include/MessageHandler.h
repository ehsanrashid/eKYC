#pragma once

#include <atomic>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

#include "loggerwrapper.h"

extern const int ShardId;
extern LoggerWrapper Log;
// Forward declaration
namespace messages {
class IdentityMessage;
}

class MessageHandler final {
   public:
    MessageHandler() noexcept;
    ~MessageHandler() noexcept;

    bool exist_user(const std::string &identityNumber,
                    const std::string &name) noexcept;
    bool add_identity(messages::IdentityMessage &identity) noexcept;

   private:
    // No database connection for now
};
