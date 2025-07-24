#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "aeron_wrapper.h"
#include "logger.h"
#include "pg_wrapper.h"

extern Logger Log;

// Forward declaration
namespace messages {
class IdentityMessage;
}

class eKYCEngine {
   public:
    static constexpr const char* AeronDir = "";

    static constexpr const char* SubscriptionChannel = "aeron:ipc";
    static constexpr int SubscriptionStreamId = 1001;

    static constexpr const char* PublicationChannel = "aeron:ipc";
    static constexpr int PublicationStreamId = 2001;

    eKYCEngine();

    ~eKYCEngine();

    void start();

    void stop();

   private:
    void process_message(const aeron_wrapper::FragmentData& fragmentData);
    void verify_and_respond(messages::IdentityMessage& identity);
    void send_response(messages::IdentityMessage& originalIdentity,
                       bool verificationResult);
    bool verify_identity(const std::string& name, const std::string& id);
    bool user_exists(const std::string& identityNumber,
                     const std::string& name);
    bool add_identity(messages::IdentityMessage& identity);

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