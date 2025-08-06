#ifndef SHARDED_QUEUE_H
#define SHARDED_QUEUE_H

#include <concurrent/ringbuffer/OneToOneRingBuffer.h>

#include <optional>
#include <string>
#include <variant>

#include "config.h"
#include "messages/IdentityMessage.h"

// Simple struct like eLoan's Order struct
struct IdentityData {
    std::string msg;
    std::string type;
    std::string id;
    std::string name;
    std::string dateOfIssue;
    std::string dateOfExpiry;
    std::string address;
    std::string verified;
};

class ShardedQueue {
   public:
    ShardedQueue();
    ~ShardedQueue();
    void enqueue(aeron::concurrent::AtomicBuffer, int32_t, int32_t);
    std::optional<std::variant<IdentityData>> dequeue();
    int size();

   private:
    std::array<
        uint8_t,
        config::MAX_RING_BUFFER_SIZE +
            aeron::concurrent::ringbuffer::RingBufferDescriptor::TRAILER_LENGTH>
        buffer;
    aeron::concurrent::AtomicBuffer _buffer;
    aeron::concurrent::ringbuffer::OneToOneRingBuffer _ring_buffer;
};

#endif  // SHARDED_QUEUE_H