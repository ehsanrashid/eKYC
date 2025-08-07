#ifndef MESSAGING_H
#define MESSAGING_H

#include <Aeron.h>
#include <Context.h>
#include <FragmentAssembler.h>
#include <Publication.h>
#include <Subscription.h>

#include <array>
#include <atomic>
#include <memory>
#include <thread>

#include "config.h"
#include "loggerwrapper.h"
#include "sharded_queue.h"

class Messaging {
   public:
    Messaging();
    ~Messaging();

    /// Initialize Aeron, set up Publication & Subscription
    bool initialize();

    /// Shutdown Aeron and stop listener thread
    void shutdown();

    /// Send response message
    bool sendResponse(my::app::messages::IdentityMessage& identity);

    /// Get access to sharded queues
    std::array<ShardedQueue, config::NUM_SHARDS>& getQueue();

   private:
    /// Listener loop that polls Aeron Subscription
    void listenerLoop();

    /// Fragment handler for incoming messages
    aeron::fragment_handler_t fragHandler();

    std::atomic_bool running_{false};
    std::thread listenerThread_;

    std::shared_ptr<aeron::Aeron> aeron_;
    std::shared_ptr<aeron::Subscription> subscription_;
    std::shared_ptr<aeron::Publication> publication_;

    std::array<ShardedQueue, config::NUM_SHARDS> sharded_queue;

    // Round-robin counter for sharding
    std::atomic<uint32_t> _shard_counter{0};
};

#endif  // MESSAGING_H