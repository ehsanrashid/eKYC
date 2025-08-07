#pragma once

#include <atomic>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

#include "loggerwrapper.h"
#include "messages/IdentityMessage.h"
// Add pg_wrapper dependency
#ifdef USE_PG_WRAPPER
#include "pg_wrapper.h"
#endif

extern const int ShardId;
extern LoggerWrapper Log;

class MessageHandler final {
   public:
    MessageHandler() noexcept;
    ~MessageHandler() noexcept;

    bool exist_user(const std::string &identityNumber,
                    const std::string &name) noexcept;
    bool add_identity(my::app::messages::IdentityMessage &identity) noexcept;

   private:
#ifdef USE_PG_WRAPPER
    std::unique_ptr<pg_wrapper::Database> db_;
    std::atomic<bool> db_connected_{false};

    // Connection management
    bool ensure_connection() noexcept;
    void reconnect_if_needed() noexcept;
#endif
};
