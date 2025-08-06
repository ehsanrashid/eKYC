#include "MessageHandler.h"

#include <exception>
#include <iostream>

#include "config.h"
#include "helper.h"
#include "messages/Char64str.h"
#include "messages/IdentityMessage.h"
#include "messages/MessageHeader.h"

namespace {

void log_identity(messages::IdentityMessage &identity) {
    Log.info_fast(ShardId, "msg: {}", identity.msg().getCharValAsString());
    Log.info_fast(ShardId, "type: {}", identity.type().getCharValAsString());
    Log.info_fast(ShardId, "id: {}", identity.id().getCharValAsString());
    Log.info_fast(ShardId, "name: {}", identity.name().getCharValAsString());
    Log.info_fast(ShardId, "dateOfIssue: {}",
                  identity.dateOfIssue().getCharValAsString());
    Log.info_fast(ShardId, "dateOfExpiry: {}",
                  identity.dateOfExpiry().getCharValAsString());
    Log.info_fast(ShardId, "address: {}",
                  identity.address().getCharValAsString());
    Log.info_fast(ShardId, "verified: {}",
                  identity.verified().getCharValAsString());
}

}  // namespace

MessageHandler::MessageHandler() noexcept {
    Log.info_fast(ShardId,
                  "MessageHandler initialized (no database connection)");
}

MessageHandler::~MessageHandler() noexcept {
    Log.info_fast(ShardId, "MessageHandler destroyed");
}

// Check if user exists in database - simplified for testing
bool MessageHandler::exist_user(const std::string &identityNumber,
                                const std::string &name) noexcept {
    try {
        // For testing, return true if ID contains "421" (simulating existing
        // user)
        bool exists = (identityNumber.find("421") != std::string::npos);

        Log.info_fast(
            ShardId,
            exists ? "Verified: {} {} found in database (simulated)"
                   : "NOT verified: {} {} not found in database (simulated)",
            identityNumber, name);

        return exists;
    } catch (const std::exception &e) {
        Log.error_fast(ShardId, "Error during user existence check: {}",
                       e.what());
        return false;
    }
}

// Add user to database - simplified for testing
bool MessageHandler::add_identity(
    messages::IdentityMessage &identity) noexcept {
    try {
        std::string type = identity.type().getCharValAsString();
        std::string identityNumber = identity.id().getCharValAsString();
        std::string name = identity.name().getCharValAsString();

        Log.info_fast(
            ShardId,
            "Adding user to system (simulated): name={}, id={}, type={}", name,
            identityNumber, type);

        // For testing, always return true (simulating successful addition)
        Log.info_fast(
            ShardId,
            "User successfully added to system (simulated): {} {} ({})", name,
            identityNumber, type);

        return true;
    } catch (const std::exception &e) {
        Log.error_fast(ShardId, "Error adding user to system: {}", e.what());
        return false;
    }
}
