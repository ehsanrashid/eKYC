#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "aeron_wrapper.h"
#include "pg_wrapper.h"
#include "logger.h"

extern Logger Log;

// Forward declaration
namespace my {
namespace app {
namespace messages {
class IdentityMessage;
}
}  // namespace app
}  // namespace my

class eKYCEngine {
   public:
    static constexpr const char* AeronDir = "";

    static constexpr const char* SubscriptionChannel =
        "aeron:udp?endpoint=0.0.0.0:50000";
    static constexpr int SubscriptionStreamId = 1001;

    static constexpr const char* PublicationChannel =
        "aeron:udp?endpoint=anas.eagri.com:10001";
    static constexpr int PublicationStreamId = 1001;

    eKYCEngine();

    ~eKYCEngine();

    void start();

    void stop();

   private:
    void process_message(const aeron_wrapper::FragmentData& fragmentData);
    void verify_and_respond(my::app::messages::IdentityMessage& identity);
    void send_response(my::app::messages::IdentityMessage& original_identity,
                       bool verification_result);
    bool verify_identity(const std::string& name, const std::string& id);
    bool user_exists(const std::string& identity_number,
                     const std::string& name);
    bool add_user_to_system(my::app::messages::IdentityMessage& identity);

    // Aeron components
    std::unique_ptr<aeron_wrapper::Aeron> aeron_;
    std::unique_ptr<aeron_wrapper::Subscription> subscription_;
    std::unique_ptr<aeron_wrapper::Publication> publication_;
    std::unique_ptr<aeron_wrapper::Subscription::BackgroundPoller>
        backgroundPoller_;
    long int receiving_packets_ = 0;
    std::atomic<bool> running_;

    std::unique_ptr<pg_wrapper::Database> db_;
};