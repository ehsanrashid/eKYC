#include "eKYCEngine.h"

#include <exception>
#include <iostream>

eKYCEngine::eKYCEngine() noexcept
    : _running(false), _packetsReceived(0), _messageHandler() {
    try {
        _aeron = std::make_unique<aeron_wrapper::Aeron>(AeronDir);
        Log.info_fast(ShardId, "Connected to Aeron Media Driver...");
        _subscription = _aeron->create_subscription(SubscriptionChannel,  //
                                                    SubscriptionStreamId);
        _publication = _aeron->create_publication(PublicationChannel,  //
                                                  PublicationStreamId);

        _running = true;
    } catch (const std::exception &e) {
        Log.info_fast(ShardId, "Error: {}", e.what());
    }
}

eKYCEngine::~eKYCEngine() noexcept { stop(); }

void eKYCEngine::start() noexcept {
    if (!_running) return;

    Log.info_fast(ShardId, "Starting eKYC engine...");
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
    Log.info_fast(ShardId, "eKYC engine stopped.");
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
        Log.error_fast(ShardId, "Error: {}", e.what());
    }
}

void eKYCEngine::send_response(std::vector<char> &buffer) noexcept {
    if (!_publication) return;

    auto result = _publication->offer(
        reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    if (result == aeron_wrapper::PublicationResult::SUCCESS) {
        Log.info_fast(ShardId, "Response sent successfully");
    } else {
        Log.error_fast(ShardId, "Failed to send response: {}",
                       static_cast<int>(result));
    }
}
