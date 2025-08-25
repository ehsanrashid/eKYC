#include "eKYCEngine.h"

#include <exception>
#include <iostream>
#include <thread>

#include "Config.h"
#include "concurrent/logbuffer/Header.h"
#include "loggerlib.h"

eKYCEngine::eKYCEngine() noexcept
    : _running(false), _packetsReceived(0), _messageHandler() {
    try {
        auto &cfg = Config::get();
        _aeron = std::make_unique<aeron_wrapper::Aeron>(cfg.AERON_DIR);
        ShardedLogger::get().info_fast(ShardId,
                                       "Connected to Aeron Media Driver...");
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

        _ring = std::make_unique<aeron_wrapper::RingBuffer>(1 << 20);

        _running = true;
    } catch (const std::exception &e) {
        ShardedLogger::get().info_fast(ShardId, "Error: {}", e.what());
    }
}

eKYCEngine::~eKYCEngine() noexcept { stop(); }

void eKYCEngine::start() noexcept {
    if (!_running) return;

    ShardedLogger::get().info_fast(ShardId, "Starting eKYC engine...");

    _backgroundPoller = _subscription->start_background_polling(
        [this](const aeron_wrapper::FragmentData &fragmentData) {
            if (_ring) _ring->write_buffer(fragmentData);
        });

    _consumerThread =
        std::make_unique<std::thread>(&eKYCEngine::consumer_loop, this);
}

void eKYCEngine::stop() noexcept {
    if (!_running) return;

    if (_backgroundPoller) {
        _backgroundPoller->stop();
    }
    _running = false;

    if (_consumerThread && _consumerThread->joinable()) {
        _consumerThread->join();
    }

    ShardedLogger::get().info_fast(ShardId, "eKYC engine stopped.");
}

void eKYCEngine::consumer_loop() noexcept {
    while (_running) {
        if (_ring) {
            int processed = 0;
            _ring->read_buffer([this, &processed](int8_t /*msgType*/,
                                                  char *base, int32_t offset,
                                                  int32_t length,
                                                  int32_t capacity) -> bool {
                aeron::concurrent::AtomicBuffer ab(
                    reinterpret_cast<std::uint8_t *>(base), capacity);
                aeron::concurrent::logbuffer::Header hdr(0, 0, nullptr);
                aeron_wrapper::FragmentData fd{ab, length, offset, hdr};
                process_message(fd);
                ++processed;
                return true;
            });
            if (processed == 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
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
        ShardedLogger::get().error_fast(ShardId, "Error: {}", e.what());
    }
}

void eKYCEngine::send_response(std::vector<char> &buffer) noexcept {
    if (!_publication) return;

    auto result = _publication->offer(
        reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    if (result == aeron_wrapper::PublicationResult::SUCCESS) {
        ShardedLogger::get().info_fast(ShardId, "Response sent successfully");
    } else {
        ShardedLogger::get().error_fast(ShardId, "Failed to send response: {}",
                                        pubresult_to_string(result));
    }
}