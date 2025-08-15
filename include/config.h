#pragma once
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

class Config {
   public:
    // Get the single instance
    static Config& getInstance(const std::string& filename = "../config.txt") {
        static Config instance(filename);
        return instance;
    }

    // Delete copy & move
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    // Variables
    std::string LOG_DIR;
    int ROTATIING_LOG_SIZE;

    int MAIN_THREAD_SHARD_ID;
    uint8_t NUM_SHARDS;
    size_t MAX_RING_BUFFER_SIZE;

    std::string AERON_PROTOCOL;
    std::string SUBSCRIPTION_IP;
    int SUBSCRIPTION_PORT;
    std::string PUBLICATION_IP;
    int PUBLICATION_PORT;
    int SUBSCRIPTION_STREAM_ID;
    int PUBLICATION_STREAM_ID;

    std::string DB_HOST;
    std::string DB_PORT;
    std::string DB_NAME;
    std::string DB_USER;
    std::string DB_PASSWORD;

    int SHARD_TIMEOUT_MS;
    int IDLE_STRATEGY_SPINS;
    int IDLE_STRATEGY_YIELDS;

   private:
    // Private constructor
    explicit Config(const std::string& filename) {
        if (!load(filename)) {
            throw std::runtime_error("Failed to load config file: " + filename);
        }
    }

    bool load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Could not open config file: " << filename << "\n";
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            // Remove whitespace
            line.erase(std::remove_if(line.begin(), line.end(), ::isspace),
                       line.end());
            if (line.empty() || line[0] == '#') continue;

            auto pos = line.find('=');
            if (pos == std::string::npos) continue;

            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            if (key == "LOG_DIR")
                LOG_DIR = value;
            else if (key == "ROTATIING_LOG_SIZE")
                ROTATIING_LOG_SIZE = std::stoi(value);
            else if (key == "MAIN_THREAD_SHARD_ID")
                MAIN_THREAD_SHARD_ID = std::stoi(value);
            else if (key == "NUM_SHARDS")
                NUM_SHARDS = static_cast<uint8_t>(std::stoi(value));
            else if (key == "MAX_RING_BUFFER_SIZE")
                MAX_RING_BUFFER_SIZE = std::stoull(value);
            else if (key == "AERON_PROTOCOL")
                AERON_PROTOCOL = value;
            else if (key == "SUBSCRIPTION_IP")
                SUBSCRIPTION_IP = value;
            else if (key == "SUBSCRIPTION_PORT")
                SUBSCRIPTION_PORT = std::stoi(value);
            else if (key == "PUBLICATION_IP")
                PUBLICATION_IP = value;
            else if (key == "PUBLICATION_PORT")
                PUBLICATION_PORT = std::stoi(value);
            else if (key == "SUBSCRIPTION_STREAM_ID")
                SUBSCRIPTION_STREAM_ID = std::stoi(value);
            else if (key == "PUBLICATION_STREAM_ID")
                PUBLICATION_STREAM_ID = std::stoi(value);
            else if (key == "DB_HOST")
                DB_HOST = value;
            else if (key == "DB_PORT")
                DB_PORT = value;
            else if (key == "DB_NAME")
                DB_NAME = value;
            else if (key == "DB_USER")
                DB_USER = value;
            else if (key == "DB_PASSWORD")
                DB_PASSWORD = value;
            else if (key == "SHARD_TIMEOUT_MS")
                SHARD_TIMEOUT_MS = std::stoi(value);
            else if (key == "IDLE_STRATEGY_SPINS")
                IDLE_STRATEGY_SPINS = std::stoi(value);
            else if (key == "IDLE_STRATEGY_YIELDS")
                IDLE_STRATEGY_YIELDS = std::stoi(value);
        }

        return true;
    }
};