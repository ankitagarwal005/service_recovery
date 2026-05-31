#pragma once

#include "action.hpp"
#include <vector>
#include <optional>
#include <stdexcept>
#include <cstddef>

namespace srs {

/**
 * Immutable snapshot of a service's current recovery state.
 * Returned by ServiceRecord and the scheduler for external queries.
 */
struct ServiceState {
    std::size_t             failure_count  = 0;
    std::size_t             level          = 0;   ///< Current escalation level in the sequence
    std::optional<RecoveryAction> last_action;     ///< nullopt if no failure yet
    bool                    exhausted      = false; ///< true when sequence is fully consumed
};

/**
 * Mutable record for a single registered service.
 *
 * Thread-safety: NOT thread-safe; callers must synchronize externally if
 * multiple threads report failures for the same service concurrently.
 */
class ServiceRecord {
public:
    explicit ServiceRecord(std::vector<RecoveryAction> sequence)
        : sequence_(std::move(sequence))
    {
        if (sequence_.empty()) {
            throw std::invalid_argument("Recovery sequence must not be empty");
        }
    }

    /**
     * Advance the failure counter and return the next action to execute.
     * Once the sequence is exhausted the last action is repeated indefinitely
     * (fail-safe: keeps alerting rather than silently doing nothing).
     */
    [[nodiscard]] RecoveryAction next_action() {
        ++state_.failure_count;

        const RecoveryAction action = sequence_[state_.level];
        state_.last_action = action;

        if (state_.level + 1 < sequence_.size()) {
            ++state_.level;
        } else {
            state_.exhausted = true;
        }

        return action;
    }

    /// Reset escalation state (e.g. after a successful health-check).
    void reset() noexcept {
        state_ = ServiceState{};
    }

    [[nodiscard]] const ServiceState& get_state() const noexcept { return state_; }
    [[nodiscard]] const std::vector<RecoveryAction>& get_sequence() const noexcept { return sequence_; }

private:
    std::vector<RecoveryAction> sequence_;
    ServiceState                state_;
};

}  // namespace srs
