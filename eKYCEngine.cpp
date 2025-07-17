#include "eKYCEngine.h"

#include <chrono>
#include <exception>
#include <iostream>
#include "logger.h"
#include "loggerwrapper.h"

eKYCEngine::eKYCEngine()
    : running_(false),
      logger_("Gateway_JSON.log", 10 * 1024 * 1024)
{
    logger_.set_log_level(LogLevel::DEBUG);
    try {
        aeron_ = std::make_unique<aeron_wrapper::Aeron>(AeronDir);
        logger_.info("Connected to Aeron Media Driver...");
        subscription_ = aeron_->create_subscription(SubscriptionChannel, SubscriptionStreamId);
        publication_ = aeron_->create_publication(PublicationChannel, PublicationStreamId);
        running_ = true;
    } catch (const std::exception& e) {
        logger_.info(std::string("Error: ") + e.what());
    }
}

eKYCEngine::~eKYCEngine() { stop(); }

void eKYCEngine::start() {
    if (!running_) return;
    logger_.info("Starting eKYC engine...");
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
    logger_.info("eKYC engine stopped.");
}

void eKYCEngine::process_message(const aeron_wrapper::FragmentData& fragmentData) {
    try {
        logger_.debug("msg: " + fragmentData.as_string());
    } catch (const std::exception& e) {
        logger_.error(std::string("Error processing message: ") + e.what());
    }
}
