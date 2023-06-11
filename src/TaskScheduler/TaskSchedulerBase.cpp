#include <atomic>

#include "TaskSchedulerBase.h"


TaskSchedulerBase::TaskSchedulerBase(Logging * logging)
    : isNewTaskScheduled_{ false }
    , logging_{ logging == nullptr ? new Logging{ "TaskScheduler" } : logging }
{
    static std::atomic<uint64_t> id{ 1u };
    id_ = id.load();
    id.fetch_add(1u);
}

///////////////////////////////////////////////////////////////////////////////////////////////
///
/// Public ITaskScheduler methods
///
///////////////////////////////////////////////////////////////////////////////////////////////

TaskSchedulerBase::~TaskSchedulerBase()
{
    notifyTaskForExecution();
}


uint64_t TaskSchedulerBase::getId() const
{
    return id_;
}


ITaskScheduler::Statistic TaskSchedulerBase::getStatistic() const
{
    return statistic_;
}


void TaskSchedulerBase::notifyTaskForExecution() const
{
    tasksMonitor_.lock();
    isNewTaskScheduled_ = true;
    tasksMonitor_.notifyAll();
    tasksMonitor_.unlock();
}