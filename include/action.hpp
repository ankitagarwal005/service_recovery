#pragma once

#include <string>
#include <stdexcept>

namespace srs {  // service recovery scheduler

/**
 * Enumeration of supported recovery actions, ordered by escalation severity.
 * New actions should be inserted in ascending severity order.
 */
enum class RecoveryAction {
    Restart = 0,   ///< Attempt a clean restart of the service
    Stop,          ///< Stop the service without restarting
    Disable,       ///< Disable the service to prevent automatic restart
    Alert,         ///< Escalate to operator (no further automated action)
};

/// Returns a human-readable name for the given action.
[[nodiscard]] inline std::string action_name(RecoveryAction action) {
    switch (action) {
        case RecoveryAction::Restart: return "RESTART";
        case RecoveryAction::Stop:    return "STOP";
        case RecoveryAction::Disable: return "DISABLE";
        case RecoveryAction::Alert:   return "ALERT";
    }
    throw std::invalid_argument("Unknown RecoveryAction value");
}

}  // namespace srs
