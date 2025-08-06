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
        messages::MessageHeader msgHeader;
        messages::IdentityMessage identity;
        msgHeader.wrap(reinterpret_cast<char *>(buffer.buffer()), offset, 0,
                       buffer.capacity());
        offset += msgHeader.encodedLength();
        if (msgHeader.templateId() ==
            messages::IdentityMessage::sbeTemplateId()) {
            identity.wrapForDecode(reinterpret_cast<char *>(buffer.buffer()),
                                   offset, msgHeader.blockLength(),
                                   msgHeader.version(), buffer.capacity());
            result.emplace(identity);
        } else {
            std::cerr << "Unexpected msgType" << std::endl;
        }
    });
    return result;
}

int ShardedQueue::size() { return _ring_buffer.size(); }