#include "ThreadPoolWorker.h"
#include "FirstComeFirstServedTaskScheduler.h"


ThreadPoolWorker::ThreadPoolWorker(ITaskScheduler * taskScheduler, OSAL::Monitor & freeStateMonitor, Logging * logging)
    : ManagedThread{ logging }
    , freeStateMonitor_{ freeStateMonitor }
    , taskScheduler_{ nullptr == taskScheduler ? std::unique_ptr<ITaskScheduler>{ new FirstComeFirstServedTaskScheduler{ logging } }
                                               : std::unique_ptr<ITaskScheduler>{ taskScheduler} }
    , waitTaskForExecutionTimeoutInMicroseconds_{ 5000000u }
    , waitingTimeMutex_{ logging_->getNewLoggingInstance("WaitingTimeMutex") }
{
}


ThreadPoolWorker::~ThreadPoolWorker()
{
    taskScheduler_->notifyTaskForExecution();

    threadMustEnd_ = true;

    waitFinished(-1);
}


ITaskScheduler::Statistic ThreadPoolWorker::getTasksStatistic() const
{
    return taskScheduler_->getStatistic();
}


size_t ThreadPoolWorker::getTasksSize() const
{
    return taskScheduler_->getSize();
}


bool ThreadPoolWorker::isTaskAdded(const uint64_t taskId) const
{
    return taskScheduler_->isScheduled(taskId);
}


uint64_t ThreadPoolWorker::getWaitingTime()
{
    waitingTimeMutex_.lock();
    const uint64_t elapsedTime{ waitingTime_.getElapsedTime() };
    waitingTimeMutex_.unlock();

    return elapsedTime;
}


std::shared_ptr<IThreadPoolTask> ThreadPoolWorker::stealTask()
{
    return taskScheduler_->steal();
}


Result ThreadPoolWorker::addTask(const std::shared_ptr<IThreadPoolTask> task)
{
    return taskScheduler_->schedule(task);
}


Result ThreadPoolWorker::addTasks(const std::vector<std::shared_ptr<IThreadPoolTask>> & tasks)  // TODO: Cover with tests
{
    return taskScheduler_->schedule(tasks);
}


std::shared_ptr<IThreadPoolTask> ThreadPoolWorker::removeOneTask(const uint64_t taskId)
{
    return taskScheduler_->unscheduleOne(taskId);
}


std::vector<std::shared_ptr<IThreadPoolTask>> ThreadPoolWorker::removeAllTasks()
{
    return taskScheduler_->unscheduleAll();
}


Result ThreadPoolWorker::clearAllTasks()
{
    return taskScheduler_->clearAll();
}


///////////////////////////////////////////////////////////////////////////////////////////////
///
/// Private OSAL::ManagedThread methods
///
///////////////////////////////////////////////////////////////////////////////////////////////

// Loop is created in OSAL::ManagedThread
void ThreadPoolWorker::managedRun()
{
    const std::shared_ptr<IThreadPoolTask> &gotTaskForExecution = taskScheduler_->getTaskForExecution();

    if (nullptr == gotTaskForExecution)
    {
        stateMonitor_.lock();
        state_ = State::WAITING;
        stateMonitor_.unlock();

        logging_->logDebug("%" PRIu64 " is waiting...", id_);

        // Notify owner about availability
        freeStateMonitor_.lock();
        freeStateMonitor_.notifyAll();
        freeStateMonitor_.unlock();

        // Avoid waiting if thread must end
        if (!threadMustEnd_)
        {
            const Result result = taskScheduler_->waitTaskForExecution(waitTaskForExecutionTimeoutInMicroseconds_);
            logging_->logDebug("%" PRIu64 " finish waiting with result %s", id_, resultToStr(result).c_str());
        }
        else
        {
            logging_->logDebug("%" PRIu64 " skip waiting since thread must end", id_);
        }
    }
    else
    {
        waitingTimeMutex_.lock();
        waitingTime_.restart();
        waitingTimeMutex_.unlock();

        stateMonitor_.lock();
        state_ = State::RUNNING;
        stateMonitor_.unlock();

        logging_->logDebug("%" PRIi64 " is running with task %" PRIu64, id_, gotTaskForExecution->getId());

        const Result result{ gotTaskForExecution->execute() };
        if (result != Result::OK)
        {
            logging_->logWarning("%" PRIi64 " can't execute task %" PRIu64, id_, gotTaskForExecution->getId());
        }
        else
        {
            logging_->logDebug("%" PRIi64 " finish execution of task %" PRIu64, id_, gotTaskForExecution->getId());
        }
    }
}
