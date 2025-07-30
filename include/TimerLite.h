#pragma once
#include <chrono>
#include <iostream>
#include <string>
#include <unordered_map>

class TimerLite {
   public:
    using Clock = std::chrono::high_resolution_clock;

    void start(const std::string &name) { startTimes[name] = Clock::now(); }

    void stop(const std::string &name) {
        auto end = Clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            end - startTimes[name])
                            .count();
        stats[name].totalTime += duration;
        stats[name].count += 1;
        if (duration > stats[name].maxTime) {
            stats[name].maxTime = duration;
        }
    }

    void report() const {
        std::cout << "\n--- Function Timing Report ---\n";
        for (const auto &[name, stat] : stats) {
            std::cout << name << ":\n"
                      << "  Calls:     " << stat.count << "\n"
                      << "  Avg Time:  " << (stat.totalTime / stat.count)
                      << " ns\n"
                      << "  Max Time:  " << stat.maxTime << " ns\n";
        }
    }

   private:
    struct Stat {
        uint64_t totalTime = 0;
        uint64_t count = 0;
        uint64_t maxTime = 0;
    };

    std::unordered_map<std::string, Clock::time_point> startTimes;
    std::unordered_map<std::string, Stat> stats;
};
