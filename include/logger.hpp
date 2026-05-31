#pragma once

#include "executor.hpp"
#include <ostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace srs {

/**
 * Concrete IActionExecutor that logs actions to a provided ostream.
 *
 * This is the "dummy" implementation required by the spec: it performs no
 * real system calls but records every dispatched action with a timestamp,
 * making output auditable and testable by capturing the stream.
 */
class LoggingExecutor : public IActionExecutor {
public:
    explicit LoggingExecutor(std::ostream& out) : out_(out) {}

    void run(const std::string& name, RecoveryAction action) override {
        out_ << "[" << timestamp() << "] "
             << "ACTION " << action_name(action)
             << " on service '" << name << "'\n";
        out_.flush();
    }

private:
    std::ostream& out_;

    static std::string timestamp() {
        const auto now  = std::chrono::system_clock::now();
        const auto time = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    }
};

}  // namespace srs
