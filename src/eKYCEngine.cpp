#include "eKYCEngine.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <iostream>
#include <vector>

#include "Char64str.h"
#include "IdentityMessage.h"
#include "MessageHeader.h"
#include "helper.h"

namespace {

void log_identity(messages::IdentityMessage& identity) {
    Log.info_fast("msg: {}", identity.msg().getCharValAsString());
    Log.info_fast("type: {}", identity.type().getCharValAsString());
    Log.info_fast("id: {}", identity.id().getCharValAsString());
    Log.info_fast("name: {}", identity.name().getCharValAsString());
    Log.info_fast("dateOfIssue: {}",
                  identity.dateOfIssue().getCharValAsString());
    Log.info_fast("dateOfExpiry: {}",
                  identity.dateOfExpiry().getCharValAsString());
    Log.info_fast("address: {}", identity.address().getCharValAsString());
    Log.info_fast("verified: {}", identity.verified().getCharValAsString());
}

}  // namespace

eKYCEngine::eKYCEngine() : running_(false) {
    try {
        aeron_ = std::make_unique<aeron_wrapper::Aeron>(AeronDir);
        Log.info_fast("Connected to Aeron Media Driver...");
        subscription_ = aeron_->create_subscription(SubscriptionChannel,  //
                                                    SubscriptionStreamId);
        publication_ = aeron_->create_publication(PublicationChannel,  //
                                                  PublicationStreamId);

        db_ = std::make_unique<pg_wrapper::Database>(
            "localhost", "5432", "ekycdb", "huzaifa", "3214");
        Log.info_fast("Connected to PostGreSQL");

        running_ = true;
    } catch (const std::exception& e) {
        Log.info_fast("Error: {}", e.what());
    }
}

eKYCEngine::~eKYCEngine() { stop(); }

void eKYCEngine::start() {
    if (!running_) return;
    Log.info_fast("Starting eKYC engine...");
    // Start background msg processing
    backgroundPoller_ = subscription_->start_background_polling(
        [this](const aeron_wrapper::FragmentData& fragmentData) {
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
        Log.info_fast("PostGreSQL connection closed!");
    }
    running_ = false;
    Log.info_fast("eKYC engine stopped.");
}

// Check if user exists in database
bool eKYCEngine::user_exists(const std::string& identityNumber,
                             const std::string& name) {
    if (!db_) {
        Log.error_fast("Database connection not available for user check");
        return false;
    }

    try {
        std::string query =
            "SELECT identity_number, name FROM users WHERE identity_number = "
            "'" +
            identityNumber + "' AND name = '" + name + "'";
        auto result = db_->exec(query);
        return !result.empty();
    } catch (const pg_wrapper::DatabaseError& e) {
        Log.error_fast("Database query error during user existence check: {}",
                       e.what());
        return false;
    }
}

// Add user to system
bool eKYCEngine::add_user_to_system(messages::IdentityMessage& identity) {
    if (!db_) {
        Log.error_fast("Database connection not available for adding user");
        return false;
    }

    try {
        std::string type = identity.type().getCharValAsString();
        std::string identityNumber = identity.id().getCharValAsString();
        std::string name = identity.name().getCharValAsString();
        std::string dateOfIssue = identity.dateOfIssue().getCharValAsString();
        std::string dateOfExpiry = identity.dateOfExpiry().getCharValAsString();
        std::string address = identity.address().getCharValAsString();

        Log.info_fast("Adding user to system: name={}, id={}, type={}", name,
                      identityNumber, type);

        // Check if user already exists using the reusable method
        if (user_exists(identityNumber, name)) {
            Log.info_fast("User already exists in system: {} {} ({})", name,
                          identityNumber, type);
            return false;  // User already exists, don't add duplicate
        }

        Log.info_fast(
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

        Log.info_fast("User successfully added to system: {} {} ({})", name,
                      identityNumber, type);
        return true;

    } catch (const pg_wrapper::DatabaseError& e) {
        Log.error_fast("Database error while adding user: {}", e.what());
        return false;
    } catch (const std::exception& e) {
        Log.error_fast("Error adding user to system: {}", e.what());
        return false;
    }
}

// Send response message
void eKYCEngine::send_response(messages::IdentityMessage& originalIdentity,
                               bool verificationResult) {
    if (!publication_) {
        Log.error_fast("Publication not available for sending response");
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
        IdentityMessage response;
        response.wrapForEncode(sbeBuffer.data(), offset, bufferCapacity);

        // Copy original data but update verification status and message
        response.msg().putCharVal("Identity Verification Response");
        response.type().putCharVal(
            originalIdentity.type().getCharValAsString());
        response.id().putCharVal(originalIdentity.id().getCharValAsString());
        response.name().putCharVal(
            originalIdentity.name().getCharValAsString());
        response.dateOfIssue().putCharVal(
            originalIdentity.dateOfIssue().getCharValAsString());
        response.dateOfExpiry().putCharVal(
            originalIdentity.dateOfExpiry().getCharValAsString());
        response.address().putCharVal(
            originalIdentity.address().getCharValAsString());
        response.verified().putCharVal(verificationResult ? "true" : "false");

        // Send the response
        if (publication_->is_connected()) {
            auto result = publication_->offer(
                reinterpret_cast<const uint8_t*>(sbeBuffer.data()),
                bufferCapacity);
            if (result == aeron_wrapper::PublicationResult::SUCCESS) {
                Log.info_fast("Response sent successfully: {} for {} {}",
                              verificationResult ? "VERIFIED" : "NOT VERIFIED",
                              originalIdentity.name().getCharValAsString(),
                              originalIdentity.id().getCharValAsString());
            } else {
                Log.error_fast("Failed to send response: {}",
                               static_cast<int>(result));
            }
        } else {
            Log.error_fast("Publication not connected, cannot send response");
        }
    } catch (const std::exception& e) {
        Log.error_fast("Error sending response: {}", e.what());
    }
}

// Add verification method
void eKYCEngine::verify_and_respond(messages::IdentityMessage& identity) {
    std::string msg_type = identity.msg().getCharValAsString();
    bool is_verified = string_to_bool(identity.verified().getCharValAsString());

    // Check if this is an "Identity Verification Request" with verified=false
    if (msg_type == "Identity Verification Request" && !is_verified) {
        std::string name = identity.name().getCharValAsString();
        std::string id = identity.id().getCharValAsString();

        Log.info_fast("Processing Identity Verification Request for: {} {}",
                      name, id);

        // Invoke verification method
        bool verificationResult = verify_identity(name, id);

        if (verificationResult) {
            Log.info_fast("Verification successful for {} {}", name, id);
            // Send back verified message with verified=true
            send_response(identity, true);
        } else {
            Log.info_fast("Verification failed for {} {}", name, id);
            // Send back message with verified=false
            send_response(identity, false);
        }
    }
    // Check if this is an "Add User in System" request with verified=false
    else if (msg_type == "Add User in System" && !is_verified) {
        std::string name = identity.name().getCharValAsString();
        std::string id = identity.id().getCharValAsString();

        Log.info_fast("Processing Add User in System request for: {} {}", name,
                      id);

        // Add user to database
        bool add_result = add_user_to_system(identity);

        if (add_result) {
            Log.info_fast("User addition successful for {} {}", name, id);
            // Send back response with verified=true (user added successfully)
            send_response(identity, true);
        } else {
            Log.info_fast("User addition failed for {} {}", name, id);
            // Send back response with verified=false (user addition failed)
            send_response(identity, false);
        }
    } else if (is_verified) {
        Log.info_fast("Identity already verified: {}",
                      identity.name().getCharValAsString());
    } else {
        Log.info_fast("Message type '{}' - no action needed", msg_type);
    }
}

bool eKYCEngine::verify_identity(const std::string& name,
                                 const std::string& id) {
    Log.info_fast("Verifying identity: name={}, id={}", name, id);

    // Use the reusable user_exists method
    bool exists = user_exists(id, name);

    if (exists) {
        Log.info_fast("Identity verified: {} {} found in database", id, name);
        return true;
    } else {
        Log.info_fast("Identity NOT verified: {} {} not found in database", id,
                      name);
        return false;
    }
}

void eKYCEngine::process_message(
    const aeron_wrapper::FragmentData& fragmentData) {
    try {
        messages::MessageHeader msgHeader;
        msgHeader.wrap(
            reinterpret_cast<char*>(const_cast<uint8_t*>(fragmentData.buffer)),
            0, 0, fragmentData.length);
        size_t offset = msgHeader.encodedLength();

        if (msgHeader.templateId() ==
            messages::IdentityMessage::sbeTemplateId()) {
            messages::IdentityMessage identity;
            identity.wrapForDecode(reinterpret_cast<char*>(const_cast<uint8_t*>(
                                       fragmentData.buffer)),
                                   offset, msgHeader.blockLength(),
                                   msgHeader.version(), fragmentData.length);

            log_identity(identity);

            verify_and_respond(identity);
        } else {
            Log.error_fast("[Decoder] Unexpected template ID: {}",
                           msgHeader.templateId());
        }

    } catch (const std::exception& e) {
        Log.error_fast("Error: {}", e.what());
    }
}
