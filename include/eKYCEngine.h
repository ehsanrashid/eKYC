#pragma once

#include <atomic>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

#include "MessageHandler.h"
#include "aeron_wrapper.h"

extern const int ShardId;

class eKYCEngine final {
   public:
    eKYCEngine() noexcept;

    ~eKYCEngine() noexcept;

    void start() noexcept;

    void stop() noexcept;

   private:
    void process_message(
        const aeron_wrapper::FragmentData &fragmentData) noexcept;
    void send_response(std::vector<char> &buffer) noexcept;

    // Aeron components
    std::unique_ptr<aeron_wrapper::Aeron> _aeron;
    std::unique_ptr<aeron_wrapper::Subscription> _subscription;
    std::unique_ptr<aeron_wrapper::Publication> _publication;
    std::unique_ptr<aeron_wrapper::Subscription::BackgroundPoller>
        _backgroundPoller;

    // SPSC ring consumer
    std::unique_ptr<aeron_wrapper::RingBuffer> _ring;
    std::unique_ptr<std::thread> _consumerThread;
    void consumer_loop() noexcept;

    std::atomic<bool> _running;
    std::uint64_t _packetsReceived;

    MessageHandler _messageHandler;
};
