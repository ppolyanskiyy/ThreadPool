#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_


#include "IThreadPool.h"
#include "ThreadPoolWorker.h"


/**
 * @brief Thread pool default implementation.
 *        Main responsibilities is load balancing incoming tasks among worker threads.
 *        Load balancing is executing in the separate thread automatically.
 *        If you want to change load balancing algorithm just inherit from this class and implement own loadBalance and getAvailableWorker methods.
 */
class ThreadPool : public IThreadPool
                 , private OSAL::ManagedThread
{
public:

    using WorkersContainer = std::deque<std::shared_ptr<ThreadPoolWorker>>;

public:

    explicit ThreadPool(const ThreadPoolOptions & options, Logging * logging = nullptr);

    ThreadPool(const ThreadPool &) = delete;
    ThreadPool & operator=(const ThreadPool &) = delete;

    ThreadPool(const ThreadPool &&) = delete;
    ThreadPool & operator=(const ThreadPool &&) = delete;

    ~ThreadPool() override;

public:

    uint64_t getId() const override;
    IThreadPool::State getState() const override;
    Statistic getStatistic() const override;
    ThreadPoolOptions getOptions() const override;
    size_t getTasksSize(const bool needsGetFromWorkers = true) const override;
    size_t getWorkersSize() const override;
    bool isTaskAdded(const uint64_t taskId) const override;

    Result waitAllTasksExecutionFinished(const int64_t timeout = -1) override final;

    Result addTask(const std::shared_ptr<IThreadPoolTask> task) override;
    Result addTasks(const std::vector<std::shared_ptr<IThreadPoolTask>> & tasks) override;
    Result addTaskToEveryWorker(const std::vector<std::shared_ptr<IThreadPoolTask>> & tasks) override;
    std::shared_ptr<IThreadPoolTask> removeOneTask(const uint64_t taskId) override;
    std::vector<std::shared_ptr<IThreadPoolTask>> removeAllTasks(const bool needsRemoveFromWorkers = true) override;
    Result clearAllTasks(const bool needsClearFromWorkers = true) override;

    Result increaseWorkers(const uint32_t number = 1u) override;
    Result decreaseWorkers(const uint32_t number = 1u, const bool needsRescheduleTasks = true) override;

    Result startExecution() override final;
    Result pauseExecution() override final;
    Result resumeExecution() override final;

protected:

    virtual void loadBalance();
    virtual WorkersContainer::value_type getAvailableWorker();
    virtual std::shared_ptr<IThreadPoolTask> getTaskForExecution();

    /**
     * @note It must be called before thread pool destruction.
     */
    virtual void waitFinished(const int64_t timeout = -1);

protected:

    void managedRun() override final;

protected:

    ITaskScheduler* getNewTaskScheduler(const ThreadPoolOptions::SchedulerType schedulerType) const;

    Result createManagingThread();
    Result createWorkerThreads();
    Result stopWorkerThreadsExecution();
    Result pauseWorkerThreadsExecution();
    Result resumeWorkerThreadsExecution();

    void eraseWorkersAndRescheduleTasks(const WorkersContainer::iterator begin, const WorkersContainer::iterator end, const bool needsRescheduleTasks = true);
    Result increaseWorkersInternal(const uint32_t number = 1u);
    Result decreaseWorkersInternal(const uint32_t number = 1u, const bool needsRescheduleTasks = true);
    uint32_t getNumberOfWorkersToIncrease(const uint32_t initialNumber) const;
    uint32_t getNumberOfWorkersToDecrease(const uint32_t initialNumber) const;
    size_t getTasksSizeFromAllWorkers() const;

protected:

    ThreadPoolOptions options_;
    mutable IThreadPool::State state_;
    mutable IThreadPool::Statistic statistic_;
    std::unique_ptr<ITaskScheduler> taskScheduler_;
    mutable OSAL::Monitor tasksExecutionMonitor_;
    WorkersContainer workers_;
    mutable OSAL::Mutex workersMutex_;
    std::unique_ptr<Logging> logging_;

private:

    uint64_t id_;
    int64_t waitForNewTaskOrWorkerAvailabilityTimeoutInMicroseconds_;
    std::shared_ptr<IThreadPoolTask> currentTaskForExecution_;
    bool needsGetNewTaskForExecution_;
    bool areAllTasksPutForExecution_;
};

#endif // _THREADPOOL_H_

