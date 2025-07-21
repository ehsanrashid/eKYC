// Cpp Standard Header Lib
#include <atomic>
#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

// Local Headers include
#include "eKYCEngine.h"
#include "logger.h"
#include "loggerwrapper.h"
Logger Log("logs/Gateway_SBE.log", 10 * 1024 * 1024);

int main(int argc, char** argv) 
{
    Log.set_log_level(LogLevel::DEBUG);

    std::atomic<bool> keepRunning{true};
    // Start input monitoring thread
    std::thread inputThread([&keepRunning]() {
        std::cin.get();  // Wait for Enter key
        keepRunning = false;
    });

    try {
        auto eKYC = std::make_unique<eKYCEngine>();

        eKYC->start();
        eKYC->run_sender();

        while (keepRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        if (inputThread.joinable()) inputThread.join();

        eKYC->stop();        
        return 0;
    } catch (const std::exception& e) {
        Log.error(std::string("Error: ") + e.what());
        return 1;
    }
}
