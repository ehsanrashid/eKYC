#include "MessageFlow.h"

#include <iostream>

// Static member definition
std::unordered_map<MessageType, std::vector<MessageFlow::Step>>
    MessageFlow::registry;

void MessageFlow::register_flow(MessageType msgType,
                                std::vector<Step> steps) noexcept {
    registry[msgType] = std::move(steps);
}

void MessageFlow::initialize() noexcept {
    // Register Order flow
    register_flow(
        MT_ORDER,
        {
            [](const Message& m) { return static_cast<const OrderMessage&>(m).validate(); },
            //[&](const Message& m) { return risk.check(m); },
            //[&](const Message& m) { return router.send(m); },
        });

    // Register Cancel flow
    register_flow(MT_CANCEL,
                  {
                      [](const Message& m) {
                          return static_cast<const CancelMessage&>(m).validate();
                      },
                      //[&](const Message& m) { return risk.check(m); },
                      //[&](const Message& m) { return router.send(m); }
                  });
}

void MessageFlow::execute(const Message& msg) noexcept {
    auto itr = registry.find(msg.msgType);
    if (itr == registry.end()) {
        std::cerr << "No flow registered for message type " << msg.msgType
                  << "\n";
        return;
    }

    for (auto& step : itr->second) {
        StepResult res = step(msg);
        if (res == StepResult::FAILED) {
            std::cerr << "[Flow] Msg " << msg.msgId
                      << " stopped due to failure\n";
            return;
        }
    }
    std::cout << "[Flow] Msg " << msg.msgId << " completed successfully\n";
}
