#pragma once

#include <atomic>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

#include "MessageHandler.h"
#include "aeron_wrapper.h"
#include "config.h"
#include "loggerwrapper.h"

extern const int ShardId;
extern LoggerWrapper Log;

class eKYCEngine final {
   public:
    static constexpr const char *AeronDir = "";

    static std::string getSubscriptionChannel() {
        return std::string("aeron:") + config::AERON_PROTOCOL +
               "?endpoint=" + config::SUBSCRIPTION_IP + ":" +
               config::SUBSCRIPTION_PORT_STR;
    }

    static std::string getPublicationChannel() {
        return std::string("aeron:") + config::AERON_PROTOCOL +
               "?endpoint=" + config::PUBLICATION_IP + ":" +
               config::PUBLICATION_PORT_STR;
    }

    // Stream IDs from config
    static constexpr int SubscriptionStreamId = config::SUBSCRIPTION_STREAM_ID;
    static constexpr int PublicationStreamId = config::PUBLICATION_STREAM_ID;

    eKYCEngine() noexcept;

    ~eKYCEngine() noexcept;

    void start() noexcept;

    void stop() noexcept;

   private:
    void process_message(
        const aeron_wrapper::FragmentData &fragmentData) noexcept;
    void send_response(std::vector<char> &buffer) noexcept;

    // Aeron components
    std::unique_ptr<aeron_wrapper::Aeron> aeron_;
    std::unique_ptr<aeron_wrapper::Subscription> subscription_;
    std::unique_ptr<aeron_wrapper::Publication> publication_;
    std::unique_ptr<aeron_wrapper::Subscription::BackgroundPoller>
        backgroundPoller_;

    std::atomic<bool> running_;
    std::uint64_t packetsReceived_;

    MessageHandler messageHandler_;
};
