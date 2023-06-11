#include "ThreadPoolOptionsBuilder.h"


ThreadPoolOptionsBuilder::ThreadPoolOptionsBuilder(const uint32_t initialNumberOfWorkers)
    : options_{ initialNumberOfWorkers }
{
}


ThreadPoolOptionsBuilder & ThreadPoolOptionsBuilder::setSchedulerType(const ThreadPoolOptions::SchedulerType schedulerType)
{
    options_.setSchedulerType(schedulerType);
    return *this;
}


ThreadPoolOptionsBuilder & ThreadPoolOptionsBuilder::setMinNumberOfWorkers(const uint32_t minNumberOfWorkers)
{
    options_.setMinNumberOfWorkers(minNumberOfWorkers);
    return *this;
}


ThreadPoolOptionsBuilder & ThreadPoolOptionsBuilder::setMaxNumberOfWorkers(const uint32_t maxNumberOfWorkers)
{
    options_.setMaxNumberOfWorkers(maxNumberOfWorkers);
    return *this;
}


ThreadPoolOptionsBuilder & ThreadPoolOptionsBuilder::setPostponeExecution(const bool postponeExecution)
{
    options_.setPostponeExecution(postponeExecution);
    return *this;
}


ThreadPoolOptionsBuilder & ThreadPoolOptionsBuilder::setWaitAllTasksExecutionFinished(const bool waitAllTasksExecutionFinished)
{
    options_.setWaitAllTasksExecutionFinished(waitAllTasksExecutionFinished);
    return *this;
}


ThreadPoolOptions ThreadPoolOptionsBuilder::build() const
{
    return options_;
}