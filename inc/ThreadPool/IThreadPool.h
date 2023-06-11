#ifndef _ITHREADPOOL_H_
#define _ITHREADPOOL_H_


#include "ITaskScheduler.h"
#include "ThreadPoolOptions.h"


class IThreadPool
{
public:

    struct Statistic
    {
        uint32_t currentNumberOfAllWorkers{ 0u };

        uint32_t numberOfWorkersInReadyState{ 0u };
        uint32_t numberOfWorkersInRunningState{ 0u };
        uint32_t numberOfWorkersInWaitingState{ 0u };
        uint32_t numberOfWorkersInPausedState{ 0u };

        uint32_t totalNumberOfAddedTasks{ 0u };

    public:

        inline std::string toString() const
        {
            return "Current number of all workers : "   + std::to_string(currentNumberOfAllWorkers)
                 + "\nWorkers in READY state : "        + std::to_string(numberOfWorkersInReadyState)
                 + "\nWorkers in RUNNING state : "      + std::to_string(numberOfWorkersInRunningState)
                 + "\nWorkers in WAITING state : "      + std::to_string(numberOfWorkersInWaitingState)
                 + "\nWorkers in PAUSED state : "       + std::to_string(numberOfWorkersInPausedState)
                 + "\nTotal number of added tasks : "   + std::to_string(totalNumberOfAddedTasks);
        }
    };

    enum class State : uint8_t
    {
        READY,          ///< Ready state. When thread pool is created and waiting to start execution.
        RUNNING,        ///< Running state. When thread pool is executing some task.
        PAUSED,         ///< Paused state. When thread pool is paused by user.
        UNDEFINED       ///< Undefined state.
    };

    static std::string stateToString(const State state)
    {
        switch (state)
        {
            case State::READY:      return "READY";
            case State::RUNNING:    return "RUNNING";
            case State::PAUSED:     return "PAUSED";
            default:                return "UNDEFINED";
        }
    };

    static State stringToState(const std::string & state)
    {
        std::string upperCaseState;
        std::transform(state.begin(), state.end(), upperCaseState.begin(), [](const std::string::value_type c) { return std::toupper(c); });

        if ("READY" == upperCaseState)      return State::READY;
        if ("RUNNING" == upperCaseState)    return State::RUNNING;
        if ("PAUSED" == upperCaseState)     return State::PAUSED;

        return State::UNDEFINED;
    };

    virtual ~IThreadPool () = default;

    virtual uint64_t getId() const = 0;
    virtual State getState() const = 0;
    virtual Statistic getStatistic() const = 0;
    virtual ThreadPoolOptions getOptions() const = 0;
    virtual size_t getTasksSize(const bool needsGetFromWorkers = true) const = 0;
    virtual size_t getWorkersSize() const = 0;
    virtual bool isTaskAdded(const uint64_t taskId) const = 0;

    virtual Result waitAllTasksExecutionFinished(const int64_t timeout = -1) = 0;
    virtual Result addTask(const std::shared_ptr<IThreadPoolTask> task) = 0;
    virtual Result addTasks(const std::vector<std::shared_ptr<IThreadPoolTask>> & tasks) = 0;
    virtual Result addTaskToEveryWorker(const std::vector<std::shared_ptr<IThreadPoolTask>> & tasks) = 0;
    virtual std::shared_ptr<IThreadPoolTask> removeOneTask(const uint64_t taskId) = 0;
    virtual std::vector<std::shared_ptr<IThreadPoolTask>> removeAllTasks(const bool needsRemoveFromWorkers = true) = 0;
    virtual Result clearAllTasks(const bool needsClearFromWorkers = true) = 0;
    virtual Result increaseWorkers(const uint32_t number = 1u) = 0;
    virtual Result decreaseWorkers(const uint32_t number = 1u, const bool needsRescheduleTasks = true) = 0;

    virtual Result startExecution() = 0;
    virtual Result pauseExecution() = 0;
    virtual Result resumeExecution() = 0;
};

#endif // _ITHREADPOOL_H_

