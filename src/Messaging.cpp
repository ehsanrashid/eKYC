#include "messaging.h"

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

                    // Round-robin shard selection
                    uint8_t shardId = (++_shard_counter) % config::NUM_SHARDS;
                    Log.info_fast(ShardId, "Enqueued to shard: {}", shardId);

                    // Enqueue to the selected shard
                    sharded_queue[shardId].enqueue(fragmentData.buffer, 0,
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

bool Messaging::sendResponse(const messages::IdentityMessage& identity) {
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

        // Copy the identity data
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