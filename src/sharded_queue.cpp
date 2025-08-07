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
        // 🔧 ADD VALIDATION
        if (length < 8) {  // Minimum header size
            std::cerr << "Message too short: " << length << " bytes"
                      << std::endl;
            return;
        }

        try {
            my::app::messages::MessageHeader msgHeader;
            my::app::messages::IdentityMessage identity;

            // 🔧 FIX: Use consistent buffer size
            msgHeader.wrap(reinterpret_cast<char *>(buffer.buffer()), offset, 0,
                           buffer.capacity());

            //  VALIDATE MESSAGE LENGTH
            if (length < msgHeader.encodedLength() + msgHeader.blockLength()) {
                std::cerr << "Message incomplete: got " << length << ", need "
                          << (msgHeader.encodedLength() +
                              msgHeader.blockLength())
                          << std::endl;
                return;
            }

            offset += msgHeader.encodedLength();

            if (msgHeader.templateId() ==
                my::app::messages::IdentityMessage::sbeTemplateId()) {
                // 🔧 FIX: Use consistent buffer size
                identity.wrapForDecode(
                    reinterpret_cast<char *>(buffer.buffer()), offset,
                    msgHeader.blockLength(), msgHeader.version(),
                    buffer.capacity());  // Use capacity for both

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