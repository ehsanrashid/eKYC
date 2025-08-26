#include "Message.h"

#include <iostream>

Message::Message(MessageType mType, int mId) noexcept
    : msgType(mType), msgId(mId) {}

OrderMessage::OrderMessage(int msgId, std::string sym, int qty,
                           double prc) noexcept
    : Message(MT_ORDER, msgId),
      symbol(std::move(sym)),
      quantity(qty),
      price(prc) {}

StepResult OrderMessage::validate() const noexcept {
    std::cout << "[Order " << msgId << "] Validate symbol=" << symbol << "\n";

    if (quantity <= 0 || price <= 0.0) {
        std::cerr << "[Order " << msgId << "] Invalid quantity/price\n";
        return StepResult::FAILED;
    }
    return StepResult::SUCCESS;
}

CancelMessage::CancelMessage(int msgId, int cId) noexcept
    : Message(MT_CANCEL, msgId), cancelId(cId) {}

StepResult CancelMessage::validate() const noexcept {
    std::cout << "[Cancel " << msgId << "] Validate cancelId=" << cancelId
              << "\n";

    if (cancelId <= 0) {
        std::cerr << "[Cancel " << msgId << "] Invalid cancelId\n";
        return StepResult::FAILED;
    }
    return StepResult::SUCCESS;
}
