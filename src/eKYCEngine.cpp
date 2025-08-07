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
                my::app::messages::IdentityMessage responseIdentity =
                    create_response_message(identity, true, shardId);
                messaging_->sendResponse(responseIdentity);
            } else {
                Log.info_fast(shardId, "Verification failed for {} {}", name,
                              id);
                // Create response with verified=false
                my::app::messages::IdentityMessage responseIdentity =
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

            // Create a proper SBE message for the database operation
            std::vector<uint8_t> raw_buffer(
                my::app::messages::MessageHeader::encodedLength() +
                my::app::messages::IdentityMessage::sbeBlockLength() +
                identity.msg.size() + identity.type.size() +
                identity.id.size() + identity.name.size() +
                identity.dateOfIssue.size() + identity.dateOfExpiry.size() +
                identity.address.size() + identity.verified.size() +
                sizeof(std::int64_t));  // Add padding

            my::app::messages::MessageHeader header_encoder;
            header_encoder
                .wrap(reinterpret_cast<char *>(raw_buffer.data()), 0, 0,
                      raw_buffer.size())
                .blockLength(
                    my::app::messages::IdentityMessage::sbeBlockLength())
                .templateId(my::app::messages::IdentityMessage::sbeTemplateId())
                .schemaId(my::app::messages::IdentityMessage::sbeSchemaId())
                .version(
                    my::app::messages::IdentityMessage::sbeSchemaVersion());

            my::app::messages::IdentityMessage identity_encoder;
            identity_encoder.wrapForEncode(
                reinterpret_cast<char *>(raw_buffer.data()),
                my::app::messages::MessageHeader::encodedLength(),
                raw_buffer.size());

            // Set the identity data
            identity_encoder.msg().putCharVal(identity.msg);
            identity_encoder.type().putCharVal(identity.type);
            identity_encoder.id().putCharVal(identity.id);
            identity_encoder.name().putCharVal(identity.name);
            identity_encoder.dateOfIssue().putCharVal(identity.dateOfIssue);
            identity_encoder.dateOfExpiry().putCharVal(identity.dateOfExpiry);
            identity_encoder.address().putCharVal(identity.address);
            identity_encoder.verified().putCharVal(identity.verified);

            // Pass the encoded message directly to the handler
            bool identityAdded = messageHandler_.add_identity(identity_encoder);

            if (identityAdded) {
                Log.info_fast(shardId, "User addition successful for {} {}",
                              name, id);
                // Send back response with verified=true (user added
                // successfully)
                my::app::messages::IdentityMessage responseIdentity =
                    create_response_message(identity, true, shardId);
                messaging_->sendResponse(responseIdentity);
            } else {
                Log.info_fast(shardId, "User addition failed for {} {}", name,
                              id);
                // Send back response with verified=false (user addition failed)
                my::app::messages::IdentityMessage responseIdentity =
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

my::app::messages::IdentityMessage eKYCEngine::create_response_message(
    const IdentityData &original, bool verified, uint8_t shardId) noexcept {
    try {
        // Calculate exact buffer size needed
        std::string responseMsg = "Identity Verification Response";
        std::string responseVerified = verified ? "true" : "false";

        // Calculate proper size including all SBE string headers
        size_t totalSize =
            my::app::messages::MessageHeader::encodedLength() +
            my::app::messages::IdentityMessage::sbeBlockLength() +
            (responseMsg.size() + 4) +  // 4 bytes for string length header
            (original.type.size() + 4) + (original.id.size() + 4) +
            (original.name.size() + 4) + (original.dateOfIssue.size() + 4) +
            (original.dateOfExpiry.size() + 4) + (original.address.size() + 4) +
            (responseVerified.size() + 4) + 64;  // Extra padding for alignment

        // Use a static thread_local buffer to ensure lifetime
        static thread_local std::vector<uint8_t> response_buffer;
        response_buffer.resize(totalSize);
        response_buffer.assign(totalSize, 0);  // Zero initialize

        my::app::messages::MessageHeader header_encoder;
        header_encoder
            .wrap(reinterpret_cast<char *>(response_buffer.data()), 0, 0,
                  response_buffer.size())
            .blockLength(my::app::messages::IdentityMessage::sbeBlockLength())
            .templateId(my::app::messages::IdentityMessage::sbeTemplateId())
            .schemaId(my::app::messages::IdentityMessage::sbeSchemaId())
            .version(my::app::messages::IdentityMessage::sbeSchemaVersion());

        my::app::messages::IdentityMessage response_encoder;
        response_encoder.wrapForEncode(
            reinterpret_cast<char *>(response_buffer.data()),
            my::app::messages::MessageHeader::encodedLength(),
            response_buffer.size());

        // Set all fields carefully
        response_encoder.msg().putCharVal(responseMsg);
        response_encoder.type().putCharVal(original.type);
        response_encoder.id().putCharVal(original.id);
        response_encoder.name().putCharVal(original.name);
        response_encoder.dateOfIssue().putCharVal(original.dateOfIssue);
        response_encoder.dateOfExpiry().putCharVal(original.dateOfExpiry);
        response_encoder.address().putCharVal(original.address);
        response_encoder.verified().putCharVal(responseVerified);

        // Create a decoder to return a readable message
        my::app::messages::IdentityMessage response_decoder;
        response_decoder.wrapForDecode(
            reinterpret_cast<char *>(response_buffer.data()),
            my::app::messages::MessageHeader::encodedLength(),
            header_encoder.blockLength(), header_encoder.version(),
            response_buffer.size());

        return response_decoder;
    } catch (const std::exception &e) {
        Log.error_fast(shardId, "Error creating response message: {}",
                       e.what());
        // Return a minimal default message
        static thread_local std::vector<uint8_t> default_buffer(512, 0);

        my::app::messages::MessageHeader default_header;
        default_header
            .wrap(reinterpret_cast<char *>(default_buffer.data()), 0, 0,
                  default_buffer.size())
            .blockLength(my::app::messages::IdentityMessage::sbeBlockLength())
            .templateId(my::app::messages::IdentityMessage::sbeTemplateId())
            .schemaId(my::app::messages::IdentityMessage::sbeSchemaId())
            .version(my::app::messages::IdentityMessage::sbeSchemaVersion());

        my::app::messages::IdentityMessage default_response;
        default_response.wrapForDecode(
            reinterpret_cast<char *>(default_buffer.data()),
            my::app::messages::MessageHeader::encodedLength(),
            default_header.blockLength(), default_header.version(),
            default_buffer.size());

        return default_response;
    }
}
