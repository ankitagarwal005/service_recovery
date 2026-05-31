#include "sched.hpp"

#include <stdexcept>
#include <sstream>

namespace srs {

Scheduler::Scheduler(std::shared_ptr<IActionExecutor> executor)
    : executor_(std::move(executor))
{
    if (!executor_) {
        throw std::invalid_argument("Executor must not be null");
    }
}

void Scheduler::add_service(const std::string& name,
                            std::vector<RecoveryAction> sequence)
{
    if (name.empty()) {
        throw std::invalid_argument("Service name must not be empty");
    }
    if (sequence.empty()) {
        throw std::invalid_argument("Recovery sequence must not be empty");
    }
    if (services_.count(name)) {
        throw std::invalid_argument("Service already registered: " + name);
    }
    services_.emplace(name, std::move(sequence));
}

RecoveryAction Scheduler::fail_service(const std::string& name)
{
    auto it = services_.find(name);
    if (it == services_.end()) {
        throw std::out_of_range("Unknown service: " + name);
    }

    const RecoveryAction action = it->second.next_action();
    executor_->run(name, action);
    return action;
}

void Scheduler::reset(const std::string& name)
{
    auto it = services_.find(name);
    if (it == services_.end()) {
        throw std::out_of_range("Unknown service: " + name);
    }
    it->second.reset();
}

ServiceState Scheduler::get_state(const std::string& name) const
{
    auto it = services_.find(name);
    if (it == services_.end()) {
        throw std::out_of_range("Unknown service: " + name);
    }
    return it->second.get_state();
}

bool Scheduler::has_service(const std::string& name) const noexcept
{
    return services_.count(name) > 0;
}

void Scheduler::for_each(
    const std::function<void(const std::string&, const ServiceState&)>& fn) const
{
    for (const auto& [name, record] : services_) {
        fn(name, record.get_state());
    }
}

}  // namespace srs
