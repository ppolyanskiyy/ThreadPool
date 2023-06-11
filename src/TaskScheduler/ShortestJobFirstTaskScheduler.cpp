#include "ShortestJobFirstTaskScheduler.h"


ShortestJobFirstTaskScheduler::ShortestJobFirstTaskScheduler(Logging * logging)
    : PriorityOrientedTaskSchedulerBase{ logging }
{
    // Fill tasks map key values with supported burst times
    for (auto burstTime = ++BurstTime::FIRST_BURST_TIMES_POSITION;
              burstTime < BurstTime::LAST_BURST_TIMES_POSITION; ++burstTime)
    {
        burstTimeToTasksMap_.emplace(burstTime, std::deque<std::shared_ptr<IThreadPoolTask>>{});
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////
///
/// Public ITaskScheduler methods
///
///////////////////////////////////////////////////////////////////////////////////////////////

size_t ShortestJobFirstTaskScheduler::getSize() const
{
    return PriorityOrientedTaskSchedulerBase::getSize(burstTimeToTasksMap_);
}


Result ShortestJobFirstTaskScheduler::waitTaskForExecution(const int64_t timeout) const
{
    return PriorityOrientedTaskSchedulerBase::waitTaskForExecution(burstTimeToTasksMap_, timeout);
}


bool ShortestJobFirstTaskScheduler::isScheduled(const uint64_t taskId) const
{
    return PriorityOrientedTaskSchedulerBase::isScheduled(burstTimeToTasksMap_, taskId);
}


std::shared_ptr<IThreadPoolTask> ShortestJobFirstTaskScheduler::getTaskForExecution()
{
    std::shared_ptr<IThreadPoolTask> taskForExecution{};

    tasksMonitor_.lock();

    // Iterate from shortest to longest burst time
    for (auto burstTime = ++BurstTime::FIRST_BURST_TIMES_POSITION;
              burstTime < BurstTime::LAST_BURST_TIMES_POSITION; ++burstTime)
    {
        std::deque<std::shared_ptr<IThreadPoolTask>> &tasks = burstTimeToTasksMap_[burstTime];

        if (!tasks.empty())
        {
            taskForExecution = std::move(tasks.front());
            tasks.pop_front();

            ++statistic_.totalNumberOfGotForExecutionTasks;

            break;
        }
    }

    tasksMonitor_.unlock();

    return taskForExecution;
}


std::shared_ptr<IThreadPoolTask> ShortestJobFirstTaskScheduler::steal()
{
    std::shared_ptr<IThreadPoolTask> stolenTask{};

    tasksMonitor_.lock();

    // Iterate from longest to shortest burst time
    for (auto burstTime = --BurstTime::LAST_BURST_TIMES_POSITION;
              burstTime > BurstTime::FIRST_BURST_TIMES_POSITION; --burstTime)
    {
        std::deque<std::shared_ptr<IThreadPoolTask>> &tasks = burstTimeToTasksMap_[burstTime];

        if (!tasks.empty())
        {
            stolenTask = std::move(tasks.back());
            tasks.pop_back();

            ++statistic_.totalNumberOfStolenTasks;

            break;
        }
    }

    tasksMonitor_.unlock();

    return stolenTask;
}


Result ShortestJobFirstTaskScheduler::schedule(const std::shared_ptr<IThreadPoolTask> task)
{
    const BurstTimeTask *burstTimeTask = dynamic_cast<BurstTimeTask*>(task.get());
    Result result{ Result::ERROR };

    if (burstTimeTask != nullptr)
    {
        tasksMonitor_.lock();

        const BurstTime burstTime{ calculateBurstTime(burstTimeTask->getBurstTime()) };
        burstTimeToTasksMap_[burstTime].emplace_back(task);

        ++statistic_.totalNumberOfScheduledTasks;

        isNewTaskScheduled_ = true;
        tasksMonitor_.notify();
        tasksMonitor_.unlock();

        result = Result::OK;
    }
    else
    {
        logging_->logWarning("Provided task is not Burst Time Task!");
    }

    return result;
}


Result ShortestJobFirstTaskScheduler::schedule(const std::vector<std::shared_ptr<IThreadPoolTask>> & tasks)
{
    Result result{ Result::ERROR };

    if (!tasks.empty())
    {
        BurstTimeTask *burstTimeTask = nullptr;

        tasksMonitor_.lock();

        for (auto && taskIt : tasks)
        {
            burstTimeTask = dynamic_cast<BurstTimeTask*>(taskIt.get());
            if (burstTimeTask != nullptr)
            {
                const BurstTime burstTime{ calculateBurstTime(burstTimeTask->getBurstTime()) };
                burstTimeToTasksMap_[burstTime].emplace_back(taskIt);

                ++statistic_.totalNumberOfScheduledTasks;
                isNewTaskScheduled_ = true;
            }
            else
            {
                logging_->logWarning("Provided task is not Burst Time Task!");
            }
        }

        if (isNewTaskScheduled_)
        {
            tasksMonitor_.notify();
            result = Result::OK;
        }

        tasksMonitor_.unlock();
    }
    else
    {
        logging_->logWarning("Provided empty container with tasks for scheduler");
    }

    return result;
}


std::shared_ptr<IThreadPoolTask> ShortestJobFirstTaskScheduler::unscheduleOne(const uint64_t taskId)
{
    return PriorityOrientedTaskSchedulerBase::unscheduleOne(burstTimeToTasksMap_, taskId);
}


std::vector<std::shared_ptr<IThreadPoolTask>> ShortestJobFirstTaskScheduler::unscheduleAll()
{
    return PriorityOrientedTaskSchedulerBase::unscheduleAll(burstTimeToTasksMap_);
}


Result ShortestJobFirstTaskScheduler::clearAll()
{
    return PriorityOrientedTaskSchedulerBase::clearAll(burstTimeToTasksMap_);
}


BurstTime ShortestJobFirstTaskScheduler::calculateBurstTime(BurstTime burstTime) const
{
    if (BurstTime::UNDEFINED == burstTime)
    {
        burstTime = BurstTime::LONG;
    }

    return burstTime;
}