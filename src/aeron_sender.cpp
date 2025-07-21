#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include "Aeron.h"
#include "Context.h"
#include "Publication.h"
#include "concurrent/AtomicBuffer.h"
#include "IdentityMessage.h"
#include "MessageHeader.h"

std::atomic<bool> running{true};
void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    running = false;
}

int main() 
{
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::cout << "Starting Aeron Sender" << std::endl;
    
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
        // Create publication - this should match your receiver's channel
        std::string channel = "aeron:udp?endpoint=anas.eagri.com:10001|reliable=true";
        std::int32_t streamId = 1001;
        std::cout << "Creating Publication on channel: " << channel << std::endl;
        std::cout << "Stream ID: " << streamId << std::endl;
        std::int64_t publicationId = aeronClient->addPublication(channel, streamId);
        // Wait for publication to be ready
        std::shared_ptr<aeron::Publication> publication;
        int attempts = 0;
        while (!publication && attempts < 100) {
            attempts++;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            publication = aeronClient->findPublication(publicationId);
        }
        if (!publication) {
            std::cerr << "Failed to create publication after " << attempts << " attempts" << std::endl;
            return 1;
        }
        std::cout << "Publication created successfully after " << attempts << " attempts" << std::endl;
        std::cout << "Publication channel: " << publication->channel() << std::endl;
        std::cout << "Publication stream ID: " << publication->streamId() << std::endl;

        // SBE message setup (header + body)
        using namespace my::app::messages;
        const size_t bufferCapacity = MessageHeader::encodedLength() + IdentityMessage::sbeBlockLength();
        std::vector<char> sbeBuffer(bufferCapacity, 0);
        size_t offset = 0;

        // 1. Wrap and encode the header at the correct offset
        MessageHeader msgHeader;
        msgHeader.wrap(sbeBuffer.data(), offset, 0, bufferCapacity);
        msgHeader.blockLength(IdentityMessage::sbeBlockLength());
        msgHeader.templateId(IdentityMessage::sbeTemplateId());
        msgHeader.schemaId(IdentityMessage::sbeSchemaId());
        msgHeader.version(IdentityMessage::sbeSchemaVersion());
        offset += msgHeader.encodedLength(); // Advance offset past header

        // 2. Wrap and encode the IdentityMessage at the new offset
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

        // 3. Send the full buffer (header+body)
        aeron::concurrent::AtomicBuffer atomicBuffer(reinterpret_cast<uint8_t*>(sbeBuffer.data()), bufferCapacity);
        while (running) {
            if (!publication->isConnected()) {
                std::cout << "No subscribers connected. Skipping send..." << std::endl;
            } else {
                auto result = publication->offer(atomicBuffer, 0, bufferCapacity);
                if (result < 0) {
                    std::cout << "Offer failed (backpressure?), retrying..." << std::endl;
                } else {
                    std::cout << "SBE message sent successfully." << std::endl;
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    catch (const std::exception& e) 
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}