#include "FirstComeFirstServedTaskScheduler.h"
#include "PriorityTaskScheduler.h"
#include "ShortestJobFirstTaskScheduler.h"
#include "ThreadPool.h"
#include "Logging.h"


///////////////////////////////////////////////////////////////////////////////////////////////
///
/// Public IThreadPool methods
///
///////////////////////////////////////////////////////////////////////////////////////////////

uint64_t ThreadPool::getId() const
{
    return id_;
}


IThreadPool::State ThreadPool::getState() const
{
    return state_;
}


IThreadPool::Statistic ThreadPool::getStatistic() const
{
    logging_->logDebug("%" PRIu64 " is requested to get statistic", id_);

    uint32_t numberOfWorkersInReadyState{ 0u };
    uint32_t numberOfWorkersInRunningState{ 0u };
    uint32_t numberOfWorkersInWaitingState{ 0u };
    uint32_t numberOfWorkersInPausedState{ 0u };

    workersMutex_.lock();

    for (auto && workerIt : workers_)
    {
        switch (workerIt->getState())
        {
            case ThreadPoolWorker::State::READY:
                ++numberOfWorkersInReadyState;
                break;
            case ThreadPoolWorker::State::RUNNING:
                ++numberOfWorkersInRunningState;
                break;
            case ThreadPoolWorker::State::WAITING:
                ++numberOfWorkersInWaitingState;
                break;
            case ThreadPoolWorker::State::PAUSED:
                ++numberOfWorkersInPausedState;
            default:
                break;
        }
    }

    statistic_.currentNumberOfAllWorkers        = static_cast<uint32_t>(workers_.size());
    statistic_.numberOfWorkersInReadyState      = numberOfWorkersInReadyState;
    statistic_.numberOfWorkersInRunningState    = numberOfWorkersInRunningState;
    statistic_.numberOfWorkersInWaitingState    = numberOfWorkersInWaitingState;
    statistic_.numberOfWorkersInPausedState     = numberOfWorkersInPausedState;

    workersMutex_.unlock();

    logging_->logDebug("%" PRIu64 " statistic:\n%s", id_, statistic_.toString().c_str());

    return statistic_;
}


ThreadPoolOptions ThreadPool::getOptions() const
{
    return options_;
}


size_t ThreadPool::getTasksSize(const bool needsGetFromWorkers) const
{
    size_t tasksSize{ 0u };

    if (needsGetFromWorkers)
    {
        workersMutex_.lock();
        tasksSize += getTasksSizeFromAllWorkers();
        workersMutex_.unlock();
    }

    tasksSize += taskScheduler_->getSize();

    return tasksSize;
}


size_t ThreadPool::getWorkersSize() const
{
    workersMutex_.lock();
    const size_t size{ static_cast<size_t>(workers_.size()) };
    workersMutex_.unlock();

    return size;
}


bool ThreadPool::isTaskAdded(const uint64_t taskId) const
{
    return taskScheduler_->isScheduled(taskId);
}


Result ThreadPool::waitAllTasksExecutionFinished(const int64_t timeout)
{
    logging_->logDebug("%" PRIu64 " is requested to wait for all tasks execution finished", id_);

    Result result{ Result::OK };
    OSAL::Timeout waitTimeout{ timeout };

    logging_->logDebug("%" PRIu64 " is waiting for all task being got for execution...", id_);

    tasksExecutionMonitor_.lock();

    // Wait for all tasks from scheduler become added to workers and workers finish execution
    while (Result::OK == result && !areAllTasksPutForExecution_ && taskScheduler_->getSize() != 0u)
    {
        result = tasksExecutionMonitor_.wait(waitTimeout.getRemainingTime());
    }

    workersMutex_.lock();
    size_t tasksSizeInAllWorkers{ getTasksSizeFromAllWorkers() };
    workersMutex_.unlock();

    logging_->logDebug("%" PRIu64 " is waiting for all workers to finish tasks execution...", id_);

    // Wait for workers to finish execution (workers use same monitor for notification about free state)
    while (Result::OK == result && tasksSizeInAllWorkers != 0u)
    {
        result = tasksExecutionMonitor_.wait(waitTimeout.getRemainingTime());

        workersMutex_.lock();
        tasksSizeInAllWorkers = getTasksSizeFromAllWorkers();
        workersMutex_.unlock();
    }

    tasksExecutionMonitor_.unlock();

    logging_->logDebug("%" PRIu64 " finish waiting for all tasks execution finished with result %s", id_, resultToStr(result).c_str());

    return result;
}


Result ThreadPool::addTask(const std::shared_ptr<IThreadPoolTask> task)
{
    Result result{ Result::ERROR };

    if (task != nullptr)
    {
        tasksExecutionMonitor_.lock();

        result = taskScheduler_->schedule(task);
        areAllTasksPutForExecution_ = false;

        ++statistic_.totalNumberOfAddedTasks;

        tasksExecutionMonitor_.notifyAll();
        tasksExecutionMonitor_.unlock();

        logging_->logDebug("%" PRIu64 " add task with id %" PRIu64, id_, task->getId());
    }
    else
    {
        logging_->logDebug("%" PRIu64 " can't add nullptr task", id_);
    }

    return result;
}


Result ThreadPool::addTasks(const std::vector<std::shared_ptr<IThreadPoolTask>> & tasks) // TODO: Cover with tests
{
    logging_->logDebug("%" PRIu64 " is requested to add %" PRIu32 " tasks", id_, static_cast<uint32_t>(tasks.size()));

    Result result{ Result::ERROR };

    if (!tasks.empty())
    {
        uint32_t addedTasksCount{ 0u };

        tasksExecutionMonitor_.lock();

        for (auto && taskIt : tasks)
        {
            if (taskIt != nullptr)
            {
                result += taskScheduler_->schedule(taskIt);
                ++addedTasksCount;
            }
        }

        if (addedTasksCount > 0u)
        {
            statistic_.totalNumberOfAddedTasks += addedTasksCount;
            areAllTasksPutForExecution_ = false;

            tasksExecutionMonitor_.notifyAll();
        }

        tasksExecutionMonitor_.unlock();

        logging_->logDebug("%" PRIu64 " add %" PRIu32 " tasks", id_, addedTasksCount);
    }
    else
    {
        logging_->logWarning("%" PRIu64 " can't add empty container with tasks", id_);
    }

    return result;
}


Result ThreadPool::addTaskToEveryWorker(const std::vector<std::shared_ptr<IThreadPoolTask>> & tasks) // TODO: Cover with tests
{
    logging_->logDebug("%" PRIu64 " is requested to add %" PRIu32 " tasks to every worker", id_, static_cast<uint32_t>(tasks.size()));

    Result result{ Result::ERROR };

    if (!tasks.empty())
    {
        workersMutex_.lock();

        if (!workers_.empty())
        {
            const auto workersSize = static_cast<uint32_t>(workers_.size());

            uint32_t workersIndex{ 0u };
            uint32_t tasksIndex{ 0u };

            for (auto && taskIt : tasks)
            {
                if (taskIt != nullptr)
                {
                    workers_[workersIndex % workersSize]->addTask(taskIt);
                    ++workersIndex;
                }
                else
                {
                    logging_->logWarning("%" PRIu64 " can't add nullptr task to worker", id_);
                }

                ++tasksIndex;
            }

            if (workersIndex > 0u)
            {
                statistic_.totalNumberOfAddedTasks += workersIndex;
                result = Result::OK;
            }
        }
        else
        {
            logging_->logWarning("%" PRIu64 " has no workers to add task", id_);
        }

        workersMutex_.unlock();
    }
    else
    {
        logging_->logWarning("%" PRIu64 " can't add empty container with tasks", id_);
    }

    return result;
}


std::shared_ptr<IThreadPoolTask> ThreadPool::removeOneTask(const uint64_t taskId)
{
    logging_->logDebug("%" PRIu64 " is requested to remove one task with id %" PRIu64, id_, taskId);

    return taskScheduler_->unscheduleOne(taskId);
}


std::vector<std::shared_ptr<IThreadPoolTask>> ThreadPool::removeAllTasks(const bool needsRemoveFromWorkers)
{
    logging_->logDebug("%" PRIu64 " is requested to remove all tasks (remove from workers = %s)", id_, std::to_string(needsRemoveFromWorkers).c_str());

    using TasksContainer = std::vector<std::shared_ptr<IThreadPoolTask>>;

    TasksContainer allRemovedTasks{ taskScheduler_->unscheduleAll() };

    if (needsRemoveFromWorkers)
    {
        workersMutex_.lock();

        for (auto && workerIt : workers_)
        {
            TasksContainer removedTasksFromWorker{ workerIt->removeAllTasks() };

            allRemovedTasks.insert(allRemovedTasks.end(),
                std::make_move_iterator(removedTasksFromWorker.begin()),
                std::make_move_iterator(removedTasksFromWorker.end()));
        }

        workersMutex_.unlock();
    }

    return allRemovedTasks;
}


Result ThreadPool::clearAllTasks(const bool needsClearFromWorkers)
{
    logging_->logDebug("%" PRIu64 " is requested to clear all tasks (clear from workers = %s)", id_, std::to_string(needsClearFromWorkers).c_str());

    Result result{ taskScheduler_->clearAll() };

    if (needsClearFromWorkers)
    {
        workersMutex_.lock();

        for (auto && workerIt : workers_)
        {
            result += workerIt->clearAllTasks();
        }

        workersMutex_.unlock();
    }

    return result;
}


Result ThreadPool::increaseWorkers(const uint32_t number)
{
    workersMutex_.lock();
    const Result result{ increaseWorkersInternal(number) };
    workersMutex_.unlock();

    return result;
}


Result ThreadPool::decreaseWorkers(const uint32_t number, const bool needsRescheduleTasks)
{
    workersMutex_.lock();
    const Result result{ decreaseWorkersInternal(number, needsRescheduleTasks) };
    workersMutex_.unlock();

    return result;
}


Result ThreadPool::startExecution()
{
    logging_->logDebug("%" PRIu64 " is requested to start execution", id_);

    workersMutex_.lock();

    Result result{ Result::ERROR };

    if (IThreadPool::State::READY == state_)
    {
        result = createManagingThread();
        if (result != Result::OK)
        {
            logging_->logError("%" PRIu64 " can't create load balancing thread", id_);
        }

        result += createWorkerThreads();
    }
    else
    {
        logging_->logWarning("%" PRIu64 " has already been started", id_);
    }

    workersMutex_.unlock();

    return result;
}


Result ThreadPool::pauseExecution()
{
    logging_->logDebug("%" PRIu64 " is requested to pause execution", id_);

    workersMutex_.lock();

    Result result{ Result::ERROR };

    if (IThreadPool::State::RUNNING == state_)
    {
        OSAL::ManagedThread::pauseExecution();
        result = pauseWorkerThreadsExecution();

        state_ = IThreadPool::State::PAUSED;
    }
    else
    {
        logging_->logWarning("%" PRIu64 " is not running", id_);
    }

    workersMutex_.unlock();

    return result;
}


Result ThreadPool::resumeExecution()
{
    logging_->logDebug("%" PRIu64 " is requested to resume execution", id_);

    workersMutex_.lock();

    Result result{ Result::ERROR };

    if (IThreadPool::State::PAUSED == state_)
    {
        OSAL::ManagedThread::resumeExecution();
        result = resumeWorkerThreadsExecution();

        state_ = IThreadPool::State::RUNNING;
    }
    else
    {
        logging_->logWarning("%" PRIu64 " is not paused", id_);
    }

    workersMutex_.unlock();

    return result;
}


///////////////////////////////////////////////////////////////////////////////////////////////
///
/// Protected ThreadPool methods used for managing workers and tasks.
///
///////////////////////////////////////////////////////////////////////////////////////////////

//! ATTENTION! This method is called with the workersMutex_ locked
void ThreadPool::loadBalance()
{
    logging_->logDebug("%" PRIu64 " is requested to do load balancing", id_);

    if (!workers_.empty())
    {
        const auto workerWithMinMaxTasksSizeIt = std::minmax_element(workers_.cbegin(), workers_.cend(),
                                                 [](const WorkersContainer::value_type & lhsWorker, const WorkersContainer::value_type & rhsWorker)
                                                 {
                                                     return lhsWorker->getTasksSize() < rhsWorker->getTasksSize();
                                                 });

        const auto minTasksSize = (*workerWithMinMaxTasksSizeIt.first)->getTasksSize();
        const auto maxTasksSize = (*workerWithMinMaxTasksSizeIt.second)->getTasksSize();

        //! We don't consider case when first workers has for example 10 tasks and other 11 as imbalanced situation.
        const bool isLoadBalanced{ minTasksSize == maxTasksSize || minTasksSize + 1u == maxTasksSize };
        if (!isLoadBalanced)
        {
            logging_->logDebug("%" PRIu64 " is load balancing tasks between workers (add to %" PRIu32 ", steal from %" PRIu32 ")",
                               id_, (*workerWithMinMaxTasksSizeIt.first)->getId(), (*workerWithMinMaxTasksSizeIt.second)->getId());

            // TODO: Add feature for stealing multiple tasks
            (*workerWithMinMaxTasksSizeIt.first)->addTask((*workerWithMinMaxTasksSizeIt.second)->stealTask());
        }
        else
        {
            logging_->logDebug("%" PRIu64 " is load balanced", id_);
        }
    }
}


//! ATTENTION! This method is called with the workersMutex_ locked
ThreadPool::WorkersContainer::value_type ThreadPool::getAvailableWorker()
{
    WorkersContainer::value_type availableWorker{};

    if (!workers_.empty())
    {
        // Firstly try to find empty and waiting worker
        const auto emptyWaitingWorkerIt = std::find_if(workers_.cbegin(), workers_.cend(),
                                            [](const WorkersContainer::value_type & worker)
                                            {
                                                return worker->getTasksSize() == 0u && worker->getState() == ThreadPoolWorker::State::WAITING;
                                            });

        if (emptyWaitingWorkerIt != workers_.cend())
        {
            availableWorker = std::move(*emptyWaitingWorkerIt);
        }
        // If there are no empty and waiting worker then check for worker with minimun tasks
        else
        {
            //! std::min_element can't be applied here since it requires strict weak ordering, when the arguments are compared
            //! each of the elements is running worker(thread) so, there are no guarantee in weak ordering comparison

            static const auto firstHasLessTasksThenSecond =
                [](const WorkersContainer::value_type & lhsWorker, const WorkersContainer::value_type & rhsWorker)
                {
                    return lhsWorker->getTasksSize() < rhsWorker->getTasksSize();
                };

            auto workerWithMinimumTasks = workers_.cbegin();

            for (auto workerIt = workers_.cbegin(); workerIt != workers_.cend(); ++workerIt)
            {
                if (firstHasLessTasksThenSecond(*workerIt, *workerWithMinimumTasks))
                {
                    workerWithMinimumTasks = workerIt;
                }
            }

            availableWorker = *workerWithMinimumTasks;
        }
    }

    return availableWorker;
}


//! ATTENTION! This method is called with the tasksExecutionMonitor_ locked
std::shared_ptr<IThreadPoolTask> ThreadPool::getTaskForExecution()
{
    logging_->logDebug("%" PRIu64 " requested to get task for execution", id_);

    return taskScheduler_->getTaskForExecution();
}


ITaskScheduler* ThreadPool::getNewTaskScheduler(const ThreadPoolOptions::SchedulerType schedulerType) const
{
    switch (schedulerType)
    {
        case ThreadPoolOptions::SchedulerType::FCFS:        return new FirstComeFirstServedTaskScheduler    { logging_->getNewLoggingInstance("FCFS") };
        case ThreadPoolOptions::SchedulerType::PRIORITY:    return new PriorityTaskScheduler                { logging_->getNewLoggingInstance("PriorityScheduler") };
        case ThreadPoolOptions::SchedulerType::SJF:         return new ShortestJobFirstTaskScheduler        { logging_->getNewLoggingInstance("SJF") };
        default:
            logging_->logWarning("%" PRIu64 " Undefined scheduler type provided", id_);
            return nullptr;
    }
}


void ThreadPool::waitFinished(const int64_t timeout)
{
    if (options_.needsWaitAllTasksExecutionFinished())
    {
        waitAllTasksExecutionFinished(timeout);
    }

    threadMustEnd_ = true;

    // Stops workers execution to avoid waiting during destruction
    workersMutex_.lock();
    stopWorkerThreadsExecution();
    workersMutex_.unlock();

    tasksExecutionMonitor_.lock();
    tasksExecutionMonitor_.notifyAll();
    tasksExecutionMonitor_.unlock();

    logging_->logDebug("%" PRIu64 " is waiting manager thread finished...", id_);

    OSAL::ManagedThread::waitFinished(timeout);
}

///////////////////////////////////////////////////////////////////////////////////////////////
///
/// Private OSAL::ManagedThread methods
///
///////////////////////////////////////////////////////////////////////////////////////////////

//! Loop is created in OSAL::ManagedThread
void ThreadPool::managedRun()
{
    if (needsGetNewTaskForExecution_)
    {
        tasksExecutionMonitor_.lock();

        currentTaskForExecution_ = getTaskForExecution();
        needsGetNewTaskForExecution_ = (nullptr == currentTaskForExecution_);

        areAllTasksPutForExecution_ = needsGetNewTaskForExecution_;

        if (areAllTasksPutForExecution_)
        {
            logging_->logDebug("%" PRIu64 " give all tasks for execution", id_);
            tasksExecutionMonitor_.notifyAll();
        }

        tasksExecutionMonitor_.unlock();
    }

    // Avoid further proceding if thread must end at this point
    if (threadMustEnd_)
    {
        return;
    }

    if (currentTaskForExecution_ != nullptr)
    {
        workersMutex_.lock();

        const WorkersContainer::value_type &availableWorker = getAvailableWorker();
        if (availableWorker != nullptr)
        {
            logging_->logDebug("%" PRIu64 " manager thread add task %" PRIu64 " to worker %" PRIu64,
                               id_, currentTaskForExecution_->getId(), availableWorker->getId());

            availableWorker->addTask(std::move(currentTaskForExecution_));
            needsGetNewTaskForExecution_ = true;
        }

        workersMutex_.unlock();
    }
    else
    {
        logging_->logDebug("%" PRIu64 " manager thread is waiting....", id_);

        // If we are going to wait for new task for execution but at this moment new task arrives, then skip waiting
        if (needsGetNewTaskForExecution_ && taskScheduler_->getSize() != 0u)
        {
            logging_->logDebug("%" PRIu64 " manager thread bypass waiting", id_);
        }
        // Else we don't have either task or available worker, so go for waiting if thread must not end
        else if (!threadMustEnd_)
        {
            // Spurious wake up is not a problem here, so no need to wait in a loop
            tasksExecutionMonitor_.lock();
            const Result result = tasksExecutionMonitor_.wait(waitForNewTaskOrWorkerAvailabilityTimeoutInMicroseconds_);
            tasksExecutionMonitor_.unlock();

            logging_->logDebug("%" PRIu64 " manager thread finish waiting with result %s", id_, resultToStr(result).c_str());
        }

        // If after waiting thread pool must end then we avoid load balancing
        if (!threadMustEnd_)
        {
            workersMutex_.lock();
            loadBalance();
            workersMutex_.unlock();
        }
    }

    // Avoid further proceding if thread must end at this point
    if (threadMustEnd_)
    {
        return;
    }
}


///////////////////////////////////////////////////////////////////////////////////////////////
///
/// Protected ThreadPool methods
///
///////////////////////////////////////////////////////////////////////////////////////////////

ThreadPool::ThreadPool(const ThreadPoolOptions & options, Logging * logging)
    : options_{ options }
    , state_{ IThreadPool::State::READY }
    , logging_{ logging == nullptr ? new Logging{ "ThreadPool" } : logging }
    , waitForNewTaskOrWorkerAvailabilityTimeoutInMicroseconds_{ 5000000u }
    , currentTaskForExecution_{}
    , needsGetNewTaskForExecution_{ true }
    , areAllTasksPutForExecution_{ true }
    , tasksExecutionMonitor_{ logging == nullptr ? new Logging{ "ThreadPool(TasksExecutionMonitor)" }
                                                 : logging->getNewLoggingInstance("TasksExecutionMonitor") }
    , workersMutex_{ logging == nullptr ? new Logging{ "ThreadPool(WorkersMutex)" }
                                        : logging->getNewLoggingInstance("WorkersMutex") }
{
    static std::atomic<uint64_t> id{ 1u };
    id_ = id.load();
    id.fetch_add(1u);

    const ThreadPoolOptions::SchedulerType schedulerType{ options.getSchedulerType() };
    taskScheduler_.reset(getNewTaskScheduler(schedulerType));

    const uint32_t workersSize{ options_.getInitialNumberOfWorkers() };
    for (uint32_t i = 0u; i < workersSize; ++i)
    {
        workers_.emplace_back(
            new ThreadPoolWorker{ getNewTaskScheduler(schedulerType), tasksExecutionMonitor_, logging_->getNewLoggingInstance("Worker") });
    }

    if (!options.needsPostponeExecution())
    {
        startExecution();
    }
}


ThreadPool::~ThreadPool()
{
    threadMustEnd_ = true;

    waitFinished(-1);
}


//! ATTENTION! This method is called with the workersMutex_ locked
Result ThreadPool::createManagingThread()
{
    logging_->logDebug("%" PRIu64 " is requested to create managing thread", id_);

    Result result{ Result::ERROR };

    if (IThreadPool::State::READY == state_)
    {
        result = this->create();
        if (Result::OK == result)
        {
            state_ = IThreadPool::State::RUNNING;
        }
        else
        {
            logging_->logError("%" PRIu64 " can't create thread for managing workers and tasks", id_);
        }
    }

    return result;
}


//! ATTENTION! This method is called with the workersMutex_ locked
Result ThreadPool::createWorkerThreads()
{
    logging_->logDebug("%" PRIu64 " is requested to create worker threads", id_);

    Result result{ Result::OK };

    for (auto && workerIt: workers_)
    {
        if (workerIt->getState() == ThreadPoolWorker::State::READY)
        {
            const Result createResult{ workerIt->create() };
            if (createResult != Result::OK)
            {
                logging_->logError("%" PRIu64 " can't create worker with id %" PRIu64, id_, workerIt->getId());
                result = createResult;
            }
        }
    }

    return result;
}


//! ATTENTION! This method is called with the workersMutex_ locked
Result ThreadPool::stopWorkerThreadsExecution()
{
    logging_->logDebug("%" PRIu64 " is requested to stop worker threads execution", id_);

    Result result{ Result::OK };

    for (auto && workerIt: workers_)
    {
        result += workerIt->stopExecution();
    }

    return result;
}


//! ATTENTION! This method is called with the workersMutex_ locked
Result ThreadPool::pauseWorkerThreadsExecution()
{
    logging_->logDebug("%" PRIu64 " is requested to pause worker threads execution", id_);

    Result result{ Result::OK };

    for (auto && workerIt: workers_)
    {
        result += workerIt->pauseExecution();
    }

    return result;
}


//! ATTENTION! This method is called with the workersMutex_ locked
Result ThreadPool::resumeWorkerThreadsExecution()
{
    logging_->logDebug("%" PRIu64 " is requested to resume worker threads execution", id_);

    Result result{ Result::OK };

    for (auto && workerIt: workers_)
    {
        result += workerIt->resumeExecution();
    }

    return result;
}


//! ATTENTION! This method is called with the workersMutex_ locked
void ThreadPool::eraseWorkersAndRescheduleTasks(const WorkersContainer::iterator begin, const WorkersContainer::iterator end, const bool needsRescheduleTasks)
{
    for (auto workerIt = begin; workerIt != end; ++workerIt)
    {
        if (needsRescheduleTasks)
        {
            const auto &removedTasks = (*workerIt)->removeAllTasks();
            for (auto && taskIt: removedTasks)
            {
                taskScheduler_->schedule(std::move(taskIt));
            }
        }

        // Stop execution to successfully erase worker
        (*workerIt)->stopExecution();

        logging_->logDebug("%" PRIu64 " marked for erase worker with id %" PRIu64, id_, (*workerIt)->getId());
    }

    workers_.erase(begin, end);
}


//! ATTENTION! This method is called with the workersMutex_ locked
Result ThreadPool::increaseWorkersInternal(const uint32_t number)
{
    logging_->logDebug("%" PRIu64 " is requested to increase workers by %" PRIu32, id_, number);

    Result result{ Result::ERROR };

    const uint32_t numberOfIncrease{ getNumberOfWorkersToIncrease(number) };

    if (numberOfIncrease > 0u)
    {
        const ThreadPoolOptions::SchedulerType schedulerType{ options_.getSchedulerType() };
        for (uint32_t i = 0u; i < numberOfIncrease; ++i)
        {
            // tasksExecutionMonitor_ works as free state monitor
            workers_.emplace_back(
                new ThreadPoolWorker{ getNewTaskScheduler(schedulerType), tasksExecutionMonitor_, logging_->getNewLoggingInstance("Worker") });
        }

        switch (state_)
        {
            case IThreadPool::State::READY:
                result = Result::OK;
                break;
            case IThreadPool::State::RUNNING:
                result = createWorkerThreads();
                break;
            case IThreadPool::State::PAUSED:
                result = createWorkerThreads();
                pauseWorkerThreadsExecution();
                break;
            default:
                break;
        }

        logging_->logDebug("%" PRIu64 " increased workers by %" PRIu32, id_, numberOfIncrease);
    }
    else
    {
        logging_->logWarning("%" PRIu64 " can't increase workers by %" PRIu32, id_, number);
    }

    return result;
}


//! ATTENTION! This method is called with the workersMutex_ locked
Result ThreadPool::decreaseWorkersInternal(const uint32_t number, const bool needsRescheduleTasks)
{
    logging_->logDebug("%" PRIu64 " is requested to decrease workers by %" PRIu32, id_, number);

    Result result{ Result::ERROR };

    uint32_t numberOfDecrease{ getNumberOfWorkersToDecrease(number) };

    if (numberOfDecrease > 0u)
    {
        // Partition workers in a 2 groups: non empty and empty
        auto emptyWorkersBeginIt = std::partition(workers_.begin(), workers_.end(),
                                            [](const WorkersContainer::value_type & worker)
                                            {
                                                return worker->getTasksSize() != 0u;
                                            });

        // Try firstly remove empty workers
        if (emptyWorkersBeginIt != workers_.end())
        {
            const uint32_t numberOfEmptyWorkers{ static_cast<uint32_t>(std::distance(emptyWorkersBeginIt, workers_.end())) };
            if (numberOfEmptyWorkers > numberOfDecrease)
            {
                logging_->logDebug("%" PRIu64 " remove %" PRIu32 " empty workers", id_, numberOfDecrease);

                eraseWorkersAndRescheduleTasks(emptyWorkersBeginIt, emptyWorkersBeginIt + numberOfDecrease, needsRescheduleTasks);
                numberOfDecrease = 0u;
            }
            else
            {
                logging_->logDebug("%" PRIu64 " remove %" PRIu32 " empty workers", id_, numberOfEmptyWorkers);

                eraseWorkersAndRescheduleTasks(emptyWorkersBeginIt, workers_.end(), needsRescheduleTasks);
                numberOfDecrease -= numberOfEmptyWorkers;
            }

            result = Result::OK;
        }

        // If after removing empty workers we still have to decrease workers, remove other workers
        if (numberOfDecrease > 0u)
        {
            logging_->logDebug("%" PRIu64 " remove %" PRIu32 " non empty workers", id_, numberOfDecrease);

            //! It's secure to do workers_.begin() + numberOfDecrease because we have check at the beginning of method
            eraseWorkersAndRescheduleTasks(workers_.begin(), workers_.begin() + numberOfDecrease, needsRescheduleTasks);

            result = Result::OK;
        }
    }
    else
    {
        logging_->logWarning("%" PRIu64 " can't decrease workers by %" PRIu32, id_, number);
    }

    return result;
}


//! ATTENTION! This method is called with the workersMutex_ locked
uint32_t ThreadPool::getNumberOfWorkersToIncrease(const uint32_t initialNumber) const
{
    const uint32_t currentNumberOfWorkers   { static_cast<uint32_t>(workers_.size()) };
    const uint32_t maxNumberOfWorkers       { options_.getMaxNumberOfWorkers() };

    uint32_t numberOfIncrease{ initialNumber };

    if (currentNumberOfWorkers + initialNumber > maxNumberOfWorkers)
    {
        numberOfIncrease = maxNumberOfWorkers - currentNumberOfWorkers;
    }

    return numberOfIncrease;
}


//! ATTENTION! This method is called with the workersMutex_ locked
uint32_t ThreadPool::getNumberOfWorkersToDecrease(const uint32_t initialNumber) const
{
    const uint32_t currentNumberOfWorkers   { static_cast<uint32_t>(workers_.size()) };
    const uint32_t minNumberOfWorkers       { options_.getMinNumberOfWorkers() };

    uint32_t numberOfDecrease{ initialNumber };

    if (static_cast<int64_t>(currentNumberOfWorkers) - static_cast<int64_t>(initialNumber) < static_cast<int64_t>(minNumberOfWorkers))
    {
        numberOfDecrease = currentNumberOfWorkers - minNumberOfWorkers;
    }

    return numberOfDecrease;
}


//! ATTENTION! This method is called with the workersMutex_ locked
size_t ThreadPool::getTasksSizeFromAllWorkers() const
{
    size_t tasksSize{ 0u };

    // Suppressing cppcheck warning suggesting usage of std::accumulate due to performance
    for (auto && workerIt : workers_)
    {
        // cppcheck-suppress useStlAlgorithm
        tasksSize += workerIt->getTasksSize();
    }

    return tasksSize;
}
