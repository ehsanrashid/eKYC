#pragma once

#include <atomic>
#include <iosfwd>
#include <memory>
#include <string>

#include "aeron_wrapper.h"
#include "loggerwrapper.h"
#include "pg_wrapper.h"

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

    std::vector<char> respond(
        const aeron_wrapper::FragmentData &fragmentData) noexcept;

    bool exist_user(const std::string &identityNumber,
                    const std::string &name) noexcept;
    bool add_identity(messages::IdentityMessage &identity) noexcept;
    std::vector<char> get_buffer(messages::IdentityMessage &originalIdentity,
                                 bool verificationResult) noexcept;

   private:
    std::unique_ptr<pg_wrapper::Database> db_;
};
