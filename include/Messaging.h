#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <thread>

#include "aeron_wrapper.h"
#include "config.h"
#include "loggerwrapper.h"
#include "messages/IdentityMessage.h"
#include "sharded_queue.h"

extern const int ShardId;
extern LoggerWrapper Log;

class Messaging {
   public:
    Messaging();
    ~Messaging();

    /// Initialize Aeron, set up Publication & Subscription
    bool initialize();

    /// Shutdown Aeron and stop listener thread
    void shutdown();

    /// Send an IdentityMessage response via Aeron Publication
    bool sendResponse(messages::IdentityMessage& identity);

    /// Get access to the sharded queue array
    std::array<ShardedQueue, config::NUM_SHARDS>& getQueue();

   private:
    /// Listener loop that polls Aeron Subscription
    void listenerLoop();

    std::atomic_bool running_{false};
    std::thread listenerThread_;

    // Aeron components
    std::unique_ptr<aeron_wrapper::Aeron> aeron_;
    std::unique_ptr<aeron_wrapper::Subscription> subscription_;
    std::unique_ptr<aeron_wrapper::Publication> publication_;

    // Sharded queues
    std::array<ShardedQueue, config::NUM_SHARDS> sharded_queue;

    // Shard counter for round-robin distribution
    std::atomic<uint32_t> _shard_counter{0};
};