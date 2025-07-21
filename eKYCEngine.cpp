#include "eKYCEngine.h"

#include <chrono>
#include <exception>
#include <iostream>
#include "IdentityMessage.h"
#include "MessageHeader.h"
#include <vector>
#include <csignal>
#include <atomic>

static std::atomic<bool> running_sender{true};
static std::atomic<bool> running_receiver{true};

void signalHandlerSender(int signal) {
    Log.info("Received signal " + std::to_string(signal) + ", shutting down sender...");
    running_sender = false;
}

void signalHandlerReceiver(int signal) {
    Log.info("Received signal " + std::to_string(signal) + ", shutting down receiver...");
    running_receiver = false;
}

eKYCEngine::eKYCEngine() : running_(false) {
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

void eKYCEngine::run_sender() 
{
    std::signal(SIGINT, signalHandlerSender);
    std::signal(SIGTERM, signalHandlerSender);
    Log.info("Starting Aeron Sender on " + std::string(PublicationChannel));
    try {
        std::string aeronDir = "/dev/shm/aeron-huzaifa";
        std::string channel = "aeron:udp?endpoint=anas.eagri.com:10001|reliable=true";
        std::int32_t streamId = 1001;
        aeron_wrapper::Aeron aeronClient(aeronDir);
        auto publication = aeronClient.create_publication(channel, streamId);
        if (!publication) {
            std::cerr << "Failed to create publication" << std::endl;
            return;
        }
        Log.info("Publication created successfully.");
        using namespace my::app::messages;
        const size_t bufferCapacity = MessageHeader::encodedLength() + IdentityMessage::sbeBlockLength();
        std::vector<char> sbeBuffer(bufferCapacity, 0);
        size_t offset = 0;
        MessageHeader msgHeader;
        msgHeader.wrap(sbeBuffer.data(), offset, 0, bufferCapacity);
        msgHeader.blockLength(IdentityMessage::sbeBlockLength());
        msgHeader.templateId(IdentityMessage::sbeTemplateId());
        msgHeader.schemaId(IdentityMessage::sbeSchemaId());
        msgHeader.version(IdentityMessage::sbeSchemaVersion());
        offset += msgHeader.encodedLength();
        IdentityMessage identity;
        identity.wrapForEncode(sbeBuffer.data(), offset, bufferCapacity);
        identity.msg().putCharVal("Identity denied presence");
        identity.type().putCharVal("passport");
        identity.id().putCharVal("1231321314124");
        identity.name().putCharVal("Huzaifa Ahmed");
        identity.dateOfIssue().putCharVal("2021-01-01");
        identity.dateOfExpiry().putCharVal("2025-01-01");
        identity.address().putCharVal("Hello");
        identity.verified().putCharVal("false");
        while (running_sender) {
            if (!publication->is_connected()) {
                Log.info("No subscribers connected. Skipping send...");
            } else {
                auto result = publication->offer(reinterpret_cast<const uint8_t*>(sbeBuffer.data()), bufferCapacity);
                if (result != aeron_wrapper::PublicationResult::SUCCESS) {
                    Log.info("Offer failed (backpressure or not connected), retrying...");
                } else {
                    Log.info("SBE message sent successfully.");
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

void eKYCEngine::run_receiver() {
    std::signal(SIGINT, signalHandlerReceiver);
    std::signal(SIGTERM, signalHandlerReceiver);
    Log.info("Starting Aeron Receiver on " + std::string(SubscriptionChannel));
    try {
        std::string aeronDir = "/dev/shm/aeron-huzaifa";
        std::string channel = "aeron:udp?endpoint=0.0.0.0:50000|reliable=true";
        std::int32_t streamId = 1001;
        aeron_wrapper::Aeron aeronClient(aeronDir);
        auto subscription = aeronClient.create_subscription(channel, streamId);
        if (!subscription) {
            Log.error("Failed to create subscription");
            return;
        }
        Log.info("Subscription created successfully.");
        auto fragmentHandler = [](const aeron_wrapper::FragmentData& fragmentData) {
            using namespace my::app::messages;
            try {
                MessageHeader msgHeader;
                msgHeader.wrap(reinterpret_cast<char*>(const_cast<uint8_t*>(fragmentData.buffer)), 0, 0, fragmentData.length);
                size_t offset = msgHeader.encodedLength();
                if (msgHeader.templateId() == IdentityMessage::sbeTemplateId()) {
                    IdentityMessage identity;
                    identity.wrapForDecode(reinterpret_cast<char*>(const_cast<uint8_t*>(fragmentData.buffer)),
                                          offset,
                                          msgHeader.blockLength(),
                                          msgHeader.version(),
                                          fragmentData.length);
                    Log.info("msg: " + identity.msg().getCharValAsString());
                    Log.info("type: " + identity.type().getCharValAsString());
                    Log.info("id: " + identity.id().getCharValAsString());
                    Log.info("name: " + identity.name().getCharValAsString());
                    Log.info("dateOfIssue: " + identity.dateOfIssue().getCharValAsString());
                    Log.info("dateOfExpiry: " + identity.dateOfExpiry().getCharValAsString());
                    Log.info("address: " + identity.address().getCharValAsString());
                    Log.info("verified: " + identity.verified().getCharValAsString());
                } else {
                    std::cerr << "[Decoder] Unexpected template ID: " << msgHeader.templateId() << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "[Decoder] Error: " << e.what() << std::endl;
            }
        };
        Log.info("Waiting for messages... (Press Ctrl+C to stop)");
        while (running_receiver) {
            int fragments = subscription->poll(fragmentHandler, 10);
            if (fragments > 0) {
                Log.info("Processed " + std::to_string(fragments) + " fragments");
            } else if (!subscription->is_connected()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}
