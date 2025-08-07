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
    aeron::concurrent::BackoffIdleStrategy idleStrategy(100, 1000);
    bool isWritten = false;
    auto start = std::chrono::high_resolution_clock::now();

    while (!isWritten) {
        isWritten = _ring_buffer.write(1, buffer, offset, length);
        if (isWritten) {
            return;
        }
        if (std::chrono::high_resolution_clock::now() - start >=
            std::chrono::microseconds(50)) {
            std::cerr << "retry timeout" << std::endl;
            return;
        }
        idleStrategy.idle();
    }
}

std::optional<std::variant<IdentityData>> ShardedQueue::dequeue() {
    std::optional<std::variant<IdentityData>> result;
    _ring_buffer.read([&](int8_t msgType,
                          aeron::concurrent::AtomicBuffer &buffer,
                          int32_t offset, int32_t length) {
        try {
            // SBE decoding exactly like eLoan
            my::app::messages::MessageHeader msgHeader;
            my::app::messages::IdentityMessage identity;

            msgHeader.wrap(reinterpret_cast<char *>(buffer.buffer()), offset, 0,
                           length);
            offset += msgHeader.encodedLength();  // usually 8 bytes

            if (msgHeader.templateId() ==
                my::app::messages::IdentityMessage::sbeTemplateId()) {
                identity.wrapForDecode(
                    reinterpret_cast<char *>(buffer.buffer()), offset,
                    msgHeader.blockLength(), msgHeader.version(),
                    buffer.capacity());

                // Create simple struct like eLoan does
                IdentityData identityData;
                identityData.msg = identity.msg().getCharValAsString();
                identityData.type = identity.type().getCharValAsString();
                identityData.id = identity.id().getCharValAsString();
                identityData.name = identity.name().getCharValAsString();
                identityData.dateOfIssue =
                    identity.dateOfIssue().getCharValAsString();
                identityData.dateOfExpiry =
                    identity.dateOfExpiry().getCharValAsString();
                identityData.address = identity.address().getCharValAsString();
                identityData.verified =
                    identity.verified().getCharValAsString();

                result.emplace(identityData);
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