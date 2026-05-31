# Service Recovery Scheduler

A small C++17 library that manages escalating recovery actions for a set of monitored services.

## What it does

Each registered service has an ordered **recovery sequence** (e.g. `RESTART → RESTART → STOP → DISABLE`). When a service reports a failure the scheduler selects and dispatches the next action in its sequence. Consecutive failures escalate through the list. When the list is exhausted the last action is repeated indefinitely — fail-safe: keeps alerting rather than silently doing nothing.

### Supported recovery actions

| Action    | Semantics (dummy implementation logs to stdout) |
|-----------|------------------------------------------------|
| `RESTART` | Attempt a clean restart of the service         |
| `STOP`    | Stop the service without restarting            |
| `DISABLE` | Prevent automatic restart                      |
| `ALERT`   | Escalate to operator; no further automation    |

## Project layout

```
include/
  action.hpp            — RecoveryAction enum + action_name
  executor.hpp          — IActionExecutor interface
  logger.hpp            — Concrete executor: logs to std::ostream
  record.hpp            — ServiceRecord (per-service state + escalation)
  sched.hpp             — Scheduler (public API surface)
src/
  sched.cpp             — Scheduler implementation
  main.cpp              — Runnable demo
tests/
  test_sched.cpp        — 19 unit tests (Catch2-compatible)
  catch/catch.hpp       — Catch2 v2 single-header (vendored for offline builds)
```

## Building

Requires: g++ with C++17 support (GCC 8+ or Clang 7+). No other dependencies.

### Demo binary
```bash
g++ -std=c++17 -Wall -Wextra -Iinclude src/sched.cpp src/main.cpp -o srs_demo
./srs_demo
```

### Unit tests
```bash
g++ -std=c++17 -Wall -Wextra -Iinclude -Itests \
  src/sched.cpp tests/test_sched.cpp -o srs_tests
./srs_tests
```

### With CMake (requires CMake ≥ 3.16)
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

### Windows (PowerShell)
Run from the `service_recovery` folder:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
.\build\srs_demo.exe
```

If `cmake` is not recognized:

```powershell
winget install Kitware.CMake
```

### Linux
Run from the `service_recovery` folder:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/srs_demo
```

Or from inside the build folder:

```bash
mkdir -p build
cd build
cmake ..
cmake --build . -j
ctest --output-on-failure
./srs_demo
```

### WSL Note
If you use WSL on Windows:

- Run Linux commands inside the WSL terminal, not PowerShell.
- Use Linux paths (example: `/mnt/c/Users/ankiaga/Downloads/service_recovery_scheduler/service_recovery`).
- Do not mix PowerShell paths like `C:\...` with Linux commands.

Example in WSL:

```bash
cd /mnt/c/Users/ankiaga/Downloads/service_recovery_scheduler/service_recovery
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/srs_demo
```

## Design decisions

**Two-phase lifecycle.** `add_service()` is separate from `fail_service()`. Registration happens once at startup (from a config file, flags, or hardcoded policy); runtime failure handling is a separate concern. This mirrors how production service supervisors work and keeps tests simple.

**Injected executor.** `IActionExecutor` is a pure virtual interface injected into `Scheduler`. This means:
- Unit tests use `RecordingExecutor` (no I/O, inspects every call).
- Production can plug in a systemd, Docker, or custom implementation.
- The scheduler is fully testable without touching real system resources.

**Sequence-exhaustion policy.** When all actions are consumed, the last action keeps repeating. The alternative (silently doing nothing) is more dangerous for a recovery scheduler. If the last action is `Alert`, the operator keeps being notified.

**Synchronous dispatch.** `fail_service()` calls the executor inline and returns only after it completes. Async dispatch is out of scope for this size of library but would fit naturally behind the `IActionExecutor` interface.

**Case-sensitive string keys.** Service names are case-sensitive `std::string` identifiers. Simple and predictable; the caller controls the naming convention.

**Thread safety.** `Scheduler` is not thread-safe. Concurrent failure reporting for the same service requires external synchronization. This is documented and intentional — adding a mutex would be trivial but the right granularity depends on the embedding environment.

## Assumptions

- Recovery sequences are fixed at registration time (no dynamic reconfiguration).
- Failures are reported one at a time per service (no batching).
- The executor is synchronous and blocking.
- `reset()` represents "recovery confirmed externally" and resets escalation to level 0.
