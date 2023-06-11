#include <thread>

#include "ThreadPoolOptions.h"


ThreadPoolOptions::ThreadPoolOptions(const SchedulerType schedulerType,
                                     const uint32_t initialNumberOfWorkers, const uint32_t minNumberOfWorkers, const uint32_t maxNumberOfWorkers,
                                     const bool needsPostponeExecution, const bool needsWaitAllTasksExecutionFinished)
    : schedulerType_{ schedulerType }
    , initialNumberOfWorkers_{ initialNumberOfWorkers }
    , needsPostponeExecution_{ needsPostponeExecution }
    , needsWaitAllTasksExecutionFinished_{ needsWaitAllTasksExecutionFinished }
{
    // Set min number of workers.
    setMinNumberOfWorkers(minNumberOfWorkers);
    setMaxNumberOfWorkers(maxNumberOfWorkers);
}


ThreadPoolOptions::ThreadPoolOptions(const uint32_t initialNumberOfWorkers, const uint32_t minNumberOfWorkers, const uint32_t maxNumberOfWorkers,
                                     const bool needsPostponeExecution, const bool needsWaitAllTasksExecutionFinished)
    : ThreadPoolOptions{ SchedulerType::FCFS, initialNumberOfWorkers, minNumberOfWorkers, maxNumberOfWorkers,
                         needsPostponeExecution, needsWaitAllTasksExecutionFinished }
{
}


ThreadPoolOptions::ThreadPoolOptions(const uint32_t initialNumberOfWorkers, const bool needsPostponeExecution, const bool needsWaitAllTasksExecutionFinished)
    : ThreadPoolOptions{ SchedulerType::FCFS, initialNumberOfWorkers, 0u, 0u,
                         needsPostponeExecution, needsWaitAllTasksExecutionFinished }
{
}


ThreadPoolOptions::ThreadPoolOptions(const bool needsPostponeExecution, const bool needsWaitAllTasksExecutionFinished)
    : ThreadPoolOptions{ SchedulerType::FCFS, std::thread::hardware_concurrency(), 0u, 0u,
                         needsPostponeExecution, needsWaitAllTasksExecutionFinished }
{
}


ThreadPoolOptions::SchedulerType ThreadPoolOptions::getSchedulerType() const
{
    return schedulerType_;
}


void ThreadPoolOptions::setSchedulerType(const SchedulerType schedulerType)
{
    schedulerType_ = schedulerType;
}


uint32_t ThreadPoolOptions::getMinNumberOfWorkers() const
{
    return minNumberOfWorkers_;
}


void ThreadPoolOptions::setMinNumberOfWorkers(const uint32_t value)
{
    if (value > initialNumberOfWorkers_)
        minNumberOfWorkers_ = initialNumberOfWorkers_;
    else
        minNumberOfWorkers_ = value;
}


uint32_t ThreadPoolOptions::getMaxNumberOfWorkers() const
{
    return maxNumberOfWorkers_;
}


void ThreadPoolOptions::setMaxNumberOfWorkers(const uint32_t value)
{
    if (value < initialNumberOfWorkers_)
        maxNumberOfWorkers_ = initialNumberOfWorkers_;
    else
        maxNumberOfWorkers_ = value;
}


uint32_t ThreadPoolOptions::getInitialNumberOfWorkers() const
{
    return initialNumberOfWorkers_;
}


void ThreadPoolOptions::setInitialNumberOfWorkers(const uint32_t value)
{
    if (value < minNumberOfWorkers_)
        initialNumberOfWorkers_ = minNumberOfWorkers_;
    else if (value > maxNumberOfWorkers_)
        initialNumberOfWorkers_ = maxNumberOfWorkers_;
    else
        initialNumberOfWorkers_ = value;
}


bool ThreadPoolOptions::needsPostponeExecution() const
{
    return needsPostponeExecution_;
}


void ThreadPoolOptions::setPostponeExecution(const bool needsPostponeExecution)
{
    needsPostponeExecution_ = needsPostponeExecution;
}


bool ThreadPoolOptions::needsWaitAllTasksExecutionFinished() const
{
    return needsWaitAllTasksExecutionFinished_;
}


void ThreadPoolOptions::setWaitAllTasksExecutionFinished(const bool needsWaitAllTasksExecutionFinished)
{
    needsWaitAllTasksExecutionFinished_ = needsWaitAllTasksExecutionFinished;
}


std::string ThreadPoolOptions::toString() const
{
    return "Scheduler type: "                   + ThreadPoolOptions::schedulerTypeToString(schedulerType_)
         + "\nInitial number of workers : "     + std::to_string(initialNumberOfWorkers_)
         + "\nMin number of workers : "         + std::to_string(minNumberOfWorkers_)
         + "\nMax number of workers : "         + std::to_string(maxNumberOfWorkers_)
         + "\nNeeds to postpone execution : "   + (needsPostponeExecution_ ? "true" : "false");
}