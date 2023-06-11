#ifndef _PRIORITYORIENTEDTASKSCHEDULERBASE_H_
#define _PRIORITYORIENTEDTASKSCHEDULERBASE_H_


#include "TaskSchedulerBase.h"


class PriorityOrientedTaskSchedulerBase : public TaskSchedulerBase
{
protected:

    explicit PriorityOrientedTaskSchedulerBase(Logging * logging  = nullptr) : TaskSchedulerBase{ logging } { }

    template<typename Key, template <typename, typename...> class Container, typename Value, typename... Parameters>
    size_t getSize(const std::unordered_map<Key, Container<Value, Parameters...>> & priorityToTasksMap) const;

    template<typename Key, template <typename, typename...> class Container, typename Value, typename... Parameters>
    Result waitTaskForExecution(const std::unordered_map<Key, Container<Value, Parameters...>> & priorityToTasksMap, const int64_t timeout) const;

    template<typename Key, template <typename, typename...> class Container, typename... Parameters>
    bool isScheduled(const std::unordered_map<Key, Container<std::shared_ptr<IThreadPoolTask>, Parameters...>> & priorityToTasksMap,
                     const uint64_t taskId) const;

    template<typename Key, template <typename, typename...> class Container, typename... Parameters>
    std::shared_ptr<IThreadPoolTask> unscheduleOne(
        std::unordered_map<Key, Container<std::shared_ptr<IThreadPoolTask>, Parameters...>> & priorityToTasksMap, const uint64_t taskId);

    /**
     * @note Order of priorities is random.
     */
    template<typename Key, template <typename, typename...> class Container, typename... Parameters>
    std::vector<std::shared_ptr<IThreadPoolTask>> unscheduleAll(
        std::unordered_map<Key, Container<std::shared_ptr<IThreadPoolTask>, Parameters...>> & priorityToTasksMap);

    template<typename Key, template <typename, typename...> class Container, typename... Parameters>
    Result clearAll(std::unordered_map<Key, Container<std::shared_ptr<IThreadPoolTask>, Parameters...>> & priorityToTasksMap);
};




template<typename Key, template <typename, typename...> class Container, typename Value, typename... Parameters>
size_t PriorityOrientedTaskSchedulerBase::getSize(const std::unordered_map<Key, Container<Value, Parameters...>> & priorityToTasksMap) const
{
    size_t size{ 0 };

    tasksMonitor_.lock();

    // Suppressing cppcheck warning suggesting usage of std::accumulate due to performance
    for (const auto & priorityToTasksIt : priorityToTasksMap)
    {
        // cppcheck-suppress useStlAlgorithm
        size += priorityToTasksIt.second.size();
    }

    tasksMonitor_.unlock();

    return size;
}


template<typename Key, template <typename, typename...> class Container, typename Value, typename... Parameters>
Result PriorityOrientedTaskSchedulerBase::waitTaskForExecution(const std::unordered_map<Key, Container<Value, Parameters...>> & priorityToTasksMap,
                                                               const int64_t timeout) const
{
    Result result{ Result::OK };
    bool isTasksEmpty{ true };

    tasksMonitor_.lock();

    for (const auto & priorityToTasksIt : priorityToTasksMap)
    {
        isTasksEmpty &= priorityToTasksIt.second.empty();
    }

    if (isTasksEmpty)
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


template<typename Key, template <typename, typename...> class Container, typename... Parameters>
bool PriorityOrientedTaskSchedulerBase::isScheduled(const std::unordered_map<Key, Container<std::shared_ptr<IThreadPoolTask>, Parameters...>> & priorityToTasksMap,
                                                    const uint64_t taskId) const
{
    bool isScheduled{ false };

    tasksMonitor_.lock();

    // Iterate over priority containers
    for (auto && priorityToTasksIt : priorityToTasksMap)
    {
        // Try to find task with given id in current priority container
        const auto foundTaskIt = TaskSchedulerBase::findTaskById(priorityToTasksIt.second.cbegin(), priorityToTasksIt.second.cend(), taskId);
        if (foundTaskIt != priorityToTasksIt.second.cend())
        {
            isScheduled = true;
            break;
        }
    }

    tasksMonitor_.unlock();

    return isScheduled;
}


template<typename Key, template <typename, typename...> class Container, typename... Parameters>
std::shared_ptr<IThreadPoolTask> PriorityOrientedTaskSchedulerBase::unscheduleOne(
                                        std::unordered_map<Key, Container<std::shared_ptr<IThreadPoolTask>, Parameters...>> & priorityToTasksMap,
                                        const uint64_t taskId)
{
    std::shared_ptr<IThreadPoolTask> unscheduledTask{};

    tasksMonitor_.lock();

    for (auto && priorityToTasksIt : priorityToTasksMap)
    {
        const auto foundTaskIt = findTaskById(priorityToTasksIt.second.cbegin(), priorityToTasksIt.second.cend(), taskId);
        if (foundTaskIt != priorityToTasksIt.second.cend())
        {
            unscheduledTask = std::move(*foundTaskIt);
            priorityToTasksIt.second.erase(foundTaskIt);

            statistic_.totalNumberOfUnscheduledTasks++;

            break;
        }
    }

    tasksMonitor_.unlock();

    return unscheduledTask;
}


template<typename Key, template <typename, typename...> class Container, typename... Parameters>
std::vector<std::shared_ptr<IThreadPoolTask>> PriorityOrientedTaskSchedulerBase::unscheduleAll(
                                                 std::unordered_map<Key, Container<std::shared_ptr<IThreadPoolTask>, Parameters...>> & priorityToTasksMap)
{
    std::vector<std::shared_ptr<IThreadPoolTask>> unscheduledTasks{};

    tasksMonitor_.lock();

    for (auto && priorityToTasksIt : priorityToTasksMap)
    {
        if (!priorityToTasksIt.second.empty())
        {
            unscheduledTasks.insert(unscheduledTasks.end(),
                    std::make_move_iterator(priorityToTasksIt.second.begin()),
                    std::make_move_iterator(priorityToTasksIt.second.end()));

            statistic_.totalNumberOfUnscheduledTasks += static_cast<uint32_t>(priorityToTasksIt.second.size());

            priorityToTasksIt.second.clear();
        }
    }

    tasksMonitor_.unlock();

    return unscheduledTasks;
}


template<typename Key, template <typename, typename...> class Container, typename... Parameters>
Result PriorityOrientedTaskSchedulerBase::clearAll(std::unordered_map<Key, Container<std::shared_ptr<IThreadPoolTask>, Parameters...>> & priorityToTasksMap)
{
    bool isAllEmpty{ true };

    tasksMonitor_.lock();

    for (auto && priorityToTasksIt : priorityToTasksMap)
    {
        if (!priorityToTasksIt.second.empty())
        {
            statistic_.totalNumberOfUnscheduledTasks += static_cast<uint32_t>(priorityToTasksIt.second.size());

            priorityToTasksIt.second.clear();

            isAllEmpty = false;
        }
    }

    tasksMonitor_.unlock();

    return isAllEmpty ? Result::ERROR : Result::OK;
}

#endif // _PRIORITYORIENTEDTASKSCHEDULERBASE_H_

