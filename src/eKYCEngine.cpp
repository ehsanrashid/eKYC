#include "eKYCEngine.h"

#include <chrono>
#include <exception>
#include <iostream>

#include "helper.h"

eKYCEngine::eKYCEngine() noexcept
    : running_(false),
      packetsReceived_(0),
      errorCount_(0),
      consecutiveErrors_(0),
      messageHandler_() {
    try {
        messaging_ = std::make_unique<Messaging>();
        Log.info_fast(ShardId,
                      "eKYC Engine initialized with Messaging component");
    } catch (const std::exception &e) {
        Log.error_fast(ShardId, "Error initializing eKYC Engine: {}", e.what());
    }
}

eKYCEngine::~eKYCEngine() noexcept { stop(); }

void eKYCEngine::start() noexcept {
    if (!messaging_) {
        Log.error_fast(ShardId, "Messaging component not initialized");
        return;
    }

    try {
        // Initialize Messaging component (Aeron setup)
        if (!messaging_->initialize()) {
            Log.error_fast(ShardId, "Failed to initialize Messaging component");
            return;
        }

        running_ = true;

        // Start processing threads for each shard
        processingThreads_.reserve(config::NUM_SHARDS);
        for (uint8_t shardId = 0; shardId < config::NUM_SHARDS; ++shardId) {
            processingThreads_.emplace_back(&eKYCEngine::process_shard_messages,
                                            this, shardId);
        }

        Log.info_fast(
            ShardId,
            "eKYC engine started with {} sharded message processing threads",
            config::NUM_SHARDS);
    } catch (const std::exception &e) {
        Log.error_fast(ShardId, "Error starting eKYC engine: {}", e.what());
    }
}

void eKYCEngine::stop() noexcept {
    if (!running_) return;

    running_ = false;

    // Wait for all processing threads to finish
    for (auto &thread : processingThreads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    // Shutdown messaging component
    if (messaging_) {
        messaging_->shutdown();
    }

    Log.info_fast(ShardId, "eKYC engine stopped.");
}

void eKYCEngine::process_shard_messages(uint8_t shardId) noexcept {
    Log.info_fast(shardId, "Shard {} message processing thread started",
                  shardId);

    while (running_) {
        try {
            // Get access to sharded queues
            auto &queues = messaging_->getQueue();
            auto &queue = queues[shardId];

            // Process all available messages in this shard
            while (running_) {
                auto identity = queue.dequeue();
                if (!identity.has_value()) {
                    break;  // No more messages in this shard
                }

                ++packetsReceived_;
                process_identity_message(identity.value(), shardId);
                // Reset consecutive errors on successful processing
                consecutiveErrors_.store(0);
            }

            // Small delay to prevent busy waiting
            if (running_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

        } catch (const std::exception &e) {
            ++errorCount_;
            ++consecutiveErrors_;
            Log.error_fast(shardId,
                           "Error in shard {} message processing: {} "
                           "(consecutive errors: {})",
                           shardId, e.what(), consecutiveErrors_.load());

            // Circuit breaker: if too many consecutive errors, pause processing
            if (consecutiveErrors_.load() > 10) {
                Log.error_fast(shardId,
                               "Circuit breaker activated for shard {}, "
                               "pausing for 5 seconds",
                               shardId);
                std::this_thread::sleep_for(std::chrono::seconds(5));
                consecutiveErrors_.store(0);
            }
        }
    }

    Log.info_fast(shardId, "Shard {} message processing thread exiting",
                  shardId);
}

void eKYCEngine::process_identity_message(messages::IdentityMessage &identity,
                                          uint8_t shardId) noexcept {
    try {
        Log.info_fast(shardId,
                      "Processing identity message for: {} {} (msg: {})",
                      identity.name().getCharValAsString(),
                      identity.id().getCharValAsString(),
                      identity.msg().getCharValAsString());

        std::string msgType = identity.msg().getCharValAsString();
        bool isVerified =
            string_to_bool(identity.verified().getCharValAsString());

        // Check if this is an "Identity Verification Request" with
        // verified=false
        if (msgType == "Identity Verification Request" && !isVerified) {
            std::string name = identity.name().getCharValAsString();
            std::string id = identity.id().getCharValAsString();

            Log.info_fast(shardId,
                          "Processing Identity Verification Request for: {} {}",
                          name, id);

            // Invoke verification method
            bool userExist = messageHandler_.exist_user(id, name);

            if (userExist) {
                Log.info_fast(shardId, "Verification successful for {} {}",
                              name, id);
                // Create response with verified=true
                messages::IdentityMessage responseIdentity =
                    create_response_message(identity, true, shardId);
                messaging_->sendResponse(responseIdentity);
            } else {
                Log.info_fast(shardId, "Verification failed for {} {}", name,
                              id);
                // Create response with verified=false
                messages::IdentityMessage responseIdentity =
                    create_response_message(identity, false, shardId);
                messaging_->sendResponse(responseIdentity);
            }
        }
        // Check if this is an "Add User in System" request with verified=false
        else if (msgType == "Add User in System" && !isVerified) {
            std::string name = identity.name().getCharValAsString();
            std::string id = identity.id().getCharValAsString();

            Log.info_fast(shardId,
                          "Processing Add User in System request for: {} {}",
                          name, id);

            // Create a mutable copy for database operations
            messages::IdentityMessage mutableIdentity = identity;

            // Add user to database
            bool identityAdded = messageHandler_.add_identity(mutableIdentity);

            if (identityAdded) {
                Log.info_fast(shardId, "User addition successful for {} {}",
                              name, id);
                // Send back response with verified=true (user added
                // successfully)
                messages::IdentityMessage responseIdentity =
                    create_response_message(identity, true, shardId);
                messaging_->sendResponse(responseIdentity);
            } else {
                Log.info_fast(shardId, "User addition failed for {} {}", name,
                              id);
                // Send back response with verified=false (user addition failed)
                messages::IdentityMessage responseIdentity =
                    create_response_message(identity, false, shardId);
                messaging_->sendResponse(responseIdentity);
            }
        } else if (isVerified) {
            Log.info_fast(shardId, "Identity already verified: {}",
                          identity.name().getCharValAsString());
        } else {
            Log.info_fast(shardId, "Message type '{}' - no action needed",
                          msgType);
        }

    } catch (const std::exception &e) {
        Log.error_fast(shardId, "Error processing identity message: {}",
                       e.what());
    }
}

messages::IdentityMessage eKYCEngine::create_response_message(
    messages::IdentityMessage &original, bool verified,
    uint8_t shardId) noexcept {
    try {
        // Create a new buffer for the response
        std::vector<char> buffer;
        const size_t bufferCapacity =
            messages::MessageHeader::encodedLength() +
            messages::IdentityMessage::sbeBlockLength();
        buffer.resize(bufferCapacity);

        size_t offset = 0;

        // Encode header
        messages::MessageHeader msgHeader;
        msgHeader.wrap(buffer.data(), offset, 0, bufferCapacity);
        msgHeader.blockLength(messages::IdentityMessage::sbeBlockLength());
        msgHeader.templateId(messages::IdentityMessage::sbeTemplateId());
        msgHeader.schemaId(messages::IdentityMessage::sbeSchemaId());
        msgHeader.version(messages::IdentityMessage::sbeSchemaVersion());
        offset += msgHeader.encodedLength();

        // Encode response message
        messages::IdentityMessage responseIdentity;
        responseIdentity.wrapForEncode(buffer.data(), offset, bufferCapacity);

        // Copy original data but update verification status and message
        responseIdentity.msg().putCharVal("Identity Verification Response");
        responseIdentity.type().putCharVal(
            original.type().getCharValAsString());
        responseIdentity.id().putCharVal(original.id().getCharValAsString());
        responseIdentity.name().putCharVal(
            original.name().getCharValAsString());
        responseIdentity.dateOfIssue().putCharVal(
            original.dateOfIssue().getCharValAsString());
        responseIdentity.dateOfExpiry().putCharVal(
            original.dateOfExpiry().getCharValAsString());
        responseIdentity.address().putCharVal(
            original.address().getCharValAsString());
        responseIdentity.verified().putCharVal(verified ? "true" : "false");

        return responseIdentity;
    } catch (const std::exception &e) {
        Log.error_fast(shardId, "Error creating response message: {}",
                       e.what());
        // Return a default message in case of error
        return original;
    }
}
