// Cpp Standard Header Lib
#include <atomic>
#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

// Local Headers include
#include "Config.h"
#include "eKYCEngine.h"
#include "loggerwrapper.h"

const int ShardId = Config::get().MAIN_THREAD_SHARD_ID;

LoggerWrapper Log(1, Config::get().LOG_DIR, 0);

int main(int argc, char** argv) {
    Log.set_log_level(ShardId, LogLevel::DEBUG);

    // Initialize the factory with default database types
    DatabaseFactory::initialize();

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
        return 0;
    } catch (const std::exception& e) {
        Log.error_fast(ShardId, "Error: {}", e.what());
        return 1;
    }
}
