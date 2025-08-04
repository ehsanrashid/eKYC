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
    void handle_message(const aeron_wrapper::FragmentData &fragmentData);

    bool exist_user(const std::string &identityNumber, const std::string &name);
    bool add_identity(messages::IdentityMessage &identity);
    void verify_and_respond(messages::IdentityMessage &identity);

   private:
    std::unique_ptr<pg_wrapper::Database> db_;
};
