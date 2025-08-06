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
                auto messageVariant = queue.dequeue();
                if (!messageVariant.has_value()) {
                    break;  // No more messages in this shard
                }

                ++packetsReceived_;

                // Handle the variant like eLoan does
                std::visit(
                    [this, shardId](auto &&message) {
                        using T = std::decay_t<decltype(message)>;
                        if constexpr (std::is_same_v<T, IdentityData>) {
                            process_identity_message(message, shardId);
                        }
                    },
                    messageVariant.value());

                // Reset consecutive errors on successful processing
                consecutiveErrors_.store(0);
            }

            // Small delay to prevent busy waiting
            if (running_) {
                std::this_thread::sleep_for(std::chrono::microseconds(
                    100));  // Reduced from 1ms to 100μs
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

void eKYCEngine::process_identity_message(IdentityData &identity,
                                          uint8_t shardId) noexcept {
    try {
        // Now we have simple string data like eLoan
        Log.info_fast(shardId,
                      "Processing identity message for: {} {} (msg: {})",
                      identity.name, identity.id, identity.msg);

        std::string msgType = identity.msg;
        bool isVerified = string_to_bool(identity.verified);

        // Check if this is an "Identity Verification Request" with
        // verified=false
        if (msgType == "Identity Verification Request" && !isVerified) {
            std::string name = identity.name;
            std::string id = identity.id;

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
            std::string name = identity.name;
            std::string id = identity.id;

            Log.info_fast(shardId,
                          "Processing Add User in System request for: {} {}",
                          name, id);

            // Create a simple message handler call
            // Create a proper SBE message for the database operation
            std::vector<uint8_t> raw_buffer(
                messages::MessageHeader::encodedLength() +
                messages::IdentityMessage::sbeBlockLength() +
                identity.msg.size() + identity.type.size() +
                identity.id.size() + identity.name.size() +
                identity.dateOfIssue.size() + identity.dateOfExpiry.size() +
                identity.address.size() + identity.verified.size());

            aeron::concurrent::AtomicBuffer atomic_buffer(raw_buffer.data(),
                                                          raw_buffer.size());

            messages::MessageHeader header_encoder;
            header_encoder
                .wrap(reinterpret_cast<char *>(raw_buffer.data()), 0, 0,
                      raw_buffer.size())
                .blockLength(messages::IdentityMessage::sbeBlockLength())
                .templateId(messages::IdentityMessage::sbeTemplateId())
                .schemaId(messages::IdentityMessage::sbeSchemaId())
                .version(messages::IdentityMessage::sbeSchemaVersion());

            messages::IdentityMessage identity_encoder;
            identity_encoder.wrapForEncode(
                reinterpret_cast<char *>(raw_buffer.data()),
                messages::MessageHeader::encodedLength(), raw_buffer.size());

            // Set the identity data
            identity_encoder.msg().putCharVal(identity.msg);
            identity_encoder.type().putCharVal(identity.type);
            identity_encoder.id().putCharVal(identity.id);
            identity_encoder.name().putCharVal(identity.name);
            identity_encoder.dateOfIssue().putCharVal(identity.dateOfIssue);
            identity_encoder.dateOfExpiry().putCharVal(identity.dateOfExpiry);
            identity_encoder.address().putCharVal(identity.address);
            identity_encoder.verified().putCharVal(identity.verified);

            // Create a proper IdentityMessage for the database call
            messages::IdentityMessage sbeIdentity;
            sbeIdentity.wrapForDecode(
                reinterpret_cast<char *>(raw_buffer.data()),
                messages::MessageHeader::encodedLength(),
                header_encoder.blockLength(), header_encoder.version(),
                raw_buffer.size());

            bool identityAdded = messageHandler_.add_identity(sbeIdentity);

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
                          identity.name);
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
    const IdentityData &original, bool verified, uint8_t shardId) noexcept {
    try {
        // Create response exactly like eLoan's SendMessage method
        std::vector<uint8_t> raw_buffer(
            messages::MessageHeader::encodedLength() +
            messages::IdentityMessage::sbeBlockLength() + original.msg.size() +
            original.type.size() + original.id.size() + original.name.size() +
            original.dateOfIssue.size() + original.dateOfExpiry.size() +
            original.address.size() + original.verified.size());

        aeron::concurrent::AtomicBuffer atomic_buffer(raw_buffer.data(),
                                                      raw_buffer.size());

        messages::MessageHeader header_encoder;
        header_encoder
            .wrap(reinterpret_cast<char *>(raw_buffer.data()), 0, 0,
                  raw_buffer.size())
            .blockLength(messages::IdentityMessage::sbeBlockLength())
            .templateId(messages::IdentityMessage::sbeTemplateId())
            .schemaId(messages::IdentityMessage::sbeSchemaId())
            .version(messages::IdentityMessage::sbeSchemaVersion());

        messages::IdentityMessage response_encoder;
        response_encoder.wrapForEncode(
            reinterpret_cast<char *>(raw_buffer.data()),
            messages::MessageHeader::encodedLength(), raw_buffer.size());

        // Set response data - change msg to "Identity Verification Response"
        // and verified to true/false
        response_encoder.msg().putCharVal("Identity Verification Response");
        response_encoder.type().putCharVal(original.type);
        response_encoder.id().putCharVal(original.id);
        response_encoder.name().putCharVal(original.name);
        response_encoder.dateOfIssue().putCharVal(original.dateOfIssue);
        response_encoder.dateOfExpiry().putCharVal(original.dateOfExpiry);
        response_encoder.address().putCharVal(original.address);
        response_encoder.verified().putCharVal(verified ? "true" : "false");

        // Create a new IdentityMessage from the encoded buffer
        messages::IdentityMessage responseIdentity;
        responseIdentity.wrapForDecode(
            reinterpret_cast<char *>(raw_buffer.data()),
            messages::MessageHeader::encodedLength(),
            header_encoder.blockLength(), header_encoder.version(),
            raw_buffer.size());

        return responseIdentity;
    } catch (const std::exception &e) {
        Log.error_fast(shardId, "Error creating response message: {}",
                       e.what());
        // Return a default message in case of error
        // Create a minimal default response
        std::vector<uint8_t> default_buffer(
            messages::MessageHeader::encodedLength() +
            messages::IdentityMessage::sbeBlockLength());

        messages::MessageHeader default_header;
        default_header
            .wrap(reinterpret_cast<char *>(default_buffer.data()), 0, 0,
                  default_buffer.size())
            .blockLength(messages::IdentityMessage::sbeBlockLength())
            .templateId(messages::IdentityMessage::sbeTemplateId())
            .schemaId(messages::IdentityMessage::sbeSchemaId())
            .version(messages::IdentityMessage::sbeSchemaVersion());

        messages::IdentityMessage default_response;
        default_response.wrapForDecode(
            reinterpret_cast<char *>(default_buffer.data()),
            messages::MessageHeader::encodedLength(),
            default_header.blockLength(), default_header.version(),
            default_buffer.size());

        return default_response;
    }
}
