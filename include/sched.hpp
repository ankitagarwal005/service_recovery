#pragma once

#include "executor.hpp"
#include "record.hpp"
#include "action.hpp"

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>

namespace srs {

/**
 * Central scheduler that owns service registrations and dispatches recovery
 * actions when failures are reported.
 *
 * Lifecycle
 * ---------
 *  1. Construct with an IActionExecutor.
 *  2. Call add_service() for each monitored service before accepting failures.
 *  3. Call fail_service() whenever a service reports a failure.
 *  4. Query state via get_state() or for_each().
 *
 * Design decisions
 * ----------------
 * - Registration is separate from failure handling (two clear phases) so that
 *   startup configuration (file, flags, etc.) is decoupled from the runtime loop.
 * - The executor is injected rather than hardcoded, enabling unit-testing without
 *   system calls and production flexibility (systemd, Docker, custom hooks).
 * - Services are keyed by name (string); names are case-sensitive.
 * - fail_service() is synchronous: it calls executor_.run() inline and returns
 *   only after the action completes. Async dispatch is intentionally out-of-scope.
 */
class Scheduler {
public:
    explicit Scheduler(std::shared_ptr<IActionExecutor> executor);

    /**
     * Register a service with its ordered recovery sequence.
     *
     * @param name      Unique service identifier (case-sensitive).
     * @param sequence  Ordered list of actions; must not be empty.
     * @throws std::invalid_argument if name is empty, sequence is empty,
     *         or the service is already registered.
     */
    void add_service(const std::string& name,
                     std::vector<RecoveryAction> sequence);

    /**
    * Report a failure for @p name.
     * Selects the next action in the service's sequence, dispatches it via
     * the executor, and returns the action that was taken.
     *
     * @throws std::out_of_range   if the service is not registered.
     * @throws std::runtime_error  propagated from the executor on failure.
     */
    RecoveryAction fail_service(const std::string& name);

    /**
    * Reset the escalation state of @p name (e.g. after recovery confirmed).
     * @throws std::out_of_range if the service is not registered.
     */
    void reset(const std::string& name);

    /**
     * Query the current state of a service.
     * @throws std::out_of_range if the service is not registered.
     */
    [[nodiscard]] ServiceState get_state(const std::string& name) const;

    /// Returns true if @p name has been registered.
    [[nodiscard]] bool has_service(const std::string& name) const noexcept;

    /// Iterate over all registered services (name, state pairs).
    void for_each(
        const std::function<void(const std::string&, const ServiceState&)>& fn) const;

private:
    std::shared_ptr<IActionExecutor>              executor_;
    std::unordered_map<std::string, ServiceRecord> services_;
};

}  // namespace srs
