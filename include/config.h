#pragma once
#include <cstdint>
#include <string>

namespace config {
// Logging configuration
constexpr const char* LOG_DIR = "../logs/ekyc";
constexpr int ROTATIING_LOG_SIZE = 0;

// Sharding configuration
constexpr int MAIN_THREAD_SHARD_ID = 0;
constexpr uint8_t NUM_SHARDS = 4;
constexpr size_t MAX_RING_BUFFER_SIZE = 8192;

// Aeron configuration
constexpr const char* AERON_PROTOCOL = "ipc";
constexpr const char* SUBSCRIPTION_IP = "0.0.0.0";
constexpr const char* SUBSCRIPTION_PORT_STR = "50000";
constexpr int SUBSCRIPTION_PORT = 50000;
constexpr const char* PUBLICATION_IP = "anas.eagri.com";
constexpr const char* PUBLICATION_PORT_STR = "10001";
constexpr int PUBLICATION_PORT = 10001;
constexpr int SUBSCRIPTION_STREAM_ID = 1001;
constexpr int PUBLICATION_STREAM_ID = 1001;

// Database configuration
constexpr const char* DB_HOST = "localhost";
constexpr const char* DB_PORT = "5432";
constexpr const char* DB_NAME = "ekycdb";
constexpr const char* DB_USER = "huzaifa";
constexpr const char* DB_PASSWORD = "3214";

// Performance tuning
constexpr int SHARD_TIMEOUT_MS = 50;
constexpr int IDLE_STRATEGY_SPINS = 100;
constexpr int IDLE_STRATEGY_YIELDS = 1000;
}  // namespace config