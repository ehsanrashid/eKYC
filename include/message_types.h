#pragma once

#include <string>

namespace ekyc_message {

// Message types as constants to avoid magic strings
const std::string IDENTITY_VERIFICATION_REQUEST =
    "Identity Verification Request";
const std::string IDENTITY_VERIFICATION_RESPONSE =
    "Identity Verification Response";
const std::string ADD_USER_REQUEST = "Add User in System";

// Helper functions
inline bool is_verification_request(const std::string& msg_type) {
    return msg_type == IDENTITY_VERIFICATION_REQUEST;
}

inline bool is_add_user_request(const std::string& msg_type) {
    return msg_type == ADD_USER_REQUEST;
}

inline std::string get_response_message() {
    return IDENTITY_VERIFICATION_RESPONSE;
}

}  // namespace ekyc_message