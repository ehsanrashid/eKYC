#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "include/aeron_wrapper.h"
#include "logger.h"

extern Logger Log;

class eKYCEngine {
   public:
    static constexpr const char* AeronDir = "";

    static constexpr const char* SubscriptionChannel =
        "aeron:udp?endpoint=0.0.0.0:50000";
    static constexpr int SubscriptionStreamId = 1001;

    static constexpr const char* PublicationChannel =
        "aeron:udp?endpoint=0.0.0.0:40124";
    static constexpr int PublicationStreamId = 100;

    eKYCEngine();

    ~eKYCEngine();

    void start();

    void stop();

   private:
    void process_message(const aeron_wrapper::FragmentData& fragmentData);

    // Aeron components
    std::unique_ptr<aeron_wrapper::Aeron> aeron_;
    std::unique_ptr<aeron_wrapper::Subscription> subscription_;
    std::unique_ptr<aeron_wrapper::Publication> publication_;
    std::unique_ptr<aeron_wrapper::Subscription::BackgroundPoller>
        backgroundPoller_;

    std::atomic<bool> running_;
};