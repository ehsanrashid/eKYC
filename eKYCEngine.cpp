#include "eKYCEngine.h"

#include <chrono>
#include <exception>
#include <iostream>

eKYCEngine::eKYCEngine() : running_(false) {
    try {
        aeron_ = std::make_unique<aeron_wrapper::Aeron>(AeronDir);
        std::cout << "Connected to Aeron Media Driver...\n";

        subscription_ = aeron_->create_subscription(SubscriptionChannel,  //
                                                    SubscriptionStreamId);
        // publication_ = aeron_->create_publication(PublicationChannel,  //
        //                                           PublicationStreamId);

        running_ = true;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

eKYCEngine::~eKYCEngine() { stop(); }

void eKYCEngine::start() {
    if (!running_) return;

    std::cout << "Starting eKYC engine...\n";

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
    std::cout << "eKYC engine stopped.\n";
}

void eKYCEngine::process_message(
    const aeron_wrapper::FragmentData& fragmentData) {
    try {
        std::cout << "msg: " << fragmentData.as_string() << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error processing message: " << e.what() << std::endl;
    }
}
