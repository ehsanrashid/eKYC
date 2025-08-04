#include "eKYCEngine.h"

#include <chrono>
#include <exception>
#include <iostream>
#include <vector>

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

eKYCEngine::eKYCEngine() : running_(false) {
    try {
        aeron_ = std::make_unique<aeron_wrapper::Aeron>(AeronDir);
        Log.info_fast(ShardId, "Connected to Aeron Media Driver...");
        subscription_ = aeron_->create_subscription(SubscriptionChannel,  //
                                                    SubscriptionStreamId);
        publication_ = aeron_->create_publication(PublicationChannel,  //
                                                  PublicationStreamId);

        db_ = std::make_unique<pg_wrapper::Database>(
            "localhost", "5432", "ekycdb", "huzaifa", "3214");
        Log.info_fast(ShardId, "Connected to PostGreSQL");

        running_ = true;
    } catch (const std::exception &e) {
        Log.info_fast(ShardId, "Error: {}", e.what());
    }
}

eKYCEngine::~eKYCEngine() { stop(); }

void eKYCEngine::start() {
    if (!running_) return;

    Log.info_fast(ShardId, "Starting eKYC engine...");
    // Start background msg processing
    backgroundPoller_ = subscription_->start_background_polling(
        [this](const aeron_wrapper::FragmentData &fragmentData) {
            process_message(fragmentData);
        });
}

void eKYCEngine::stop() {
    if (!running_) return;

    if (backgroundPoller_) {
        backgroundPoller_->stop();
    }
    if (db_) {
        db_->close();
        Log.info_fast(ShardId, "PostGreSQL connection closed!");
    }
    running_ = false;
    Log.info_fast(ShardId, "eKYC engine stopped.");
}

// Check if user exists in database (with verification logging)
bool eKYCEngine::exist_user(const std::string &identityNumber,
                            const std::string &name) {
    if (!db_) {
        Log.error_fast(ShardId,
                       "Database connection not available for user check");
        return false;
    }

    try {
        std::string query =
            "SELECT identity_number, name FROM users WHERE identity_number = "
            "'" +
            identityNumber + "' AND name = '" + name + "'";
        auto result = db_->exec(query);

        bool exists = !result.empty();

        if (exists) {
            Log.info_fast(ShardId, "Verified: {} {} found in database",
                          identityNumber, name);
        } else {
            Log.info_fast(ShardId, "NOT verified: {} {} not found in database",
                          identityNumber, name);
        }

        return exists;
    } catch (const pg_wrapper::DatabaseError &e) {
        Log.error_fast(ShardId,
                       "Database query error during user existence check: {}",
                       e.what());
        return false;
    }
}

// Add user to system
bool eKYCEngine::add_identity(messages::IdentityMessage &identity) {
    if (!db_) {
        Log.error_fast(ShardId,
                       "Database connection not available for adding user");
        return false;
    }

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

        auto result = db_->exec(insertQuery);

        Log.info_fast(ShardId, "User successfully added to system: {} {} ({})",
                      name, identityNumber, type);
        return true;

    } catch (const pg_wrapper::DatabaseError &e) {
        Log.error_fast(ShardId, "Database error while adding user: {}",
                       e.what());
        return false;
    } catch (const std::exception &e) {
        Log.error_fast(ShardId, "Error adding user to system: {}", e.what());
        return false;
    }
}

// Add verification method
void eKYCEngine::verify_and_respond(messages::IdentityMessage &identity) {
    std::string msgType = identity.msg().getCharValAsString();
    bool isVerified = string_to_bool(identity.verified().getCharValAsString());

    // Check if this is an "Identity Verification Request" with verified=false
    if (msgType == "Identity Verification Request" && !isVerified) {
        std::string name = identity.name().getCharValAsString();
        std::string id = identity.id().getCharValAsString();

        Log.info_fast(ShardId,
                      "Processing Identity Verification Request for: {} {}",
                      name, id);

        // Invoke verification method
        bool userExist = exist_user(id, name);

        if (userExist) {
            Log.info_fast(ShardId, "Verification successful for {} {}", name,
                          id);
            // Send back verified message with verified=true
            send_response(identity, true);
        } else {
            Log.info_fast(ShardId, "Verification failed for {} {}", name, id);
            // Send back message with verified=false
            send_response(identity, false);
        }
    }
    // Check if this is an "Add User in System" request with verified=false
    else if (msgType == "Add User in System" && !isVerified) {
        std::string name = identity.name().getCharValAsString();
        std::string id = identity.id().getCharValAsString();

        Log.info_fast(ShardId,
                      "Processing Add User in System request for: {} {}", name,
                      id);

        // Add user to database
        bool addResult = add_identity(identity);

        if (addResult) {
            Log.info_fast(ShardId, "User addition successful for {} {}", name,
                          id);
            // Send back response with verified=true (user added successfully)
            send_response(identity, true);
        } else {
            Log.info_fast(ShardId, "User addition failed for {} {}", name, id);
            // Send back response with verified=false (user addition failed)
            send_response(identity, false);
        }
    } else if (isVerified) {
        Log.info_fast(ShardId, "Identity already verified: {}",
                      identity.name().getCharValAsString());
    } else {
        Log.info_fast(ShardId, "Message type '{}' - no action needed", msgType);
    }
}

// Send response message
void eKYCEngine::send_response(messages::IdentityMessage &originalIdentity,
                               bool verificationResult) {
    if (!publication_) {
        Log.error_fast(ShardId,
                       "Publication not available for sending response");
        return;
    }

    try {
        using namespace messages;
        const size_t bufferCapacity =
            MessageHeader::encodedLength() + IdentityMessage::sbeBlockLength();
        std::vector<char> sbeBuffer(bufferCapacity, 0);
        size_t offset = 0;

        // Encode header
        MessageHeader msgHeader;
        msgHeader.wrap(sbeBuffer.data(), offset, 0, bufferCapacity);
        msgHeader.blockLength(IdentityMessage::sbeBlockLength());
        msgHeader.templateId(IdentityMessage::sbeTemplateId());
        msgHeader.schemaId(IdentityMessage::sbeSchemaId());
        msgHeader.version(IdentityMessage::sbeSchemaVersion());
        offset += msgHeader.encodedLength();

        // Encode response message
        IdentityMessage identity;
        identity.wrapForEncode(sbeBuffer.data(), offset, bufferCapacity);

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

        // Send the response
        send_response(sbeBuffer);
    } catch (const std::exception &e) {
        Log.error_fast(ShardId, "Error sending response: {}", e.what());
    }
}

void eKYCEngine::process_message(
    const aeron_wrapper::FragmentData &fragmentData) {
    ++packetsReceived_;
    try {
        messages::MessageHeader msgHeader;
        msgHeader.wrap(reinterpret_cast<char *>(
                           const_cast<uint8_t *>(fragmentData.buffer)),
                       0, 0, fragmentData.length);
        size_t offset = msgHeader.encodedLength();

        if (msgHeader.templateId() ==
            messages::IdentityMessage::sbeTemplateId()) {
            messages::IdentityMessage identity;
            identity.wrapForDecode(
                reinterpret_cast<char *>(
                    const_cast<uint8_t *>(fragmentData.buffer)),
                offset, msgHeader.blockLength(), msgHeader.version(),
                fragmentData.length);

            log_identity(identity);

            verify_and_respond(identity);
        } else {
            Log.error_fast(ShardId, "[Decoder] Unexpected template ID: {}",
                           msgHeader.templateId());
        }

    } catch (const std::exception &e) {
        Log.error_fast(ShardId, "Error: {}", e.what());
    }
}

void eKYCEngine::send_response(std::vector<char> &buffer) {
    if (!publication_) return;

    auto result = publication_->offer(
        reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    if (result == aeron_wrapper::PublicationResult::SUCCESS) {
        Log.info_fast(ShardId, "Response sent successfully");
    } else {
        Log.error_fast(ShardId, "Failed to send response: {}",
                       static_cast<int>(result));
    }
}
