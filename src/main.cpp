// Cpp Standard Header Lib
#include <atomic>
#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

// Local Headers include
#include "TimerLite.h"
#include "eKYCEngine.h"
#include "loggerwrapper.h"

const int shard_id = 0;

LoggerWrapper Log(1, "../logs/ekyc", 0);
TimerLite timer;

int main(int argc, char** argv) {
    Log.set_log_level(shard_id, LogLevel::DEBUG);

    std::atomic<bool> keepRunning{true};
    // Start input monitoring thread
    std::thread inputThread([&keepRunning]() {
        std::cin.get();  // Wait for Enter key
        keepRunning = false;
    });

    try {
        auto eKYC = std::make_unique<eKYCEngine>();

        eKYC->start();

        while (keepRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        if (inputThread.joinable()) inputThread.join();

        eKYC->stop();
        timer.report();
        return 0;
    } catch (const std::exception& e) {
        Log.error_fast(shard_id, "Error: {}", e.what());
        return 1;
    }
}
