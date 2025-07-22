#include "eKYCEngine.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <iostream>
#include <vector>

#include "Char64str.h"
#include "IdentityMessage.h"
#include "MessageHeader.h"

namespace {
void log_identity(my::app::messages::IdentityMessage& identity) {
    Log.info_fast("msg: {}", identity.msg().getCharValAsString());
    Log.info_fast("type: {}", identity.type().getCharValAsString());
    Log.info_fast("id: {}", identity.id().getCharValAsString());
    Log.info_fast("name: {}", identity.name().getCharValAsString());
    Log.info_fast("dateOfIssue: {}",
                  identity.dateOfIssue().getCharValAsString());
    Log.info_fast("dateOfExpiry: {}",
                  identity.dateOfExpiry().getCharValAsString());
    Log.info_fast("address: {}", identity.address().getCharValAsString());
    Log.info_fast("verified: {}", identity.verified().getCharValAsString());
}
}  // namespace

eKYCEngine::eKYCEngine() : running_(false) {
    try {
        aeron_ = std::make_unique<aeron_wrapper::Aeron>(AeronDir);
        Log.info_fast("Connected to Aeron Media Driver...");
        subscription_ = aeron_->create_subscription(SubscriptionChannel,  //
                                                    SubscriptionStreamId);
        publication_ = aeron_->create_publication(PublicationChannel,  //
                                                  PublicationStreamId);
        running_ = true;
    } catch (const std::exception& e) {
        Log.info_fast("Error: {}", e.what());
    }
}

eKYCEngine::~eKYCEngine() { stop(); }

void eKYCEngine::start() {
    if (!running_) return;
    Log.info_fast("Starting eKYC engine...");
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
    Log.info_fast("eKYC engine stopped.");
}

void eKYCEngine::process_message(
    const aeron_wrapper::FragmentData& fragmentData) {
    using namespace my::app::messages;

    ++receiving_packets_;
    try {
        MessageHeader msgHeader;
        msgHeader.wrap(
            reinterpret_cast<char*>(const_cast<uint8_t*>(fragmentData.buffer)),
            0, 0, fragmentData.length);
        size_t offset = msgHeader.encodedLength();

        if (msgHeader.templateId() == IdentityMessage::sbeTemplateId()) {
            IdentityMessage identity;
            identity.wrapForDecode(reinterpret_cast<char*>(const_cast<uint8_t*>(
                                       fragmentData.buffer)),
                                   offset, msgHeader.blockLength(),
                                   msgHeader.version(), fragmentData.length);

            log_identity(identity);

            Log.info_fast("Packet # {} received successfully!",
                          receiving_packets_);

        } else {
            Log.error_fast("[Decoder] Unexpected template ID: {}",
                           msgHeader.templateId());
        }

    } catch (const std::exception& e) {
        Log.error_fast("Error: {}", e.what());
    }
}

// void eKYCEngine::run_sender() {
//     Log.info("Starting Aeron Sender on " + std::string(PublicationChannel));
//     try {
//         // std::string aeronDir = "/dev/shm/aeron-huzaifa";
//         std::string channel =
//             "aeron:udp?endpoint=anas.eagri.com:10001|reliable=true";
//         std::int32_t streamId = 1001;
//         aeron_wrapper::Aeron aeronClient;  // Use default
//         directory auto publication =
//             aeronClient.create_publication(channel, streamId);
//         if (!publication) {
//             std::cerr << "Failed to create publication" << std::endl;
//             return;
//         }
//         Log.info("Publication created successfully.");
//         using namespace my::app::messages;
//         const size_t bufferCapacity =
//             MessageHeader::encodedLength() +
//             IdentityMessage::sbeBlockLength();
//         std::vector<char> sbeBuffer(bufferCapacity, 0);
//         size_t offset = 0;
//         MessageHeader msgHeader;
//         msgHeader.wrap(sbeBuffer.data(), offset, 0, bufferCapacity);
//         msgHeader.blockLength(IdentityMessage::sbeBlockLength());
//         msgHeader.templateId(IdentityMessage::sbeTemplateId());
//         msgHeader.schemaId(IdentityMessage::sbeSchemaId());
//         msgHeader.version(IdentityMessage::sbeSchemaVersion());
//         offset += msgHeader.encodedLength();
//         IdentityMessage identity;
//         identity.wrapForEncode(sbeBuffer.data(), offset, bufferCapacity);
//         identity.msg().putCharVal("Identity denied presence");
//         identity.type().putCharVal("passport");
//         identity.id().putCharVal("1231321314124");
//         identity.name().putCharVal("Huzaifa Ahmed");
//         identity.dateOfIssue().putCharVal("2021-01-01");
//         identity.dateOfExpiry().putCharVal("2025-01-01");
//         identity.address().putCharVal("Hello");
//         identity.verified().putCharVal("false");
//         while (running_sender) {
//             if (!publication->is_connected()) {
//                 Log.info("No subscribers connected. Skipping send...");
//             } else {
//                 auto result = publication->offer(
//                     reinterpret_cast<const uint8_t*>(sbeBuffer.data()),
//                     bufferCapacity);
//                 if (result != aeron_wrapper::PublicationResult::SUCCESS) {
//                     Log.info("Offer failed (backpressure or not connected),
//                     retrying...");
//                 } else {
//                     Log.info("SBE message sent successfully.");
//                 }
//             }
//             std::this_thread::sleep_for(std::chrono::seconds(1));
//         }
//     } catch (const std::exception& e) {
//         std::cerr << "Exception: " << e.what() << std::endl;
//     }
// }
