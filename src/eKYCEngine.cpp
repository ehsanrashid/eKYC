#include "eKYCEngine.h"

#include <chrono>
#include <exception>
#include <iostream>
#include <vector>

#include "helper.h"
#include "messages/Char64str.h"
#include "messages/IdentityMessage.h"
#include "messages/MessageHeader.h"

eKYCEngine::eKYCEngine() : running_(false), messageHandler_() {
    try {
        aeron_ = std::make_unique<aeron_wrapper::Aeron>(AeronDir);
        Log.info_fast(ShardId, "Connected to Aeron Media Driver...");
        subscription_ = aeron_->create_subscription(SubscriptionChannel,  //
                                                    SubscriptionStreamId);
        publication_ = aeron_->create_publication(PublicationChannel,  //
                                                  PublicationStreamId);

        running_ = true;
    } catch (const std::exception &e) {
        Log.info_fast(ShardId, "Error: {}", e.what());
    }
}

eKYCEngine::~eKYCEngine() { stop(); }

void eKYCEngine::start() {
    if (!running_) return;

    Log.info_fast(ShardId, "Starting eKYC engine...");
    // Start background msg processing
    backgroundPoller_ = subscription_->start_background_polling(
        [this](const aeron_wrapper::FragmentData &fragmentData) {
            process_message(fragmentData);
        });
}

void eKYCEngine::stop() {
    if (!running_) return;

    if (backgroundPoller_) {
        backgroundPoller_->stop();
    }

    running_ = false;
    Log.info_fast(ShardId, "eKYC engine stopped.");
}

void eKYCEngine::process_message(
    const aeron_wrapper::FragmentData &fragmentData) {
    ++packetsReceived_;
    try {
        auto buffer = messageHandler_.handle_message(fragmentData);
        send_response(buffer);
    } catch (const std::exception &e) {
        Log.error_fast(ShardId, "Error: {}", e.what());
    }
}

void eKYCEngine::send_response(std::vector<char> &buffer) {
    if (!publication_) return;

    auto result = publication_->offer(
        reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    if (result == aeron_wrapper::PublicationResult::SUCCESS) {
        Log.info_fast(ShardId, "Response sent successfully");
    } else {
        Log.error_fast(ShardId, "Failed to send response: {}",
                       static_cast<int>(result));
    }
}
