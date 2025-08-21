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
#include "loggerlib.h"

const int ShardId = Config::get().MAIN_THREAD_SHARD_ID;

int main(int argc, char** argv) {
    Sharded_Logger::getInstance().initialize(1, "logs/sharded_app");
    Sharded_Logger::getInstance().set_log_level_all(
        Sharded_Logger::LogLevel::DEBUG);

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
        Sharded_Logger::getInstance().error_fast(ShardId, "Error: {}",
                                                 e.what());
        return 1;
    }
}
