#include "sharded_queue.h"

#include <concurrent/BackOffIdleStrategy.h>

#include <iostream>

#include "messages/MessageHeader.h"

ShardedQueue::ShardedQueue()
    : _buffer(buffer, config::MAX_RING_BUFFER_SIZE +
                          aeron::concurrent::ringbuffer::RingBufferDescriptor::
                              TRAILER_LENGTH),
      _ring_buffer(_buffer) {}

ShardedQueue::~ShardedQueue() = default;

void ShardedQueue::enqueue(aeron::concurrent::AtomicBuffer buffer,
                           int32_t offset, int32_t length) {
    aeron::concurrent::BackoffIdleStrategy idleStrategy(
        config::IDLE_STRATEGY_SPINS, config::IDLE_STRATEGY_YIELDS);
    bool isWritten = false;
    auto start = std::chrono::high_resolution_clock::now();
    while (!isWritten) {
        isWritten = _ring_buffer.write(1, buffer, offset, length);
        if (isWritten) {
            return;
        }
        if (std::chrono::high_resolution_clock::now() - start >=
            std::chrono::microseconds(config::SHARD_TIMEOUT_MS * 1000)) {
            std::cerr << "retry timeout" << std::endl;
            return;
        }
        idleStrategy.idle();
    }
}

std::optional<messages::IdentityMessage> ShardedQueue::dequeue() {
    std::optional<messages::IdentityMessage> result;
    _ring_buffer.read([&](int8_t msgType,
                          aeron::concurrent::AtomicBuffer &buffer,
                          int32_t offset, int32_t length) {
        try {
            // The buffer contains raw message data, we need to decode it
            // properly
            messages::MessageHeader msgHeader;
            messages::IdentityMessage identity;

            // Wrap the header at the beginning of the buffer
            msgHeader.wrap(reinterpret_cast<char *>(buffer.buffer()), offset, 0,
                           offset + length);

            // Check if this is an IdentityMessage
            if (msgHeader.templateId() ==
                messages::IdentityMessage::sbeTemplateId()) {
                // Calculate the offset for the message body
                int32_t messageOffset = offset + msgHeader.encodedLength();

                // Decode the identity message
                identity.wrapForDecode(
                    reinterpret_cast<char *>(buffer.buffer()), messageOffset,
                    msgHeader.blockLength(), msgHeader.version(),
                    offset + length);

                std::cout << "Successfully decoded message: "
                          << identity.msg().getCharValAsString()
                          << ", ID: " << identity.id().getCharValAsString()
                          << std::endl;

                result.emplace(identity);
            } else {
                std::cerr << "Unexpected template ID: "
                          << msgHeader.templateId() << std::endl;
            }
        } catch (const std::exception &e) {
            std::cerr << "Error decoding message: " << e.what() << std::endl;
        }
    });
    return result;
}

int ShardedQueue::size() { return _ring_buffer.size(); }