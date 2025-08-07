#include "MessageHandler.h"

#include <exception>
#include <iostream>
#include <mutex>
#include <sstream>

#include "config.h"
#include "helper.h"
#include "messages/Char64str.h"
#include "messages/IdentityMessage.h"
#include "messages/MessageHeader.h"

// Add utils include:

#include "config.h"
#include "helper.h"
#include "messages/Char64str.h"
#include "messages/IdentityMessage.h"
#include "messages/MessageHeader.h"
#include "utils.h"

namespace {
#ifdef USE_PG_WRAPPER
static std::mutex db_mutex;  // Global mutex for database operations
#endif

void log_identity(my::app::messages::IdentityMessage &identity) {
    Log.info_fast(ShardId, "msg: {}", identity.msg().getCharValAsString());
    Log.info_fast(ShardId, "type: {}", identity.type().getCharValAsString());
    Log.info_fast(ShardId, "id: {}", identity.id().getCharValAsString());
    Log.info_fast(ShardId, "name: {}", identity.name().getCharValAsString());
    Log.info_fast(ShardId, "dateOfIssue: {}",
                  identity.dateOfIssue().getCharValAsString());
    Log.info_fast(ShardId, "dateOfExpiry: {}",
                  identity.dateOfExpiry().getCharValAsString());
    Log.info_fast(ShardId, "address: {}",
                  identity.address().getCharValAsString());
    Log.info_fast(ShardId, "verified: {}",
                  identity.verified().getCharValAsString());
}

}  // namespace

MessageHandler::MessageHandler() noexcept {
#ifdef USE_PG_WRAPPER
    try {
        // Initialize database connection with connection pooling
        db_ = std::make_unique<pg_wrapper::Database>(
            config::DB_HOST, config::DB_PORT, config::DB_NAME, config::DB_USER,
            config::DB_PASSWORD);

        // Test connection
        auto result = db_->exec("SELECT 1");
        if (!result.empty()) {
            db_connected_.store(true);
            Log.info_fast(ShardId, "Connected to PostgreSQL successfully");
        } else {
            Log.error_fast(ShardId, "Failed to connect to PostgreSQL");
        }
    } catch (const std::exception &e) {
        Log.error_fast(ShardId, "PostgreSQL connection error: {}", e.what());
        db_connected_.store(false);
    }
#else
    Log.info_fast(ShardId, "MessageHandler initialized (simulated mode)");
#endif
}

MessageHandler::~MessageHandler() noexcept {
#ifdef USE_PG_WRAPPER
    if (db_ && db_connected_.load()) {
        db_->close();
        Log.info_fast(ShardId, "PostgreSQL connection closed");
    }
#endif
    Log.info_fast(ShardId, "MessageHandler destroyed");
}

#ifdef USE_PG_WRAPPER
bool MessageHandler::ensure_connection() noexcept {
    if (!db_connected_.load()) {
        try {
            reconnect_if_needed();
        } catch (const std::exception &e) {
            Log.error_fast(ShardId, "Failed to reconnect to database: {}",
                           e.what());
            return false;
        }
    }
    return db_connected_.load();
}

void MessageHandler::reconnect_if_needed() noexcept {
    try {
        if (db_) {
            db_->close();
        }

        db_ = std::make_unique<pg_wrapper::Database>(
            config::DB_HOST, config::DB_PORT, config::DB_NAME, config::DB_USER,
            config::DB_PASSWORD);

        auto result = db_->exec("SELECT 1");
        if (!result.empty()) {
            db_connected_.store(true);
            Log.info_fast(ShardId, "Reconnected to PostgreSQL successfully");
        }
    } catch (const std::exception &e) {
        Log.error_fast(ShardId, "Reconnection failed: {}", e.what());
        db_connected_.store(false);
    }
}
#endif

// Check if user exists in database
bool MessageHandler::exist_user(const std::string &identityNumber,
                                const std::string &name) noexcept {
#ifdef USE_PG_WRAPPER
    if (!ensure_connection()) {
        Log.error_fast(ShardId, "Database not connected for user check");
        return false;
    }

    try {
        // Thread-safe database access
        std::lock_guard<std::mutex> lock(db_mutex);

        // Use utility function to build query
        std::string query = ekyc_utils::build_select_sql(
            "users", {"identity_number", "name"}, "identity_number",
            identityNumber, "name", name);

        auto result = db_->exec(query);
        bool exists = !result.empty();

        Log.info_fast(ShardId,
                      exists ? "Verified: {} {} found in database"
                             : "NOT verified: {} {} not found in database",
                      identityNumber, name);

        return exists;
    } catch (const std::exception &e) {
        Log.error_fast(ShardId, "Database query error: {}", e.what());
        db_connected_.store(false);  // Mark as disconnected
        return false;
    }
#else
    // Fallback to simulated mode
    try {
        bool exists = (identityNumber.find("421") != std::string::npos);
        Log.info_fast(
            ShardId,
            exists ? "Verified: {} {} found in database (simulated)"
                   : "NOT verified: {} {} not found in database (simulated)",
            identityNumber, name);
        return exists;
    } catch (const std::exception &e) {
        Log.error_fast(ShardId, "Error during user existence check: {}",
                       e.what());
        return false;
    }
#endif
}

// Add user to database
bool MessageHandler::add_identity(
    my::app::messages::IdentityMessage &identity) noexcept {
#ifdef USE_PG_WRAPPER
    if (!ensure_connection()) {
        Log.error_fast(ShardId, "Database not connected for adding user");
        return false;
    }

    try {
        std::string type = identity.type().getCharValAsString();
        std::string identityNumber = identity.id().getCharValAsString();
        std::string name = identity.name().getCharValAsString();
        std::string dateOfIssue = identity.dateOfIssue().getCharValAsString();
        std::string dateOfExpiry = identity.dateOfExpiry().getCharValAsString();
        std::string address = identity.address().getCharValAsString();

        Log.info_fast(ShardId, "Adding user to system: name={}, id={}, type={}",
                      name, identityNumber, type);

        // Check if user already exists
        if (exist_user(identityNumber, name)) {
            Log.info_fast(ShardId, "User already exists: {} {}", name,
                          identityNumber);
            return false;
        }

        // Thread-safe database access
        std::lock_guard<std::mutex> lock(db_mutex);

        // Use utility function to build insert query
        std::string query = ekyc_utils::build_insert_sql(
            "users",
            {"type", "identity_number", "name", "date_of_issue",
             "date_of_expiry", "address"},
            {type, identityNumber, name, dateOfIssue, dateOfExpiry, address});

        db_->exec(query);

        Log.info_fast(ShardId, "User successfully added: {} {} ({})", name,
                      identityNumber, type);
        return true;

    } catch (const std::exception &e) {
        Log.error_fast(ShardId, "Database error while adding user: {}",
                       e.what());
        db_connected_.store(false);
        return false;
    }
#else
    // Fallback to simulated mode
    try {
        std::string type = identity.type().getCharValAsString();
        std::string identityNumber = identity.id().getCharValAsString();
        std::string name = identity.name().getCharValAsString();

        Log.info_fast(
            ShardId,
            "Adding user to system (simulated): name={}, id={}, type={}", name,
            identityNumber, type);

        Log.info_fast(ShardId,
                      "User successfully added (simulated): {} {} ({})", name,
                      identityNumber, type);
        return true;
    } catch (const std::exception &e) {
        Log.error_fast(ShardId, "Error adding user to system: {}", e.what());
        return false;
    }
#endif
}
