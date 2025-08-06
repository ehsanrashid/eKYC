#pragma once

#include <atomic>
#include <iosfwd>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "MessageHandler.h"
#include "Messaging.h"
#include "config.h"
#include "loggerwrapper.h"

extern const int ShardId;
extern LoggerWrapper Log;

class eKYCEngine final {
   public:
    eKYCEngine() noexcept;

    ~eKYCEngine() noexcept;

    void start() noexcept;

    void stop() noexcept;

    // Monitoring methods
    std::uint64_t getPacketsReceived() const noexcept {
        return packetsReceived_.load();
    }
    std::uint64_t getErrorCount() const noexcept { return errorCount_.load(); }
    std::uint64_t getConsecutiveErrors() const noexcept {
        return consecutiveErrors_.load();
    }

   private:
    void process_shard_messages(uint8_t shardId) noexcept;
    void process_identity_message(messages::IdentityMessage& identity,
                                  uint8_t shardId) noexcept;
    messages::IdentityMessage create_response_message(
        messages::IdentityMessage& original, bool verified,
        uint8_t shardId) noexcept;

    std::atomic<bool> running_;
    std::atomic<std::uint64_t> packetsReceived_;
    std::atomic<std::uint64_t> errorCount_;
    std::atomic<std::uint64_t> consecutiveErrors_;

    // Messaging component for Aeron handling
    std::unique_ptr<Messaging> messaging_;

    // Message handler for business logic
    MessageHandler messageHandler_;

    // Processing threads for sharded queues (one per shard)
    std::vector<std::thread> processingThreads_;
};
