#include "MessageHandler.h"

#include <iostream>

void MessageHandler::handle_message(
    const aeron_wrapper::FragmentData& fragmentData) {
    std::cout << "hello";
}
