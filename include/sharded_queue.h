#ifndef SHARDED_QUEUE_H
#define SHARDED_QUEUE_H

#include <concurrent/ringbuffer/OneToOneRingBuffer.h>

#include <array>
#include <optional>

#include "config.h"
#include "messages/IdentityMessage.h"

class ShardedQueue {
   public:
    ShardedQueue();
    ~ShardedQueue();
    void enqueue(aeron::concurrent::AtomicBuffer, int32_t, int32_t);
    std::optional<messages::IdentityMessage> dequeue();
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