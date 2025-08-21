#include "eKYCEngine.h"

#include <exception>
#include <iostream>

#include "Config.h"
#include "loggerlib.h"

eKYCEngine::eKYCEngine() noexcept
    : _running(false), _packetsReceived(0), _messageHandler() {
    try {
        auto &cfg = Config::get();
        _aeron = std::make_unique<aeron_wrapper::Aeron>(cfg.AERON_DIR);
        Sharded_Logger::getInstance().info_fast(
            ShardId, "Connected to Aeron Media Driver...");
        std::string subscriptionChannel =
            "aeron:" + cfg.AERON_PROTOCOL + "?endpoint=" + cfg.SUBSCRIPTION_IP +
            ":" + std::to_string(cfg.SUBSCRIPTION_PORT);
        _subscription = _aeron->create_subscription(subscriptionChannel,  //
                                                    cfg.SUBSCRIPTION_STREAM_ID);
        std::string publicationChannel =
            "aeron:" + cfg.AERON_PROTOCOL + "?endpoint=" + cfg.PUBLICATION_IP +
            ":" + std::to_string(cfg.PUBLICATION_PORT);
        _publication = _aeron->create_publication(publicationChannel,  //
                                                  cfg.PUBLICATION_STREAM_ID);

        _running = true;
    } catch (const std::exception &e) {
        Sharded_Logger::getInstance().info_fast(ShardId, "Error: {}", e.what());
    }
}

eKYCEngine::~eKYCEngine() noexcept { stop(); }

void eKYCEngine::start() noexcept {
    if (!_running) return;

    Sharded_Logger::getInstance().info_fast(ShardId, "Starting eKYC engine...");
    // Start background msg processing
    _backgroundPoller = _subscription->start_background_polling(
        [this](const aeron_wrapper::FragmentData &fragmentData) {
            process_message(fragmentData);
        });
}

void eKYCEngine::stop() noexcept {
    if (!_running) return;

    if (_backgroundPoller) {
        _backgroundPoller->stop();
    }

    _running = false;
    Sharded_Logger::getInstance().info_fast(ShardId, "eKYC engine stopped.");
}

void eKYCEngine::process_message(
    const aeron_wrapper::FragmentData &fragmentData) noexcept {
    ++_packetsReceived;
    try {
        auto buffer = _messageHandler.respond(fragmentData);
        if (!buffer.empty()) {
            send_response(buffer);
        }
    } catch (const std::exception &e) {
        Sharded_Logger::getInstance().error_fast(ShardId, "Error: {}",
                                                 e.what());
    }
}

void eKYCEngine::send_response(std::vector<char> &buffer) noexcept {
    if (!_publication) return;

    auto result = _publication->offer(
        reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    if (result == aeron_wrapper::PublicationResult::SUCCESS) {
        Sharded_Logger::getInstance().info_fast(ShardId,
                                                "Response sent successfully");
    } else {
        Sharded_Logger::getInstance().error_fast(ShardId,
                                                 "Failed to send response: {}",
                                                 pubresult_to_string(result));
    }
}
