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

void ShardedQueue::enqueue(const aeron::concurrent::AtomicBuffer &buffer,
                           int offset, int length) {
    // Create a non-const copy to satisfy the ring buffer API
    aeron::concurrent::AtomicBuffer non_const_buffer(
        const_cast<uint8_t *>(static_cast<const uint8_t *>(buffer.buffer())),
        buffer.capacity());

    // Write directly using the ring buffer's write method
    _ring_buffer.write(1, non_const_buffer, offset, length);
}

std::optional<std::variant<IdentityData>> ShardedQueue::dequeue() {
    std::optional<std::variant<IdentityData>> result;
    _ring_buffer.read([&](int8_t msgType,
                          aeron::concurrent::AtomicBuffer &buffer,
                          int32_t offset, int32_t length) {
        try {
            my::app::messages::MessageHeader msgHeader;
            my::app::messages::IdentityMessage identity;

            msgHeader.wrap(reinterpret_cast<char *>(buffer.buffer()), offset, 0,
                           buffer.capacity());

            offset += msgHeader.encodedLength();

            if (msgHeader.templateId() ==
                my::app::messages::IdentityMessage::sbeTemplateId()) {
                identity.wrapForDecode(
                    reinterpret_cast<char *>(buffer.buffer()), offset,
                    msgHeader.blockLength(), msgHeader.version(),
                    buffer.capacity());

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