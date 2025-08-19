#include "MessageHandler.h"

#include <exception>
#include <iostream>

#include "Config.h"
#include "DatabaseFactory.h"
#include "DatabaseManager.h"
#include "PostgreDatabase.h"
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
    auto &cfg = Config::get();

    try {
        _pgConfig = DatabaseConfig(cfg.DB_HOST, cfg.DB_PORT, cfg.DB_NAME,
                                   cfg.DB_USER, cfg.DB_PASSWORD);
        Log.info_fast(ShardId, "Connected to PostGreSQL");
    } catch (const std::exception &e) {
        Log.info_fast(ShardId, "Error: {}", e.what());
    }
}

MessageHandler::~MessageHandler() noexcept {}

std::vector<char> MessageHandler::respond(
    const aeron_wrapper::FragmentData &fragmentData) noexcept {
    std::vector<char> buffer;

    messages::MessageHeader msgHeader;
    msgHeader.wrap(reinterpret_cast<char *>(const_cast<uint8_t *>(
                       fragmentData.atomicBuffer.buffer())),
                   0, 0, fragmentData.length);
    size_t offset = msgHeader.encodedLength();

    if (msgHeader.templateId() == messages::IdentityMessage::sbeTemplateId()) {
        messages::IdentityMessage identity;
        identity.wrapForDecode(reinterpret_cast<char *>(const_cast<uint8_t *>(
                                   fragmentData.atomicBuffer.buffer())),
                               offset, msgHeader.blockLength(),
                               msgHeader.version(), fragmentData.length);

        log_identity(identity);

        std::string msgType = identity.msg().getCharValAsString();
        bool isVerified =
            string_to_bool(identity.verified().getCharValAsString());

        // Check if this is an "Identity Verification Request" with
        // verified=false
        if (msgType == "Identity Verification Request" && !isVerified) {
            std::string name = identity.name().getCharValAsString();
            std::string id = identity.id().getCharValAsString();

            Log.info_fast(ShardId,
                          "Processing Identity Verification Request for: {} {}",
                          name, id);

            // Invoke verification method
            bool userExist = exist_user(id, name);

            if (userExist) {
                Log.info_fast(ShardId, "Verification successful for {} {}",
                              name, id);
                // Send back verified message with verified=true
                buffer = get_buffer(identity, true);
            } else {
                Log.info_fast(ShardId, "Verification failed for {} {}", name,
                              id);
                // Send back message with verified=false
                buffer = get_buffer(identity, false);
            }
        }
        // Check if this is an "Add User in System" request with verified=false
        else if (msgType == "Add User in System" && !isVerified) {
            std::string name = identity.name().getCharValAsString();
            std::string id = identity.id().getCharValAsString();

            Log.info_fast(ShardId,
                          "Processing Add User in System request for: {} {}",
                          name, id);

            // Add user to database
            bool identityAdded = add_identity(identity);

            if (identityAdded) {
                Log.info_fast(ShardId, "User addition successful for {} {}",
                              name, id);
                // Send back response with verified=true (user added
                // successfully)
                buffer = get_buffer(identity, true);
            } else {
                Log.info_fast(ShardId, "User addition failed for {} {}", name,
                              id);
                // Send back response with verified=false (user addition failed)
                buffer = get_buffer(identity, false);
            }
        } else if (isVerified) {
            Log.info_fast(ShardId, "Identity already verified: {}",
                          identity.name().getCharValAsString());
        } else {
            Log.info_fast(ShardId, "Message type '{}' - no action needed",
                          msgType);
        }

    } else {
        Log.error_fast(ShardId, "[Decoder] Unexpected template ID: {}",
                       msgHeader.templateId());
    }
    return buffer;
}

// Check if user exists in database
bool MessageHandler::exist_user(const std::string &identityNumber,
                                const std::string &name) noexcept {
    try {
        auto db = DatabaseFactory::create("postgresql", _pgConfig);
        DatabaseManager dbManager(std::move(db));

        std::string selectQuery =
            "SELECT identity_number, name FROM users WHERE identity_number = "
            "'" +
            identityNumber + "' AND name = '" + name + "'";

        // auto result = dbManager->exec(selectQuery);
        auto pgResult =
            dynamic_cast<PostgreResult *>(dbManager->exec(selectQuery).get());
        bool exists = !pgResult->empty();

        Log.info_fast(ShardId,
                      exists ? "Verified: {} {} found in database"
                             : "NOT verified: {} {} not found in database",
                      identityNumber, name);

        return exists;
    } catch (const std::exception &e) {
        Log.error_fast(ShardId,
                       "Database query error during user existence check: {}",
                       e.what());
        return false;
    }
}

// Add user to database
bool MessageHandler::add_identity(
    messages::IdentityMessage &identity) noexcept {
    auto db = DatabaseFactory::create("postgresql", _pgConfig);
    DatabaseManager dbManager(std::move(db));

    try {
        std::string type = identity.type().getCharValAsString();
        std::string identityNumber = identity.id().getCharValAsString();
        std::string name = identity.name().getCharValAsString();
        std::string dateOfIssue = identity.dateOfIssue().getCharValAsString();
        std::string dateOfExpiry = identity.dateOfExpiry().getCharValAsString();
        std::string address = identity.address().getCharValAsString();

        Log.info_fast(ShardId, "Adding user to system: name={}, id={}, type={}",
                      name, identityNumber, type);

        // Check if user already exists using the reusable method
        if (exist_user(identityNumber, name)) {
            Log.info_fast(ShardId, "User already exists in system: {} {} ({})",
                          name, identityNumber, type);
            return false;  // User already exists, don't add duplicate
        }

        Log.info_fast(
            ShardId,
            "User not found in system, proceeding with addition: {} {}", name,
            identityNumber);

        // Insert user into database
        std::string insertQuery =
            "INSERT INTO users (type, identity_number, name, date_of_issue, "
            "date_of_expiry, address) "
            "VALUES ('" +
            type + "', '" + identityNumber + "', '" + name + "', '" +
            dateOfIssue + "', '" + dateOfExpiry + "', '" + address + "')";

        dbManager->exec(insertQuery);

        Log.info_fast(ShardId, "User successfully added to system: {} {} ({})",
                      name, identityNumber, type);

        return true;
    } catch (const std::exception &e) {
        Log.error_fast(ShardId, "Database error while adding user: {}",
                       e.what());
        return false;
    }
}

// Send response message
std::vector<char> MessageHandler::get_buffer(
    messages::IdentityMessage &originalIdentity,
    bool verificationResult) noexcept {
    std::vector<char> buffer;
    try {
        using namespace messages;
        const size_t bufferCapacity =
            MessageHeader::encodedLength() + IdentityMessage::sbeBlockLength();
        buffer.resize(bufferCapacity);
        size_t offset = 0;

        // Encode header
        MessageHeader msgHeader;
        msgHeader.wrap(buffer.data(), offset, 0, bufferCapacity);
        msgHeader.blockLength(IdentityMessage::sbeBlockLength());
        msgHeader.templateId(IdentityMessage::sbeTemplateId());
        msgHeader.schemaId(IdentityMessage::sbeSchemaId());
        msgHeader.version(IdentityMessage::sbeSchemaVersion());
        offset += msgHeader.encodedLength();

        // Encode response message
        IdentityMessage identity;
        identity.wrapForEncode(buffer.data(), offset, bufferCapacity);

        // Copy original data but update verification status and message
        identity.msg().putCharVal("Identity Verification Response");
        identity.type().putCharVal(
            originalIdentity.type().getCharValAsString());
        identity.id().putCharVal(originalIdentity.id().getCharValAsString());
        identity.name().putCharVal(
            originalIdentity.name().getCharValAsString());
        identity.dateOfIssue().putCharVal(
            originalIdentity.dateOfIssue().getCharValAsString());
        identity.dateOfExpiry().putCharVal(
            originalIdentity.dateOfExpiry().getCharValAsString());
        identity.address().putCharVal(
            originalIdentity.address().getCharValAsString());
        identity.verified().putCharVal(verificationResult ? "true" : "false");
    } catch (const std::exception &e) {
        Log.error_fast(ShardId, "Error sending response: {}", e.what());
    }
    return buffer;
}
