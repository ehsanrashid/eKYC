#pragma once
#include <cstdint>
#include <string>

namespace config {
// Sharding Configuration
constexpr uint8_t NUM_SHARDS = 4;
constexpr int MAX_RING_BUFFER_SIZE = 4096;
// Abstracted away by the aeronWrapper constexpr int MAX_FRAGMENT_POLL_SIZE =
// 10;

// Aeron Configuration
constexpr const char* AERON_PROTOCOL = "aeron:ipc";
constexpr const char* SUBSCRIPTION_IP = "0.0.0.0";
constexpr int SUBSCRIPTION_PORT = 50000;
constexpr const char* PUBLICATION_IP = "anas.eagri.com";
constexpr int PUBLICATION_PORT = 10001;
constexpr int SUBSCRIPTION_STREAM_ID = 1001;
constexpr int PUBLICATION_STREAM_ID = 1001;
}  // namespace config