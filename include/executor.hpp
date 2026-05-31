#pragma once

#include "action.hpp"
#include <string>

namespace srs {

/**
 * Pure-virtual interface for executing a recovery action against a named service.
 *
 * Design rationale: keeping execution separate from scheduling allows the
 * scheduler to be unit-tested without touching real system resources, and
 * makes it trivial to swap in production implementations (systemd, Docker, etc.)
 */
class IActionExecutor {
public:
    virtual ~IActionExecutor() = default;

    /**
     * Execute @p action for the service identified by @p name.
     * Implementations are expected to be synchronous and idempotent.
     * Throws std::runtime_error on unrecoverable execution failure.
     */
    virtual void run(const std::string& name, RecoveryAction action) = 0;
};

}  // namespace srs
