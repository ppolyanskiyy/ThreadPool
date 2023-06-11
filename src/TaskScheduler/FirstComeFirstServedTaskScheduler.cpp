#include "FirstComeFirstServedTaskScheduler.h"


FirstComeFirstServedTaskScheduler::FirstComeFirstServedTaskScheduler(Logging * logging)
    : TaskSchedulerBase{ logging }
{
}

///////////////////////////////////////////////////////////////////////////////////////////////
///
/// Public ITaskScheduler methods
///
///////////////////////////////////////////////////////////////////////////////////////////////

size_t FirstComeFirstServedTaskScheduler::getSize() const
{
    tasksMonitor_.lock();
    const size_t size{ tasks_.size() };
    tasksMonitor_.unlock();

    return size;
}


Result FirstComeFirstServedTaskScheduler::waitTaskForExecution(const int64_t timeout) const
{
    Result result{ Result::OK };

    tasksMonitor_.lock();

    if (tasks_.empty())
    {
        isNewTaskScheduled_ = false;
        OSAL::Timeout waitTimeout{ timeout };

        while (Result::OK == result && !isNewTaskScheduled_)
        {
            result = tasksMonitor_.wait(waitTimeout.getRemainingTime());
        }
    }

    tasksMonitor_.unlock();

    return result;
}


bool FirstComeFirstServedTaskScheduler::isScheduled(const uint64_t taskId) const
{
    bool isScheduled{ false };

    tasksMonitor_.lock();

    const auto foundTaskIt = TaskSchedulerBase::findTaskById(tasks_.cbegin(), tasks_.cend(), taskId);
    if (foundTaskIt != tasks_.cend())
    {
        isScheduled = true;
    }

    tasksMonitor_.unlock();

    return isScheduled;
}


std::shared_ptr<IThreadPoolTask> FirstComeFirstServedTaskScheduler::getTaskForExecution()
{
    std::shared_ptr<IThreadPoolTask> taskForExecution{};

    tasksMonitor_.lock();

    if (!tasks_.empty())
    {
        taskForExecution = std::move(tasks_.front());
        tasks_.pop_front();

        ++statistic_.totalNumberOfGotForExecutionTasks;
    }

    tasksMonitor_.unlock();

    return taskForExecution;
}


std::shared_ptr<IThreadPoolTask> FirstComeFirstServedTaskScheduler::steal()
{
    std::shared_ptr<IThreadPoolTask> stolenTask{};

    tasksMonitor_.lock();

    if (!tasks_.empty())
    {
        stolenTask = std::move(tasks_.back());
        tasks_.pop_back();

        ++statistic_.totalNumberOfStolenTasks;
    }

    tasksMonitor_.unlock();

    return stolenTask;
}


Result FirstComeFirstServedTaskScheduler::schedule(const std::shared_ptr<IThreadPoolTask> task)
{
    Result result{ Result::ERROR };

    if (task != nullptr)
    {
        tasksMonitor_.lock();

        tasks_.emplace_back(task);

        ++statistic_.totalNumberOfScheduledTasks;

        isNewTaskScheduled_ = true;
        tasksMonitor_.notify();
        tasksMonitor_.unlock();

        result = Result::OK;
    }

    return result;
}


Result FirstComeFirstServedTaskScheduler::schedule(const std::vector<std::shared_ptr<IThreadPoolTask>> & tasks)
{
    Result result{ Result::ERROR };

    if (!tasks.empty())
    {
        tasksMonitor_.lock();

        for (auto && taskIt : tasks)
        {
            if (taskIt != nullptr)
            {
                tasks_.emplace_back(taskIt);

                ++statistic_.totalNumberOfScheduledTasks;
                isNewTaskScheduled_ = true;
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


std::shared_ptr<IThreadPoolTask> FirstComeFirstServedTaskScheduler::unscheduleOne(const uint64_t taskId)
{
    std::shared_ptr<IThreadPoolTask> unscheduledTask{};

    tasksMonitor_.lock();

    if (!tasks_.empty())
    {
        const auto foundTaskIt = TaskSchedulerBase::findTaskById(tasks_.cbegin(), tasks_.cend(), taskId);

       if (foundTaskIt != tasks_.cend())
       {
           unscheduledTask = std::move(*foundTaskIt);
           tasks_.erase(foundTaskIt);

           ++statistic_.totalNumberOfUnscheduledTasks;
       }
    }

    tasksMonitor_.unlock();

    return unscheduledTask;
}


std::vector<std::shared_ptr<IThreadPoolTask>> FirstComeFirstServedTaskScheduler::unscheduleAll()
{
    std::vector<std::shared_ptr<IThreadPoolTask>> unscheduledTasks{};

    tasksMonitor_.lock();

    if (!tasks_.empty())
    {
        unscheduledTasks.insert(unscheduledTasks.end(),
            std::make_move_iterator(tasks_.begin()),
            std::make_move_iterator(tasks_.end()));

        statistic_.totalNumberOfUnscheduledTasks += static_cast<uint32_t>(tasks_.size());

        tasks_.clear();
    }

    tasksMonitor_.unlock();

    return unscheduledTasks;
}


Result FirstComeFirstServedTaskScheduler::clearAll()
{
    Result result{ Result::ERROR };

    tasksMonitor_.lock();

    if (!tasks_.empty())
    {
        statistic_.totalNumberOfUnscheduledTasks += static_cast<uint32_t>(tasks_.size());
        tasks_.clear();

        result = Result::OK;
    }

    tasksMonitor_.unlock();

    return result;
}