#pragma once

#include <atomic>
#include <iosfwd>
#include <memory>
#include <string>

#include "MessageHandler.h"
#include "aeron_wrapper.h"
#include "loggerwrapper.h"
#include "pg_wrapper.h"

extern const int ShardId;
extern LoggerWrapper Log;
// Forward declaration
namespace messages {
class IdentityMessage;
}

class eKYCEngine final {
   public:
    static constexpr const char *AeronDir = "";

    static constexpr const char *SubscriptionChannel =
        "aeron:udp?endpoint=0.0.0.0:50000";
    static constexpr int SubscriptionStreamId = 1001;

    static constexpr const char *PublicationChannel =
        "aeron:udp?endpoint=anas.eagri.com:10001";
    static constexpr int PublicationStreamId = 1001;

    eKYCEngine();

    ~eKYCEngine();

    void start();

    void stop();

   private:
    void process_message(const aeron_wrapper::FragmentData &fragmentData);
    void send_response(std::vector<char> &buffer);

    // Aeron components
    std::unique_ptr<aeron_wrapper::Aeron> aeron_;
    std::unique_ptr<aeron_wrapper::Subscription> subscription_;
    std::unique_ptr<aeron_wrapper::Publication> publication_;
    std::unique_ptr<aeron_wrapper::Subscription::BackgroundPoller>
        backgroundPoller_;

    std::atomic<bool> running_;
    long int packetsReceived_ = 0;

    MessageHandler messageHandler_;
};