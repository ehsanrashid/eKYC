#include "Messaging.h"

#include <chrono>
#include <iostream>

#include "messages/MessageHeader.h"

Messaging::Messaging() {}

Messaging::~Messaging() { shutdown(); }

bool Messaging::initialize() {
    try {
        // Initialize Aeron
        aeron_ = std::make_unique<aeron_wrapper::Aeron>("");
        Log.info_fast(ShardId, "Connected to Aeron Media Driver...");

        // Create subscription
        subscription_ = aeron_->create_subscription(
            "aeron:" + std::string(config::AERON_PROTOCOL) + "?endpoint=" +
                config::SUBSCRIPTION_IP + ":" + config::SUBSCRIPTION_PORT_STR,
            config::SUBSCRIPTION_STREAM_ID);

        // Create publication
        publication_ = aeron_->create_publication(
            "aeron:" + std::string(config::AERON_PROTOCOL) + "?endpoint=" +
                config::PUBLICATION_IP + ":" + config::PUBLICATION_PORT_STR,
            config::PUBLICATION_STREAM_ID);

        Log.info_fast(ShardId, "Aeron initialized successfully");

        running_ = true;
        listenerThread_ = std::thread(&Messaging::listenerLoop, this);
        Log.info_fast(ShardId, "Listener thread started");

        return true;
    } catch (const std::exception& e) {
        Log.error_fast(ShardId, "Aeron initialization failed: {}", e.what());
        return false;
    }
}

void Messaging::listenerLoop() {
    Log.info_fast(ShardId, "Listener loop started");

    while (running_) {
        try {
            // Poll for fragments
            subscription_->poll(
                [this](const aeron_wrapper::FragmentData& fragmentData) {
                    Log.info_fast(ShardId,
                                  "-----Got New Identity Message-----");

                    // Smart shard selection for SBE-encoded messages
                    uint8_t shardId;

                    try {
                        // Decode the SBE message to extract the ID for sharding
                        messages::MessageHeader msgHeader;
                        msgHeader.wrap(
                            reinterpret_cast<char*>(
                                const_cast<uint8_t*>(fragmentData.buffer)),
                            0, 0, fragmentData.length);

                        Log.info_fast(
                            ShardId, "Fragment length: {}, Template ID: {}",
                            fragmentData.length, msgHeader.templateId());

                        if (msgHeader.templateId() ==
                            messages::IdentityMessage::sbeTemplateId()) {
                            // Calculate offset for message body
                            size_t offset = msgHeader.encodedLength();

                            // Decode the identity message to get the ID
                            messages::IdentityMessage identity;
                            identity.wrapForDecode(
                                reinterpret_cast<char*>(
                                    const_cast<uint8_t*>(fragmentData.buffer)),
                                offset, msgHeader.blockLength(),
                                msgHeader.version(), fragmentData.length);

                            // Extract ID for sharding
                            std::string idValue =
                                identity.id().getCharValAsString();

                            // Add message counter to ensure distribution even
                            // with identical IDs
                            static std::atomic<uint32_t> message_counter{0};
                            uint32_t counter = message_counter.fetch_add(1);

                            // Combine ID with counter for better distribution
                            std::string combined_key =
                                idValue + "_" + std::to_string(counter);
                            std::hash<std::string> hasher;
                            shardId = hasher(combined_key) % config::NUM_SHARDS;

                            Log.info_fast(ShardId,
                                          "Decoded SBE message - ID: {}, "
                                          "Counter: {}, Shard: {}",
                                          idValue, counter, shardId);
                        } else {
                            // Fallback to round-robin for non-identity messages
                            shardId = (_shard_counter.fetch_add(1) + 1) %
                                      config::NUM_SHARDS;
                        }
                    } catch (const std::exception& e) {
                        // Fallback to round-robin if decoding fails
                        shardId = (_shard_counter.fetch_add(1) + 1) %
                                  config::NUM_SHARDS;
                        Log.error_fast(
                            ShardId,
                            "Error decoding SBE message for sharding: {}",
                            e.what());
                        Log.info_fast(ShardId, "Using fallback shard: {}",
                                      shardId);
                    }

                    Log.info_fast(ShardId, "Enqueued to shard: {}", shardId);

                    // Enqueue to the selected shard
                    aeron::concurrent::AtomicBuffer atomicBuffer(
                        const_cast<uint8_t*>(fragmentData.buffer),
                        fragmentData.length);
                    sharded_queue[shardId].enqueue(atomicBuffer, 0,
                                                   fragmentData.length);
                },
                10);  // Poll up to 10 fragments

            // If no fragments, yield CPU
            if (running_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        } catch (const std::exception& e) {
            Log.error_fast(ShardId, "Error in listener loop: {}", e.what());
        }
    }

    Log.info_fast(ShardId, "Listener thread exiting");
}

bool Messaging::sendResponse(messages::IdentityMessage& identity) {
    try {
        // Create buffer for the response
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

        // Encode identity message
        messages::IdentityMessage responseIdentity;
        responseIdentity.wrapForEncode(buffer.data(), offset, bufferCapacity);

        // Copy the identity data (this method now expects a properly formatted
        // response message)
        responseIdentity.msg().putCharVal(identity.msg().getCharValAsString());
        responseIdentity.type().putCharVal(
            identity.type().getCharValAsString());
        responseIdentity.id().putCharVal(identity.id().getCharValAsString());
        responseIdentity.name().putCharVal(
            identity.name().getCharValAsString());
        responseIdentity.dateOfIssue().putCharVal(
            identity.dateOfIssue().getCharValAsString());
        responseIdentity.dateOfExpiry().putCharVal(
            identity.dateOfExpiry().getCharValAsString());
        responseIdentity.address().putCharVal(
            identity.address().getCharValAsString());
        responseIdentity.verified().putCharVal(
            identity.verified().getCharValAsString());

        // Send via Aeron
        auto result = publication_->offer(
            reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size());

        if (result == aeron_wrapper::PublicationResult::SUCCESS) {
            Log.info_fast(ShardId, "Response sent successfully");
            return true;
        } else {
            Log.error_fast(ShardId, "Failed to send response: {}",
                           static_cast<int>(result));
            return false;
        }
    } catch (const std::exception& e) {
        Log.error_fast(ShardId, "Error sending response: {}", e.what());
        return false;
    }
}

std::array<ShardedQueue, config::NUM_SHARDS>& Messaging::getQueue() {
    return sharded_queue;
}

void Messaging::shutdown() {
    if (!running_) return;

    running_ = false;

    if (listenerThread_.joinable()) {
        listenerThread_.join();
    }

    publication_.reset();
    subscription_.reset();
    aeron_.reset();

    Log.info_fast(ShardId, "Messaging shutdown complete");
}