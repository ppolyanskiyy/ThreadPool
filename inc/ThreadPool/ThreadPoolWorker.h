#ifndef _THREADPOOLWORKER_H_
#define _THREADPOOLWORKER_H_


#include <atomic>

#include "ITaskScheduler.h"
#include "Logging.h"


/**
 * @brief Thread pool worker.
 *        Separate thread, which is contoled by the thread pool.
 *        Main responsibility is executing thread pool tasks.
 *        Load balancing of workers is performed in thread pool.
 *        Worker uses specific scheduling algorithm for getting tasks to execute.
 *        Basically it's wrapper over ITaskScheduler, which is executing provided by ITaskScheduler tasks
 *        in separate thread. That's mean all methods calls are transffered to underlying ITaskScheduler.
 */
class ThreadPoolWorker : public OSAL::ManagedThread
{
public:

    /**
     * @param freeStateMonitor Monitor for notification about free state (means when state is WAITING).
     */
    ThreadPoolWorker(ITaskScheduler * taskScheduler, OSAL::Monitor & freeStateMonitor, Logging * logging = nullptr);

    ThreadPoolWorker(const ThreadPoolWorker &) = delete;
    ThreadPoolWorker & operator=(const ThreadPoolWorker &) = delete;

    ~ThreadPoolWorker() override;

    ITaskScheduler::Statistic getTasksStatistic() const;
    size_t getTasksSize() const;
    bool isTaskAdded(const uint64_t taskId) const;
    uint64_t getWaitingTime();

    std::shared_ptr<IThreadPoolTask> stealTask();
    Result addTask(const std::shared_ptr<IThreadPoolTask> task);
    Result addTasks(const std::vector<std::shared_ptr<IThreadPoolTask>> & tasks);
    std::shared_ptr<IThreadPoolTask> removeOneTask(const uint64_t taskId);
    std::vector<std::shared_ptr<IThreadPoolTask>> removeAllTasks();
    Result clearAllTasks();

private:

    void managedRun() override;

private:

    OSAL::Monitor &freeStateMonitor_;
    std::unique_ptr<ITaskScheduler> taskScheduler_;
    int64_t waitTaskForExecutionTimeoutInMicroseconds_;
    OSAL::Time waitingTime_;
    OSAL::Monitor waitingTimeMutex_;
};

#endif // _THREADPOOLWORKER_H_

