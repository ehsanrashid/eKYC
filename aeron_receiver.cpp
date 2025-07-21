#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include "Aeron.h"
#include "Context.h"
#include "Subscription.h"
#include "FragmentAssembler.h"
#include "output/my_app_messages/IdentityMessage.h"
#include "output/my_app_messages/MessageHeader.h"
void fragmentHandler(const aeron::AtomicBuffer& buffer,
                     aeron::util::index_t offset,
                     aeron::util::index_t length,
                     const aeron::Header& /*header*/) {
    using namespace my::app::messages;
    try {
        // 1. Wrap the header at the correct offset
        MessageHeader msgHeader;
        msgHeader.wrap(reinterpret_cast<char*>(buffer.buffer()), offset, 0, buffer.capacity());

        // 2. Advance offset past the header
        offset += msgHeader.encodedLength();

        // 3. Check template ID and decode the message
        if (msgHeader.templateId() == IdentityMessage::sbeTemplateId()) {
            IdentityMessage identity;
            identity.wrapForDecode(reinterpret_cast<char*>(buffer.buffer()),
                                  offset,
                                  msgHeader.blockLength(),
                                  msgHeader.version(),
                                  buffer.capacity());

            // Print fields
            std::cout << "msg: " << identity.msg().getCharValAsString() << "\n";
            std::cout << "type: " << identity.type().getCharValAsString() << "\n";
            std::cout << "id: " << identity.id().getCharValAsString() << "\n";
            std::cout << "name: " << identity.name().getCharValAsString() << "\n";
            std::cout << "dateOfIssue: " << identity.dateOfIssue().getCharValAsString() << "\n";
            std::cout << "dateOfExpiry: " << identity.dateOfExpiry().getCharValAsString() << "\n";
            std::cout << "address: " << identity.address().getCharValAsString() << "\n";
            std::cout << "verified: " << identity.verified().getCharValAsString() << "\n";
        } else {
            std::cerr << "[Decoder] Unexpected template ID: " << msgHeader.templateId() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[Decoder] Error: " << e.what() << std::endl;
    }
}
// Poller class encapsulates polling logic for Aeron subscription
class Poller {
public:
    Poller(std::shared_ptr<aeron::Subscription> sub, std::atomic<bool>& running)
        : subscription_(std::move(sub)), running_(running) {}
    void start() {
        aeron::FragmentAssembler assembler(fragmentHandler);
        std::cout << "Waiting for messages... (Press Ctrl+C to stop)" << std::endl;
        while (running_) {
            int fragments = subscription_->poll(assembler.handler(), 10);
            if (fragments > 0) {
                std::cout << "Processed " << fragments << " fragments\n";
            } else if (!subscription_->isConnected()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }
private:
    std::shared_ptr<aeron::Subscription> subscription_;
    std::atomic<bool>& running_;
};
std::atomic<bool> running{true};
void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    running = false;
}
int main()
{
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::cout << "Starting Aeron Subscriber on 172.17.10.58:50000" << std::endl;
    try {
        // Create Aeron context
        auto ctx = std::make_shared<aeron::Context>();
        ctx->aeronDir("/dev/shm/aeron-huzaifa"); // Match your publisher's directory
        // Connect to Aeron
        auto aeronClient = aeron::Aeron::connect(*ctx);
        if (!aeronClient) {
            std::cerr << "Failed to connect to Aeron" << std::endl;
            return 1;
        }
        std::cout << "Connected to Aeron media driver" << std::endl;
        // Create subscription - this should match your publisher's channel
        std::string channel = "aeron:udp?endpoint=0.0.0.0:50000|reliable=true";
        std::int32_t streamId = 1001;
        std::cout << "Creating subscription on channel: " << channel << std::endl;
        std::cout << "Stream ID: " << streamId << std::endl;
        std::int64_t subscriptionId = aeronClient->addSubscription(channel, streamId);
        // Wait for subscription to be ready
        std::shared_ptr<aeron::Subscription> subscription;
        int attempts = 0;
        while (!subscription && attempts < 100) {
            attempts++;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            subscription = aeronClient->findSubscription(subscriptionId);
        }
        if (!subscription) {
            std::cerr << "Failed to create subscription after " << attempts << " attempts" << std::endl;
            return 1;
        }
        std::cout << "Subscription created successfully after " << attempts << " attempts" << std::endl;
        std::cout << "Subscription channel: " << subscription->channel() << std::endl;
        std::cout << "Subscription stream ID: " << subscription->streamId() << std::endl;
        // Create Poller and start polling
        Poller poller(subscription, running);
        poller.start();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}