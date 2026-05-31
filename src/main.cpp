#include "sched.hpp"
#include "logger.hpp"
#include "action.hpp"

#include <iostream>
#include <memory>

/**
 * Minimal runnable entry point.
 *
 * Registers three services with different recovery sequences, then simulates
 * a series of failures to demonstrate escalation and state querying.
 */
int main() {
    using namespace srs;

    auto executor  = std::make_shared<LoggingExecutor>(std::cout);
    Scheduler scheduler(executor);

    // --- Registration phase ------------------------------------------------
    // auth-service: typical web-service — try restarting twice, then stop.
    scheduler.add_service("auth-service", {
        RecoveryAction::Restart,
        RecoveryAction::Restart,
        RecoveryAction::Stop,
        RecoveryAction::Disable,
    });

    // db-proxy: more conservative — stop on first failure, then disable.
    scheduler.add_service("db-proxy", {
        RecoveryAction::Stop,
        RecoveryAction::Disable,
        RecoveryAction::Alert,
    });

    // worker: aggressive restart, final alert to operator.
    scheduler.add_service("worker", {
        RecoveryAction::Restart,
        RecoveryAction::Restart,
        RecoveryAction::Restart,
        RecoveryAction::Alert,
    });

    // --- Failure simulation -------------------------------------------------
    std::cout << "\n=== Simulating failures ===\n\n";

    // auth-service: 4 consecutive failures → full escalation through the sequence
    for (int i = 0; i < 4; ++i) {
        scheduler.fail_service("auth-service");
    }

    std::cout << '\n';

    // db-proxy: 2 failures
    for (int i = 0; i < 2; ++i) {
        scheduler.fail_service("db-proxy");
    }

    std::cout << '\n';

    // worker: recovers after 2 failures, then fails again
    scheduler.fail_service("worker");
    scheduler.fail_service("worker");
    std::cout << "[info] worker recovered — resetting state\n";
    scheduler.reset("worker");
    scheduler.fail_service("worker");   // should restart from the top

    // --- State query -------------------------------------------------------
    std::cout << "\n=== Current service states ===\n";
    scheduler.for_each([](const std::string& name, const ServiceState& s) {
        std::cout << "  " << name
                  << "  failures=" << s.failure_count
                  << "  level=" << s.level
                  << "  last_action=" << (s.last_action ? action_name(*s.last_action) : "none")
                  << "  exhausted=" << (s.exhausted ? "yes" : "no")
                  << '\n';
    });

    return 0;
}
