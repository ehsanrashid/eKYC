
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "include/aeron_wrapper.h"

const std::string AeronDir = "";
const std::string AeronChannel =
    //"aeron:udp?endpoint=localhost:40123";
    //"aeron:udp?endpoint=172.17.10.58:40123";
    "aeron:udp?endpoint=239.101.9.9:40123";

void process_incoming_message(const aeron_wrapper::FragmentData& fragmentData) {
    try {
        std::cout << "msg: " << fragmentData.as_string() << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error processing message: " << e.what() << std::endl;
    }
}

int main(int argc, char** argv) {
    std::atomic<bool> keepRunning{true};

    // Start input monitoring thread
    std::thread inputThread([&keepRunning]() {
        std::cin.get();  // Wait for Enter key
        keepRunning = false;
    });

    try {
        auto aeron = std::make_unique<aeron_wrapper::Aeron>(AeronDir);
        std::cout << "Connected to Aeron Media Driver" << std::endl;
        auto subscription = aeron->create_subscription(AeronChannel, 100);

        // Start background msg processing
        auto backgroundPoller = subscription->start_background_polling(
            [](const aeron_wrapper::FragmentData& fragmentData) {
                process_incoming_message(fragmentData);
            });

        while (keepRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        inputThread.join();
        backgroundPoller->stop();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
