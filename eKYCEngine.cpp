#include "eKYCEngine.h"

#include <chrono>
#include <exception>
#include <iostream>


eKYCEngine::eKYCEngine()
    : running_(false)
    {
    try {
        aeron_ = std::make_unique<aeron_wrapper::Aeron>(AeronDir);
        Log.info("Connected to Aeron Media Driver...");
        subscription_ = aeron_->create_subscription(SubscriptionChannel,  //
                                                    SubscriptionStreamId);
        publication_ = aeron_->create_publication(PublicationChannel,  //
                                                  PublicationStreamId);
        running_ = true;
    } catch (const std::exception& e) {
        Log.info(std::string("Error: ") + e.what());
    }
}

eKYCEngine::~eKYCEngine() { stop(); }

void eKYCEngine::start() {
    if (!running_) return;
    Log.info("Starting eKYC engine...");
    // Start background msg processing
    backgroundPoller_ = subscription_->start_background_polling(
        [this](const aeron_wrapper::FragmentData& fragmentData) {
            process_message(fragmentData);
        });
}

void eKYCEngine::stop() {
    if (!running_) return;
    if (backgroundPoller_) {
        backgroundPoller_->stop();
    }

    running_ = false;
    Log.info("eKYC engine stopped.");
}

void eKYCEngine::process_message(
    const aeron_wrapper::FragmentData& fragmentData) {
    try {
        Log.debug("msg: " + fragmentData.as_string());
    } catch (const std::exception& e) {
        Log.error(std::string("Error processing message: ") + e.what());
    }
}
