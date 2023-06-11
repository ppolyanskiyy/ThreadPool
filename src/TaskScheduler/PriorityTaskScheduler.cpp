#include "PriorityTaskScheduler.h"


PriorityTaskScheduler::PriorityTaskScheduler(Logging * logging)
    : PriorityOrientedTaskSchedulerBase{ logging }
{
    // Fill tasks map key values with supported priorities
    for (auto priority = ++Priority::FIRST_PRIORITIES_POSITION;
              priority < Priority::LAST_PRIORITIES_POSITION; ++priority)
    {
        priorityToTasksMap_.emplace(priority, std::deque<std::shared_ptr<IThreadPoolTask>>{});
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////
///
/// Public ITaskScheduler methods
///
///////////////////////////////////////////////////////////////////////////////////////////////

size_t PriorityTaskScheduler::getSize() const
{
   return PriorityOrientedTaskSchedulerBase::getSize(priorityToTasksMap_);
}


Result PriorityTaskScheduler::waitTaskForExecution(const int64_t timeout) const
{
    return PriorityOrientedTaskSchedulerBase::waitTaskForExecution(priorityToTasksMap_, timeout);
}


bool PriorityTaskScheduler::isScheduled(const uint64_t taskId) const
{
   return PriorityOrientedTaskSchedulerBase::isScheduled(priorityToTasksMap_, taskId);
}


std::shared_ptr<IThreadPoolTask> PriorityTaskScheduler::getTaskForExecution()
{
    std::shared_ptr<IThreadPoolTask> taskForExecution{};

    tasksMonitor_.lock();

    // Iterate from highest to lowest priority
    for (auto priority = ++Priority::FIRST_PRIORITIES_POSITION;
              priority < Priority::LAST_PRIORITIES_POSITION; ++priority)
    {
        std::deque<std::shared_ptr<IThreadPoolTask>> &tasks = priorityToTasksMap_[priority];

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


std::shared_ptr<IThreadPoolTask> PriorityTaskScheduler::steal()
{
    std::shared_ptr<IThreadPoolTask> stolenTask{};

    tasksMonitor_.lock();

    // Iterate from lowest to highest priority
    for (auto priority = --Priority::LAST_PRIORITIES_POSITION;
              priority > Priority::FIRST_PRIORITIES_POSITION; --priority)
    {
        std::deque<std::shared_ptr<IThreadPoolTask>> &tasks = priorityToTasksMap_[priority];

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


Result PriorityTaskScheduler::schedule(const std::shared_ptr<IThreadPoolTask> task)
{
    const PriorityTask *priorityTask = dynamic_cast<PriorityTask*>(task.get());
    Result result{ Result::ERROR };

    if (priorityTask != nullptr)
    {
        tasksMonitor_.lock();

        priorityToTasksMap_[priorityTask->getPriority()].emplace_back(task);

        ++statistic_.totalNumberOfScheduledTasks;

        isNewTaskScheduled_ = true;
        tasksMonitor_.notify();
        tasksMonitor_.unlock();

        result = Result::OK;
    }
    else
    {
        logging_->logWarning("Provided task is not Priority Task!");
    }

    return result;
}


Result PriorityTaskScheduler::schedule(const std::vector<std::shared_ptr<IThreadPoolTask>> & tasks)
{
    Result result{ Result::ERROR };

    if (!tasks.empty())
    {
        PriorityTask *priorityTask = nullptr;

        tasksMonitor_.lock();

        for (auto && taskIt : tasks)
        {
            priorityTask = dynamic_cast<PriorityTask*>(taskIt.get());
            if (priorityTask != nullptr)
            {
                priorityToTasksMap_[priorityTask->getPriority()].emplace_back(taskIt);

                ++statistic_.totalNumberOfScheduledTasks;
                isNewTaskScheduled_ = true;
            }
            else
            {
                logging_->logWarning("Provided task is not Priority Task!");
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


std::shared_ptr<IThreadPoolTask> PriorityTaskScheduler::unscheduleOne(const uint64_t taskId)
{
    return PriorityOrientedTaskSchedulerBase::unscheduleOne(priorityToTasksMap_, taskId);
}


std::vector<std::shared_ptr<IThreadPoolTask>> PriorityTaskScheduler::unscheduleAll()
{
    return PriorityOrientedTaskSchedulerBase::unscheduleAll(priorityToTasksMap_);
}


Result PriorityTaskScheduler::clearAll()
{
   return PriorityOrientedTaskSchedulerBase::clearAll(priorityToTasksMap_);
}