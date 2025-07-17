
#include <atomic>
#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "eKYCEngine.h"

int main(int argc, char** argv) {
    std::atomic<bool> keepRunning{true};

    // Start input monitoring thread
    std::thread inputThread([&keepRunning]() {
        std::cin.get();  // Wait for Enter key
        keepRunning = false;
    });

    try {
        auto eKYC = eKYCEngine();

        eKYC.start();

        while (keepRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        if (inputThread.joinable()) inputThread.join();

        eKYC.stop();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
