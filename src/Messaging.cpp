#include "Messaging.h"

#include <Aeron.h>
#include <Context.h>
#include <FragmentAssembler.h>
#include <Publication.h>
#include <Subscription.h>

#include <chrono>
#include <functional>
#include <iostream>
#include <vector>

#include "config.h"
#include "loggerwrapper.h"
#include "messages/IdentityMessage.h"
#include "messages/MessageHeader.h"

extern const int ShardId;
extern LoggerWrapper Log;

// Use config values instead of hardcoded ones
static constexpr std::chrono::duration<long, std::milli> SLEEP_IDLE_MS(1);

Messaging::Messaging() = default;

Messaging::~Messaging() { shutdown(); }

bool Messaging::initialize() {
    try {
        aeron::Context context;
        aeron_ = aeron::Aeron::connect(context);

        // Create subscription for incoming messages using config values
        std::string channel_in = std::string("aeron:") +
                                 config::AERON_PROTOCOL +
                                 "?endpoint=" + config::SUBSCRIPTION_IP + ":" +
                                 config::SUBSCRIPTION_PORT_STR;
        std::int64_t id =
            aeron_->addSubscription(channel_in, config::SUBSCRIPTION_STREAM_ID);
        subscription_ = aeron_->findSubscription(id);
        while (!subscription_) {
            std::this_thread::yield();
            subscription_ = aeron_->findSubscription(id);
        }
        Log.info_fast(ShardId, "Subscribed. Sub Id: {}", id);

        // Create publication for outgoing messages using config values
        std::string channel_out = std::string("aeron:") +
                                  config::AERON_PROTOCOL +
                                  "?endpoint=" + config::PUBLICATION_IP + ":" +
                                  config::PUBLICATION_PORT_STR;
        id = aeron_->addPublication(channel_out, config::PUBLICATION_STREAM_ID);
        publication_ = aeron_->findPublication(id);
        while (!publication_) {
            std::this_thread::yield();
            publication_ = aeron_->findPublication(id);
        }
        Log.info_fast(ShardId, "Published. Pub Id: {}", id);

    } catch (const std::exception& e) {
        Log.error_fast(ShardId, "Aeron initialization failed: {}", e.what());
        return false;
    }

    running_ = true;
    listenerThread_ = std::thread(&Messaging::listenerLoop, this);
    Log.info_fast(ShardId, "Aeron initialized and listener thread started");
    return true;
}

aeron::fragment_handler_t Messaging::fragHandler() {
    return [&](const aeron::AtomicBuffer& buffer, std::int32_t offset,
               std::int32_t length, const aeron::Header& header) {
        if (length < sizeof(std::uint8_t)) {
            return;  // too small to read any header
        }
        Log.info_fast(ShardId, "-----Got New Identity Message-----");

        // Simple round-robin sharding like eLoan
        uint8_t shardId = (_shard_counter.fetch_add(1)) % config::NUM_SHARDS;
        Log.info_fast(ShardId, "Round robin assigned shard: {}", shardId);

        // Enqueue to the selected shard
        aeron::concurrent::AtomicBuffer atomicBuffer(
            const_cast<uint8_t*>(buffer.buffer()), buffer.capacity());
        sharded_queue[shardId].enqueue(atomicBuffer, offset, length);
    };
}

void Messaging::listenerLoop() {
    Log.info_fast(ShardId, "Listener loop started");
    aeron::FragmentAssembler fragmentAssembler(fragHandler());
    aeron::fragment_handler_t handler = fragmentAssembler.handler();
    aeron::SleepingIdleStrategy sleepStrategy(SLEEP_IDLE_MS);

    while (running_) {
        // Poll up to 10 fragments per iteration
        std::int32_t fragmentsRead = subscription_->poll(handler, 10);
        sleepStrategy.idle(fragmentsRead);
    }

    Log.info_fast(ShardId, "Listener thread exiting");
}

bool Messaging::sendResponse(messages::IdentityMessage& identity) {
    try {
        // Calculate exact buffer size needed - use fixed block size like eLoan
        std::string msg = identity.msg().getCharValAsString();
        std::string type = identity.type().getCharValAsString();
        std::string id = identity.id().getCharValAsString();
        std::string name = identity.name().getCharValAsString();
        std::string dateOfIssue = identity.dateOfIssue().getCharValAsString();
        std::string dateOfExpiry = identity.dateOfExpiry().getCharValAsString();
        std::string address = identity.address().getCharValAsString();
        std::string verified = identity.verified().getCharValAsString();

        // Use fixed block size - variable fields are already included in the
        // 512-byte block
        std::vector<uint8_t> raw_buffer(
            messages::MessageHeader::encodedLength() +
            messages::IdentityMessage::sbeBlockLength());

        aeron::concurrent::AtomicBuffer atomic_buffer(raw_buffer.data(),
                                                      raw_buffer.size());

        messages::MessageHeader header_encoder;
        header_encoder
            .wrap(reinterpret_cast<char*>(raw_buffer.data()), 0, 0,
                  raw_buffer.size())
            .blockLength(messages::IdentityMessage::sbeBlockLength())
            .templateId(messages::IdentityMessage::sbeTemplateId())
            .schemaId(messages::IdentityMessage::sbeSchemaId())
            .version(messages::IdentityMessage::sbeSchemaVersion());

        messages::IdentityMessage response_encoder;
        response_encoder.wrapForEncode(
            reinterpret_cast<char*>(raw_buffer.data()),
            messages::MessageHeader::encodedLength(),
            raw_buffer.size() - messages::MessageHeader::encodedLength());

        // Copy all fields from the passed identity message
        response_encoder.msg().putCharVal(msg);
        response_encoder.type().putCharVal(type);
        response_encoder.id().putCharVal(id);
        response_encoder.name().putCharVal(name);
        response_encoder.dateOfIssue().putCharVal(dateOfIssue);
        response_encoder.dateOfExpiry().putCharVal(dateOfExpiry);
        response_encoder.address().putCharVal(address);
        response_encoder.verified().putCharVal(verified);

        std::int64_t result = publication_->offer(
            atomic_buffer, 0,
            header_encoder.encodedLength() + response_encoder.encodedLength());

        if (result > 0) {
            Log.info_fast(ShardId, "Response sent successfully");
            return true;
        } else {
            Log.error_fast(ShardId, "Failed to send response: {}", result);
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