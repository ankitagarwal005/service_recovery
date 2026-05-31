#define CATCH_CONFIG_MAIN
#include "catch/catch.hpp"

#include "sched.hpp"
#include "record.hpp"
#include "action.hpp"
#include "executor.hpp"

#include <memory>
#include <string>
#include <vector>

using namespace srs;

// ---------------------------------------------------------------------------
// Test double: records every execute() call for assertion
// ---------------------------------------------------------------------------
class RecordingExecutor : public IActionExecutor {
public:
    struct Event {
        std::string    name;
        RecoveryAction action;
    };

    void run(const std::string& name, RecoveryAction action) override {
        events.push_back({name, action});
    }

    std::vector<Event> events;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::shared_ptr<RecordingExecutor> make_exec() {
    return std::make_shared<RecordingExecutor>();
}

static Scheduler make_sched(std::shared_ptr<RecordingExecutor>& exec) {
    exec = make_exec();
    return Scheduler(exec);
}

// ---------------------------------------------------------------------------
// action_name
// ---------------------------------------------------------------------------
TEST_CASE("action_name returns labels", "[recovery_action]") {
    REQUIRE(action_name(RecoveryAction::Restart) == "RESTART");
    REQUIRE(action_name(RecoveryAction::Stop)    == "STOP");
    REQUIRE(action_name(RecoveryAction::Disable) == "DISABLE");
    REQUIRE(action_name(RecoveryAction::Alert)   == "ALERT");
}

// ---------------------------------------------------------------------------
// ServiceRecord
// ---------------------------------------------------------------------------
TEST_CASE("record rejects empty list", "[service_record]") {
    REQUIRE_THROWS_AS(ServiceRecord({}), std::invalid_argument);
}

TEST_CASE("record advances on failure", "[service_record]") {
    ServiceRecord rec({RecoveryAction::Restart, RecoveryAction::Stop, RecoveryAction::Disable});

    REQUIRE(rec.next_action() == RecoveryAction::Restart);
    REQUIRE(rec.get_state().failure_count == 1);
    REQUIRE(rec.get_state().level  == 1);
    REQUIRE(!rec.get_state().exhausted);

    REQUIRE(rec.next_action() == RecoveryAction::Stop);
    REQUIRE(rec.get_state().failure_count == 2);
    REQUIRE(rec.get_state().level  == 2);
    REQUIRE(!rec.get_state().exhausted);

    REQUIRE(rec.next_action() == RecoveryAction::Disable);
    REQUIRE(rec.get_state().failure_count == 3);
    REQUIRE(rec.get_state().exhausted);
}

TEST_CASE("record repeats last action", "[service_record]") {
    ServiceRecord rec({RecoveryAction::Restart, RecoveryAction::Alert});

(void)rec.next_action();  // Restart
(void)rec.next_action();  // Alert — exhausted

    // Further failures should keep returning Alert
    REQUIRE(rec.next_action() == RecoveryAction::Alert);
    REQUIRE(rec.next_action() == RecoveryAction::Alert);
    REQUIRE(rec.get_state().failure_count == 4);
    REQUIRE(rec.get_state().exhausted);
}

TEST_CASE("record reset clears state", "[service_record]") {
    ServiceRecord rec({RecoveryAction::Restart, RecoveryAction::Stop});
(void)rec.next_action();
(void)rec.next_action();

    rec.reset();

    const auto& s = rec.get_state();
    REQUIRE(s.failure_count == 0);
    REQUIRE(s.level  == 0);
    REQUIRE(!s.last_action.has_value());
    REQUIRE(!s.exhausted);
}

TEST_CASE("single action becomes exhausted", "[service_record]") {
    ServiceRecord rec({RecoveryAction::Disable});
    REQUIRE(rec.next_action() == RecoveryAction::Disable);
    REQUIRE(rec.get_state().exhausted);
    REQUIRE(rec.next_action() == RecoveryAction::Disable);  // still returns it
}

// ---------------------------------------------------------------------------
// Scheduler — registration
// ---------------------------------------------------------------------------
TEST_CASE("scheduler rejects null exec", "[scheduler]") {
    REQUIRE_THROWS_AS(Scheduler(nullptr), std::invalid_argument);
}

TEST_CASE("scheduler rejects empty name", "[scheduler]") {
    auto exec = make_exec();
    Scheduler sched(exec);
    REQUIRE_THROWS_AS(
        sched.add_service("", {RecoveryAction::Restart}),
        std::invalid_argument);
}

TEST_CASE("scheduler rejects empty sequence", "[scheduler]") {
    auto exec = make_exec();
    Scheduler sched(exec);
    REQUIRE_THROWS_AS(
        sched.add_service("svc", {}),
        std::invalid_argument);
}

TEST_CASE("scheduler rejects duplicate add", "[scheduler]") {
    auto exec = make_exec();
    Scheduler sched(exec);
    sched.add_service("svc", {RecoveryAction::Restart});
    REQUIRE_THROWS_AS(
        sched.add_service("svc", {RecoveryAction::Stop}),
        std::invalid_argument);
}

TEST_CASE("scheduler has_service reflects add", "[scheduler]") {
    auto exec = make_exec();
    Scheduler sched(exec);

    REQUIRE(!sched.has_service("svc"));
    sched.add_service("svc", {RecoveryAction::Restart});
    REQUIRE(sched.has_service("svc"));
}

// ---------------------------------------------------------------------------
// Scheduler — failure dispatching
// ---------------------------------------------------------------------------
TEST_CASE("scheduler dispatches in order", "[scheduler]") {
    std::shared_ptr<RecordingExecutor> exec;
    auto sched = make_sched(exec);

    sched.add_service("web", {
        RecoveryAction::Restart,
        RecoveryAction::Stop,
        RecoveryAction::Disable,
    });

    REQUIRE(sched.fail_service("web") == RecoveryAction::Restart);
    REQUIRE(sched.fail_service("web") == RecoveryAction::Stop);
    REQUIRE(sched.fail_service("web") == RecoveryAction::Disable);

    REQUIRE(exec->events.size() == 3);
    REQUIRE(exec->events[0].action == RecoveryAction::Restart);
    REQUIRE(exec->events[1].action == RecoveryAction::Stop);
    REQUIRE(exec->events[2].action == RecoveryAction::Disable);

    for (const auto& e : exec->events) {
        REQUIRE(e.name == "web");
    }
}

TEST_CASE("scheduler unknown service throws", "[scheduler]") {
    auto exec = make_exec();
    Scheduler sched(exec);
    REQUIRE_THROWS_AS(sched.fail_service("ghost"), std::out_of_range);
    REQUIRE(exec->events.empty());
}

TEST_CASE("scheduler get_state reflects history", "[scheduler]") {
    std::shared_ptr<RecordingExecutor> exec;
    auto sched = make_sched(exec);

    sched.add_service("db", {RecoveryAction::Restart, RecoveryAction::Alert});

    // No failures yet
    auto s0 = sched.get_state("db");
    REQUIRE(s0.failure_count == 0);
    REQUIRE(!s0.last_action.has_value());

    sched.fail_service("db");
    auto s1 = sched.get_state("db");
    REQUIRE(s1.failure_count == 1);
    REQUIRE(s1.last_action == RecoveryAction::Restart);

    sched.fail_service("db");
    auto s2 = sched.get_state("db");
    REQUIRE(s2.failure_count == 2);
    REQUIRE(s2.last_action == RecoveryAction::Alert);
    REQUIRE(s2.exhausted);
}

TEST_CASE("scheduler get_state unknown throws", "[scheduler]") {
    auto exec = make_exec();
    Scheduler sched(exec);
    REQUIRE_THROWS_AS(sched.get_state("ghost"), std::out_of_range);
}

TEST_CASE("scheduler reset restores initial", "[scheduler]") {
    std::shared_ptr<RecordingExecutor> exec;
    auto sched = make_sched(exec);

    sched.add_service("cache", {RecoveryAction::Restart, RecoveryAction::Stop});

    sched.fail_service("cache");
    sched.fail_service("cache");

    sched.reset("cache");
    const auto s = sched.get_state("cache");
    REQUIRE(s.failure_count == 0);
    REQUIRE(!s.last_action.has_value());
    REQUIRE(!s.exhausted);

    // Failures after reset should start from the beginning
    REQUIRE(sched.fail_service("cache") == RecoveryAction::Restart);
}

TEST_CASE("scheduler reset unknown throws", "[scheduler]") {
    auto exec = make_exec();
    Scheduler sched(exec);
    REQUIRE_THROWS_AS(sched.reset("ghost"), std::out_of_range);
}

TEST_CASE("scheduler for_each visits all", "[scheduler]") {
    std::shared_ptr<RecordingExecutor> exec;
    auto sched = make_sched(exec);

    sched.add_service("alpha", {RecoveryAction::Restart});
    sched.add_service("beta",  {RecoveryAction::Stop});
    sched.add_service("gamma", {RecoveryAction::Disable});

    std::vector<std::string> visited;
    sched.for_each([&](const std::string& name, const ServiceState&) {
        visited.push_back(name);
    });

    REQUIRE(visited.size() == 3);
    // Order is unspecified (unordered_map), so check membership
    auto has = [&](const std::string& n) {
        return std::find(visited.begin(), visited.end(), n) != visited.end();
    };
    REQUIRE(has("alpha"));
    REQUIRE(has("beta"));
    REQUIRE(has("gamma"));
}

TEST_CASE("scheduler handles many services", "[scheduler]") {
    std::shared_ptr<RecordingExecutor> exec;
    auto sched = make_sched(exec);

    sched.add_service("svc-a", {RecoveryAction::Restart, RecoveryAction::Stop});
    sched.add_service("svc-b", {RecoveryAction::Disable, RecoveryAction::Alert});

    sched.fail_service("svc-a");
    sched.fail_service("svc-b");
    sched.fail_service("svc-a");

    REQUIRE(sched.get_state("svc-a").failure_count == 2);
    REQUIRE(sched.get_state("svc-a").last_action   == RecoveryAction::Stop);

    REQUIRE(sched.get_state("svc-b").failure_count == 1);
    REQUIRE(sched.get_state("svc-b").last_action   == RecoveryAction::Disable);
}
