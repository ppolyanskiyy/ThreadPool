#include "gtest/gtest.h"
#include "ThreadPoolTask.h"
#include "ThreadPool.h"


class TestTask : public ThreadPoolTask
{
};


class ExtendedThreadPoolTest : public ::testing::Test
{
public:
    using TasksContainer = std::vector<std::shared_ptr<IThreadPoolTask>>;

    void EXPECT_UNAVAILABLE_CAPACITY
        (const IThreadPool::Statistic & statisticBeforeChanges, const IThreadPool::Statistic & statisticAfterChanges, const IThreadPool::State targetState)
    {
        EXPECT_EQ(statisticAfterChanges.currentNumberOfAllWorkers, statisticBeforeChanges.currentNumberOfAllWorkers);

        switch (targetState)
        {
            case IThreadPool::State::READY:
                EXPECT_EQ(statisticAfterChanges.numberOfWorkersInReadyState, statisticBeforeChanges.numberOfWorkersInReadyState);
                break;

            case IThreadPool::State::RUNNING:
                EXPECT_EQ(statisticAfterChanges.numberOfWorkersInRunningState + statisticAfterChanges.numberOfWorkersInWaitingState,
                          statisticBeforeChanges.numberOfWorkersInRunningState + statisticBeforeChanges.numberOfWorkersInWaitingState);
                break;

            case IThreadPool::State::PAUSED:
                EXPECT_EQ(statisticAfterChanges.numberOfWorkersInPausedState, statisticBeforeChanges.numberOfWorkersInPausedState);
                break;

            default:
                break;
        }
    }

    void EXPECT_TASKS_SAME(const TasksContainer & tasks, const std::shared_ptr<IThreadPoolTask> & sourceTask)
    {
        EXPECT_FALSE(tasks.empty());
        EXPECT_EQ(static_cast<uint32_t>(tasks.size()), 1u);

        const uint64_t taskId = sourceTask->getId();

        for(auto && taskIt: tasks)
        {
            EXPECT_EQ(taskIt->getId(), taskId);
        }
    }
};


class Foundations_ThreadPoolBase: public ExtendedThreadPoolTest
{
public:

    const uint32_t inTestDelayInMicroseconds{ 250000u };

    // Thread pool options with following name conventions:
    //* options_<initial workers>_<min workers>_<max workers>_?postpone_?waitFinished
    // <?> - means optional. Put only if you need it. By default it's false.
    ThreadPoolOptions options_2_1_3                     { 2u, 1u, 3u, false, false };
    ThreadPoolOptions options_2_1_3_postpone            { 2u, 1u, 3u, true,  false };

    ThreadPoolOptions options_1_1_1                     { 1u, 1u, 1u, false, false };
    ThreadPoolOptions options_1_1_1_postpone            { 1u, 1u, 1u, true,  false };

    ThreadPoolOptions options_2_3_1                     { 2u, 1u, 3u, false, false };

    ThreadPoolOptions options_0_0_0                     { 0u, 0u, 0u, false, false };
    ThreadPoolOptions options_0_0_0_postpone            { 0u, 0u, 0u, true,  false };

    ThreadPoolOptions options_2_2_2                     { 2u, 2u, 2u, false, false };
    ThreadPoolOptions options_1_1_3                     { 1u, 1u, 3u, false, false };

protected: // Helper methods

    std::shared_ptr<TestTask> getSubmittedTask(const uint32_t inTaskDelayInMicroseconds = 1000000u)
    {
        std::shared_ptr<TestTask> task = std::make_shared<TestTask>();
        task->submitOne([inTaskDelayInMicroseconds] {
            OSAL::Thread::delay(inTaskDelayInMicroseconds);
            return true;
            });

        return task;
    }

    TasksContainer getSubmittedTasks(const uint32_t repetitions, const uint32_t inTaskDelayInMicroseconds = 1000000u)
    {
        return ThreadPoolTask::submitRepeated(repetitions, [inTaskDelayInMicroseconds] {
            OSAL::Thread::delay(inTaskDelayInMicroseconds);
            return true;
            });
    }

protected: // increaseWorkers

    void testIncreaseWorkersWithValidIncreaseNumber
        (const std::shared_ptr<IThreadPool> & threadPool, const uint32_t number, const IThreadPool::State targetState)
    {
        uint32_t newNumberOfWorkers                         { threadPool->getStatistic().currentNumberOfAllWorkers + number };
        const uint32_t maxNumberOfWorkers                   { threadPool->getOptions().getMaxNumberOfWorkers() };
        if (newNumberOfWorkers > maxNumberOfWorkers)        { newNumberOfWorkers = maxNumberOfWorkers; }

        const Result result                                 { threadPool->increaseWorkers(number) };
        const IThreadPool::Statistic statisticAfterChanges  { threadPool->getStatistic() };


        EXPECT_EQ(result, Result::OK);
        EXPECT_EQ(statisticAfterChanges.currentNumberOfAllWorkers, newNumberOfWorkers);

        switch (targetState)
        {
            case IThreadPool::State::READY:
                EXPECT_EQ(statisticAfterChanges.numberOfWorkersInReadyState, newNumberOfWorkers);
                break;

            case IThreadPool::State::RUNNING:
                EXPECT_EQ(statisticAfterChanges.numberOfWorkersInRunningState + statisticAfterChanges.numberOfWorkersInWaitingState, newNumberOfWorkers);
                break;

            case IThreadPool::State::PAUSED:
                EXPECT_EQ(statisticAfterChanges.numberOfWorkersInPausedState, newNumberOfWorkers);
                break;

            default:
                break;
        }
    }

    void testIncreaseWorkersWithInvalidIncreaseNumberAndUnavailableCapacity
        (const std::shared_ptr<IThreadPool> & threadPool, const uint32_t number, const IThreadPool::State targetState)
    {
        const IThreadPool::Statistic statisticBeforeChanges     { threadPool->getStatistic() };
        const Result result                                     { threadPool->increaseWorkers(number) };
        const IThreadPool::Statistic statisticAfterChanges      { threadPool->getStatistic() };

        EXPECT_EQ(result, Result::ERROR);
        EXPECT_UNAVAILABLE_CAPACITY(statisticBeforeChanges, statisticAfterChanges, targetState);
    }


protected: // decreaseWorkers

    void testDecreaseWorkersWithValidDecreaseNumber
        (const std::shared_ptr<IThreadPool> & threadPool, const uint32_t number, const bool needsReschedule, const IThreadPool::State targetState)
    {
        const uint32_t workersSizeBeforDecrease{ static_cast<uint32_t>(threadPool->getWorkersSize()) };

        if (needsReschedule)
        {
            threadPool->addTaskToEveryWorker(getSubmittedTasks(workersSizeBeforDecrease));
            OSAL::Thread::delay(inTestDelayInMicroseconds); // Let the workers get tasks for execution
        }

        const uint32_t minNumberOfWorkers                   { threadPool->getOptions().getMinNumberOfWorkers() };
        const uint32_t newNumberOfWorkers
            {
              static_cast<int64_t>(threadPool->getWorkersSize()) - static_cast<int64_t>(number) < static_cast<int64_t>(minNumberOfWorkers)
              ? minNumberOfWorkers
              : workersSizeBeforDecrease - number
            };

        const Result result                                 { threadPool->decreaseWorkers(number, needsReschedule) };

        const uint32_t tasksSizeAfterDecrease               { static_cast<uint32_t>(threadPool->getTasksSize(false)) };
        const IThreadPool::Statistic statisticAfterChanges  { threadPool->getStatistic() };


        EXPECT_EQ(result, Result::OK);
        EXPECT_EQ(statisticAfterChanges.currentNumberOfAllWorkers, newNumberOfWorkers);

        switch (targetState)
        {
            case IThreadPool::State::READY:
                EXPECT_EQ(statisticAfterChanges.numberOfWorkersInReadyState, newNumberOfWorkers);
                if (needsReschedule)
                {
                    EXPECT_EQ(tasksSizeAfterDecrease, newNumberOfWorkers);
                }
                break;

            case IThreadPool::State::RUNNING:
                EXPECT_EQ(statisticAfterChanges.numberOfWorkersInRunningState + statisticAfterChanges.numberOfWorkersInWaitingState, newNumberOfWorkers);
                if (needsReschedule)
                {
                    EXPECT_LE(tasksSizeAfterDecrease, newNumberOfWorkers);
                }
                break;

            case IThreadPool::State::PAUSED:
                EXPECT_EQ(statisticAfterChanges.numberOfWorkersInPausedState, newNumberOfWorkers);
                if (needsReschedule)
                {
                    EXPECT_EQ(tasksSizeAfterDecrease, newNumberOfWorkers);
                }
                break;

            default:
                break;
        }
    }

    void testDecreaseWorkersWithRunningAndWaitingWorkers(const std::shared_ptr<IThreadPool> & threadPool)
    {
        const IThreadPool::Statistic statisticBeforeChanges     { threadPool->getStatistic() };

        threadPool->addTask(getSubmittedTask());
        OSAL::Thread::delay(inTestDelayInMicroseconds); // Let the workers get tasks for execution

        const Result result                                     { threadPool->decreaseWorkers(1u, false) };
        const IThreadPool::Statistic statisticAfterChanges      { threadPool->getStatistic() };

        EXPECT_EQ(result, Result::OK);
        EXPECT_EQ(statisticBeforeChanges.numberOfWorkersInWaitingState, 2u);
        EXPECT_EQ(statisticAfterChanges.numberOfWorkersInWaitingState, 1u);
        EXPECT_EQ(statisticAfterChanges.numberOfWorkersInRunningState, 0u);
    }

    void testDecreaseWorkersWithInvalidDecreaseNumberAndUnavailableCapacity
        (const std::shared_ptr<IThreadPool> & threadPool, const uint32_t number, const IThreadPool::State targetState)
    {
        const IThreadPool::Statistic statisticBeforeChanges     { threadPool->getStatistic() };
        const Result result                                     { threadPool->decreaseWorkers(number) };
        const IThreadPool::Statistic statisticAfterChanges      { threadPool->getStatistic() };


        EXPECT_EQ(result, Result::ERROR);
        EXPECT_UNAVAILABLE_CAPACITY(statisticBeforeChanges, statisticAfterChanges, targetState);
    }


protected: // addTask

    void testAddTaskWithValidTask(const std::shared_ptr<IThreadPool> & threadPool, const IThreadPool::State targetState)
    {
        const std::shared_ptr<TestTask> task{ getSubmittedTask(0u) };

        uint32_t newNumberOfTasks                           { static_cast<uint32_t>(threadPool->getTasksSize(false)) + 1u};

        const Result result                                 { threadPool->addTask(task) };
        const IThreadPool::Statistic statisticAfterChanges  { threadPool->getStatistic() };

        EXPECT_EQ(result, Result::OK);
        EXPECT_EQ(statisticAfterChanges.totalNumberOfAddedTasks, newNumberOfTasks);

        switch (targetState)
        {
            case IThreadPool::State::READY:
                EXPECT_EQ(threadPool->getTasksSize(false), static_cast<size_t>(newNumberOfTasks));
                EXPECT_TRUE(threadPool->isTaskAdded(task->getId()));
                break;

            case IThreadPool::State::RUNNING:
                EXPECT_LE(threadPool->getTasksSize(false), static_cast<size_t>(newNumberOfTasks));
                break;

            case IThreadPool::State::PAUSED:
                EXPECT_EQ(threadPool->getTasksSize(false), static_cast<size_t>(newNumberOfTasks));
                EXPECT_TRUE(threadPool->isTaskAdded(task->getId()));
                break;

            default:
                break;
        }
    }

    void testAddTaskWithInvalidTask(const std::shared_ptr<IThreadPool> & threadPool)
    {
        const Result result{ threadPool->addTask(nullptr) };

        EXPECT_EQ(result, Result::ERROR);
        EXPECT_EQ(threadPool->getTasksSize(false), 0u);
        EXPECT_EQ(threadPool->getStatistic().totalNumberOfAddedTasks, 0u);
    }


protected: // addTaskToEveryWorker

    void testAddTaskToEveryWorkerWithValidTask(const std::shared_ptr<IThreadPool> & threadPool, IThreadPool::State targetState)
    {
        auto tasks = getSubmittedTasks(static_cast<uint32_t>(threadPool->getWorkersSize()), 0u);

        const Result result                                 { threadPool->addTaskToEveryWorker(tasks) };
        const IThreadPool::Statistic statisticAfterChanges  { threadPool->getStatistic() };

        EXPECT_EQ(result, Result::OK);
        EXPECT_EQ(statisticAfterChanges.totalNumberOfAddedTasks, static_cast<uint32_t>(threadPool->getWorkersSize()));
        EXPECT_EQ(threadPool->getTasksSize(false), 0u);

        switch (targetState)
        {
            case IThreadPool::State::READY:
                // Nothing to test for now
                break;

            case IThreadPool::State::RUNNING:
                // Nothing to test for now
                break;

            case IThreadPool::State::PAUSED:
                // Nothing to test for now
                break;

            default:
                break;
        }
    }

    void testAddTaskToEveryWorkerWithInvalidTask(const std::shared_ptr<IThreadPool> & threadPool)
    {
        const Result result{ threadPool->addTaskToEveryWorker(TasksContainer{}) };

        EXPECT_EQ(result, Result::ERROR);
        EXPECT_EQ(threadPool->getTasksSize(false), 0u);
        EXPECT_EQ(threadPool->getStatistic().totalNumberOfAddedTasks, 0u);
    }

    void testAddTaskToEveryWorkerWithEmptyWorkers(const std::shared_ptr<IThreadPool> & threadPool)
    {
        const Result result{ threadPool->addTaskToEveryWorker(getSubmittedTasks(1u)) };

        ASSERT_EQ(threadPool->getWorkersSize(), 0u);
        EXPECT_EQ(result, Result::ERROR);
        EXPECT_EQ(threadPool->getTasksSize(false), 0u);
        EXPECT_EQ(threadPool->getStatistic().totalNumberOfAddedTasks, 0u);
    }


protected: // removeOneTask

    void testRemoveOneTaskWithAvailableTasks(const std::shared_ptr<IThreadPool> & threadPool, IThreadPool::State targetState)
    {
        if (IThreadPool::State::RUNNING == targetState) // adding extra task for running thread pool
        {
            threadPool->addTask(getSubmittedTask(100000u));
        }

        std::shared_ptr<TestTask> task{ getSubmittedTask(100000u) };
        threadPool->addTask(task);

        const uint32_t taskSizeBeforeChanges                 { static_cast<uint32_t>(threadPool->getTasksSize(false)) };
        const std::shared_ptr<IThreadPoolTask> removedTask   { threadPool->removeOneTask(task->getId()) };
        const uint32_t taskSizeAfterChanges                  { static_cast<uint32_t>(threadPool->getTasksSize(false)) };

        switch (targetState)
        {
            case IThreadPool::State::READY:
            case IThreadPool::State::PAUSED:
                EXPECT_NE(removedTask, nullptr);
                EXPECT_FALSE(threadPool->isTaskAdded(task->getId()));
                EXPECT_EQ(taskSizeAfterChanges, taskSizeBeforeChanges - 1u);
                break;

            case IThreadPool::State::RUNNING:
                // Not checking nullptr since while thread is running there may be no task before call to removeOneTask
                EXPECT_FALSE(threadPool->isTaskAdded(task->getId()));
                EXPECT_LE(taskSizeAfterChanges, taskSizeBeforeChanges - 1u);
                break;

            default:
                break;
        }
    }

    void testRemoveOneTaskWithUnavailableTasks(const std::shared_ptr<IThreadPool> & threadPool)
    {
        std::shared_ptr<TestTask> task{ getSubmittedTask() };
        threadPool->addTaskToEveryWorker(TasksContainer{ task });

        const std::shared_ptr<IThreadPoolTask> removedTask{ threadPool->removeOneTask(task->getId()) };

        EXPECT_EQ(removedTask, nullptr);
    }


protected: // removeAllTasks

    void testRemoveAllTasksWithAvailableTasks(const std::shared_ptr<IThreadPool> & threadPool, IThreadPool::State targetState)
    {
        std::shared_ptr<TestTask> task{ getSubmittedTask(100000u) };
        threadPool->addTask(task);

        const TasksContainer removedTasks{ threadPool->removeAllTasks(false) };

        switch (targetState)
        {
            case IThreadPool::State::READY:
            case IThreadPool::State::PAUSED:
                EXPECT_FALSE(removedTasks.empty());
                EXPECT_EQ(threadPool->getTasksSize(false), 0u);
                EXPECT_FALSE(threadPool->isTaskAdded(task->getId()));
                EXPECT_TASKS_SAME(removedTasks, task);
                break;

            case IThreadPool::State::RUNNING:
                // Not checking removedTasks.empty() since while thread is running there may be no tasks before call to removeAllTasks
                EXPECT_EQ(threadPool->getTasksSize(false), 0u);
                EXPECT_FALSE(threadPool->isTaskAdded(task->getId()));
                for (auto && removedTaskIt : removedTasks)
                {
                    EXPECT_EQ(removedTaskIt->getId(), task->getId());
                }
                break;

            default:
                break;
        }
    }

    void testRemoveAllTasksWithRemovingFromWorkersOption(const std::shared_ptr<IThreadPool> & threadPool)
    {
        std::shared_ptr<TestTask> task{ getSubmittedTask(0u) };
        threadPool->addTaskToEveryWorker( TasksContainer{ task });

        const TasksContainer removedTasks { threadPool->removeAllTasks(true) };

        EXPECT_FALSE(removedTasks.empty());
        EXPECT_EQ(threadPool->getTasksSize(false), 0u);
        EXPECT_FALSE(threadPool->isTaskAdded(task->getId()));
        for (auto && removedTaskIt : removedTasks)
        {
            EXPECT_EQ(removedTaskIt->getId(), task->getId());
        }
    }

    void testRemoveAllTasksWithUnavailableTasks(const std::shared_ptr<IThreadPool> & threadPool)
    {
        threadPool->addTaskToEveryWorker(getSubmittedTasks(1u));

        const TasksContainer removedTasks { threadPool->removeAllTasks(false) };

        EXPECT_TRUE(removedTasks.empty());
    }


protected: // clearAllTasks

    void testClearAllTasksWithAvailableTasks(const std::shared_ptr<IThreadPool> & threadPool, IThreadPool::State targetState)
    {
        if (IThreadPool::State::RUNNING == targetState) // adding extra task for running thread pool
        {
            threadPool->addTask(getSubmittedTask(100000u));
        }

        std::shared_ptr<TestTask> task{ getSubmittedTask(100000u) };
        threadPool->addTask(task);

        const Result result{ threadPool->clearAllTasks(false) };

        switch (targetState)
        {
            case IThreadPool::State::READY:
            case IThreadPool::State::PAUSED:
                EXPECT_EQ(result, Result::OK);
                EXPECT_EQ(threadPool->getTasksSize(false), 0u);
                EXPECT_FALSE(threadPool->isTaskAdded(task->getId()));
                break;

            case IThreadPool::State::RUNNING:
                // Not checking Result::OK since while thread is running there may be no tasks before call to clearAllTasks
                EXPECT_EQ(threadPool->getTasksSize(false), 0u);
                EXPECT_FALSE(threadPool->isTaskAdded(task->getId()));
                break;

            default:
                break;
        }
    }

    void testClearAllTasksWithClearingFromWorkersOption(const std::shared_ptr<IThreadPool> & threadPool)
    {
        std::shared_ptr<TestTask> task{ getSubmittedTask(0u) };
        threadPool->addTask(task);

        auto tasks = getSubmittedTasks(static_cast<uint32_t>(threadPool->getWorkersSize()), 0u);
        threadPool->addTaskToEveryWorker(tasks);

        const Result result{ threadPool->clearAllTasks(true) };

        EXPECT_EQ(result, Result::OK);
        EXPECT_EQ(threadPool->getTasksSize(false), 0u);
        EXPECT_FALSE(threadPool->isTaskAdded(task->getId()));
    }

    void testClearAllTasksWithUnavailableTasks(const std::shared_ptr<IThreadPool> & threadPool)
    {
        threadPool->addTaskToEveryWorker(getSubmittedTasks(1u));

        const Result result{ threadPool->clearAllTasks(false) };

        EXPECT_EQ(result, Result::ERROR);
    }


protected: // startExecution

    void testStartExecution(const std::shared_ptr<IThreadPool> & threadPool, const IThreadPool::State targetState)
    {
        const Result result{ threadPool->startExecution() };

        switch (targetState)
        {
            case IThreadPool::State::READY:
                EXPECT_EQ(result, Result::OK);
                EXPECT_EQ(threadPool->getState(), IThreadPool::State::RUNNING);
                break;

            case IThreadPool::State::RUNNING:
                EXPECT_EQ(result, Result::ERROR);
                EXPECT_EQ(threadPool->getState(), IThreadPool::State::RUNNING);
                break;

            case IThreadPool::State::PAUSED:
                EXPECT_EQ(result, Result::ERROR);
                EXPECT_EQ(threadPool->getState(), IThreadPool::State::PAUSED);
                break;

            default:
                break;
        }
    }


protected: // pauseExecution

    void testPauseExecution(const std::shared_ptr<IThreadPool> & threadPool, const IThreadPool::State targetState)
    {
        const Result result{ threadPool->pauseExecution() };

        switch (targetState)
        {
            case IThreadPool::State::READY:
                EXPECT_EQ(result, Result::ERROR);
                EXPECT_EQ(threadPool->getState(), IThreadPool::State::READY);
                break;

            case IThreadPool::State::RUNNING:
                EXPECT_EQ(result, Result::OK);
                EXPECT_EQ(threadPool->getState(), IThreadPool::State::PAUSED);
                break;

            case IThreadPool::State::PAUSED:
                EXPECT_EQ(result, Result::ERROR);
                EXPECT_EQ(threadPool->getState(), IThreadPool::State::PAUSED);
                break;

            default:
                break;
        }

        // Avoid blocking thread pool during destruction
        threadPool->resumeExecution();
    }


protected: // resumeExecution

    void testResumeExecution(const std::shared_ptr<IThreadPool> & threadPool, const IThreadPool::State targetState)
    {
        const Result result{ threadPool->resumeExecution() };

        switch (targetState)
        {
            case IThreadPool::State::READY:
                EXPECT_EQ(result, Result::ERROR);
                EXPECT_EQ(threadPool->getState(), IThreadPool::State::READY);
                break;

            case IThreadPool::State::RUNNING:
                EXPECT_EQ(result, Result::ERROR);
                EXPECT_EQ(threadPool->getState(), IThreadPool::State::RUNNING);
                break;

            case IThreadPool::State::PAUSED:
                EXPECT_EQ(result, Result::OK);
                EXPECT_EQ(threadPool->getState(), IThreadPool::State::RUNNING);
                break;

            default:
                break;
        }
    }


protected: // waitAllTasksExecutionFinished

    void testWaitAllTasksExecutionFinished(const std::shared_ptr<IThreadPool> & threadPool, const IThreadPool::State targetState)
    {
        auto tasks = getSubmittedTasks(static_cast<uint32_t>(threadPool->getWorkersSize()), 0u);
        threadPool->addTaskToEveryWorker(tasks);

        const Result result{ threadPool->waitAllTasksExecutionFinished(1000000u) };

        switch (targetState)
        {
            case IThreadPool::State::READY:
                EXPECT_EQ(result, Result::TIMEOUT);
                break;

            case IThreadPool::State::RUNNING:
                EXPECT_EQ(result, Result::OK);
                break;

            case IThreadPool::State::PAUSED:
                EXPECT_EQ(result, Result::TIMEOUT);
                break;

            default:
                break;
        }
    }
};




class Foundations_ThreadPool_Happy: public Foundations_ThreadPoolBase
{
};


class Foundations_ThreadPool_Unhappy: public Foundations_ThreadPoolBase
{
};




/////////////////////////////////////////////////////////////////////////////////////// ThreadPool

TEST_F(Foundations_ThreadPool_Happy, increaseWorkers)
{
    // Case with ready thread pool and valid increase number
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_2_1_3_postpone);
        Foundations_ThreadPoolBase::testIncreaseWorkersWithValidIncreaseNumber(threadPool, 1u, IThreadPool::State::READY);
    }

    // Case with running thread pool and valid increase number
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_2_1_3);
        Foundations_ThreadPoolBase::testIncreaseWorkersWithValidIncreaseNumber(threadPool, 1u, IThreadPool::State::RUNNING);
    }

    // Case with paused thread pool and valid increase number
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_2_1_3);
        threadPool->pauseExecution();
        Foundations_ThreadPoolBase::testIncreaseWorkersWithValidIncreaseNumber(threadPool, 1u, IThreadPool::State::PAUSED);
        threadPool->resumeExecution();
    }

    // Case with ready thread pool and invalid increase number but available capacity
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_2_1_3_postpone);
        Foundations_ThreadPoolBase::testIncreaseWorkersWithValidIncreaseNumber(threadPool, 999u, IThreadPool::State::READY);
    }

    // Case with running thread pool and invalid increase number but available capacity
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_2_1_3);
        Foundations_ThreadPoolBase::testIncreaseWorkersWithValidIncreaseNumber(threadPool, 999u, IThreadPool::State::RUNNING);
    }

    // Case with paused thread pool and invalid increase number but available capacity
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_2_1_3);
        threadPool->pauseExecution();
        Foundations_ThreadPoolBase::testIncreaseWorkersWithValidIncreaseNumber(threadPool, 999u, IThreadPool::State::PAUSED);
        threadPool->resumeExecution();
    }
}


TEST_F(Foundations_ThreadPool_Unhappy, increaseWorkers)
{
    // Case with ready thread pool, invalid increase number and unavailable capacity
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1_postpone);
        Foundations_ThreadPoolBase::testIncreaseWorkersWithInvalidIncreaseNumberAndUnavailableCapacity(threadPool, 999u, IThreadPool::State::READY);
    }

    // Case with running thread pool, invalid increase number and unavailable capacity
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1);
        Foundations_ThreadPoolBase::testIncreaseWorkersWithInvalidIncreaseNumberAndUnavailableCapacity(threadPool, 999u, IThreadPool::State::RUNNING);
    }

    // Case with paused thread pool, invalid increase number and unavailable capacity
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1);
        threadPool->pauseExecution();
        Foundations_ThreadPoolBase::testIncreaseWorkersWithInvalidIncreaseNumberAndUnavailableCapacity(threadPool, 999u, IThreadPool::State::PAUSED);
        threadPool->resumeExecution();
    }
}


TEST_F(Foundations_ThreadPool_Happy, decreaseWorkers)
{
     // Case with ready thread pool, valid decrease number and without rescheduling option
     {
         std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_2_1_3_postpone);
         Foundations_ThreadPoolBase::testDecreaseWorkersWithValidDecreaseNumber(threadPool, 1u, false, IThreadPool::State::READY);
     }

     // Case with ready thread pool, valid decrease number and with rescheduling option
     {
         std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_2_1_3_postpone);
         Foundations_ThreadPoolBase::testDecreaseWorkersWithValidDecreaseNumber(threadPool, 1u, true, IThreadPool::State::READY);
     }

     //  Case with ready thread pool, invalid decrease number but available capacity and without rescheduling
     {
         std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_2_1_3_postpone);
         Foundations_ThreadPoolBase::testDecreaseWorkersWithValidDecreaseNumber(threadPool, 999u, false, IThreadPool::State::READY);
     }

     // Case with running thread pool, valid decrease number and without rescheduling option
     {
         std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_2_1_3);
         Foundations_ThreadPoolBase::testDecreaseWorkersWithValidDecreaseNumber(threadPool, 1u, false, IThreadPool::State::RUNNING);
     }

     // Case with running thread pool, valid decrease number and with rescheduling option
     {
         std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_2_1_3);
         Foundations_ThreadPoolBase::testDecreaseWorkersWithValidDecreaseNumber(threadPool, 1u, true, IThreadPool::State::RUNNING);
     }

     //  Case with running thread pool, invalid decrease number but available capacity and without rescheduling
     {
         std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_2_1_3);
         Foundations_ThreadPoolBase::testDecreaseWorkersWithValidDecreaseNumber(threadPool, 999u, false, IThreadPool::State::RUNNING);
     }

     // Case with paused thread pool, valid decrease number and without rescheduling option
     {
         std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_2_1_3);
         threadPool->pauseExecution();
         Foundations_ThreadPoolBase::testDecreaseWorkersWithValidDecreaseNumber(threadPool, 1u, false, IThreadPool::State::PAUSED);
         threadPool->clearAllTasks(true);
         threadPool->resumeExecution();
     }

     // Case with paused thread pool, valid decrease number and with rescheduling option
     {
         std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_2_1_3);
         threadPool->pauseExecution();
         Foundations_ThreadPoolBase::testDecreaseWorkersWithValidDecreaseNumber(threadPool, 1u, true, IThreadPool::State::PAUSED);
         threadPool->clearAllTasks(true);
         threadPool->resumeExecution();
     }

     ////  Case with paused thread pool, invalid decrease number but available capacity and without rescheduling
     //{
     //    std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_2_1_3);
     //    threadPool->pauseExecution();
     //    Foundations_ThreadPoolBase::testDecreaseWorkersWithValidDecreaseNumber(threadPool, 999u, false, IThreadPool::State::PAUSED);
     //    threadPool->clearAllTasks(true);
     //    threadPool->resumeExecution();
     //}

    // Case with valid decrease and both running waiting workers
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_2_3_1);
        Foundations_ThreadPoolBase::testDecreaseWorkersWithRunningAndWaitingWorkers(threadPool);
    }
}


TEST_F(Foundations_ThreadPool_Unhappy, decreaseWorkers)
{
    // Case with ready thread pool, invalid decrease number and unavailable capacity
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1_postpone);
        Foundations_ThreadPoolBase::testDecreaseWorkersWithInvalidDecreaseNumberAndUnavailableCapacity(threadPool, 999u, IThreadPool::State::READY);
    }

    // Case with running thread pool, invalid decrease number and unavailable capacity
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1);
        Foundations_ThreadPoolBase::testDecreaseWorkersWithInvalidDecreaseNumberAndUnavailableCapacity(threadPool, 999u, IThreadPool::State::RUNNING);
    }

    // Case with paused thread pool, invalid decrease number and unavailable capacity
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1);
        threadPool->pauseExecution();
        Foundations_ThreadPoolBase::testDecreaseWorkersWithInvalidDecreaseNumberAndUnavailableCapacity(threadPool, 999u, IThreadPool::State::PAUSED);
        threadPool->resumeExecution();
    }
}


TEST_F(Foundations_ThreadPool_Happy, addTask)
{
    // Case with ready thread pool and valid task
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1_postpone);
        Foundations_ThreadPoolBase::testAddTaskWithValidTask(threadPool, IThreadPool::State::READY);
    }

    // Case with running thread pool and valid task
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1);
        Foundations_ThreadPoolBase::testAddTaskWithValidTask(threadPool, IThreadPool::State::RUNNING);
    }

    // Case with paused thread pool and valid task
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1);
        threadPool->pauseExecution();
        Foundations_ThreadPoolBase::testAddTaskWithValidTask(threadPool, IThreadPool::State::PAUSED);
        threadPool->clearAllTasks(true);
        threadPool->resumeExecution();
    }
}


TEST_F(Foundations_ThreadPool_Unhappy, addTask)
{
    // Case with nullptr task
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1_postpone);
        Foundations_ThreadPoolBase::testAddTaskWithInvalidTask(threadPool);
    }
}


TEST_F(Foundations_ThreadPool_Happy, addTaskToEveryWorker)
{
    // Case with ready thread pool, valid task and existing workers
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1_postpone);
        Foundations_ThreadPoolBase::testAddTaskToEveryWorkerWithValidTask(threadPool, IThreadPool::State::READY);
    }

    // Case with running thread pool, valid task and existing workers
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1);
        Foundations_ThreadPoolBase::testAddTaskToEveryWorkerWithValidTask(threadPool, IThreadPool::State::RUNNING);
    }

    // Case with paused thread pool, valid task and existing workers
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1);
        threadPool->pauseExecution();
        Foundations_ThreadPoolBase::testAddTaskToEveryWorkerWithValidTask(threadPool, IThreadPool::State::PAUSED);
        threadPool->clearAllTasks(true);
        threadPool->resumeExecution();
    }
}


TEST_F(Foundations_ThreadPool_Unhappy, addTaskToEveryWorker)
{
    // Case with nullptr task
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1_postpone);
        Foundations_ThreadPoolBase::testAddTaskToEveryWorkerWithInvalidTask(threadPool);
    }

    // Case with empty workers
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_0_0_0_postpone);
        Foundations_ThreadPoolBase::testAddTaskToEveryWorkerWithEmptyWorkers(threadPool);
    }
}


TEST_F(Foundations_ThreadPool_Happy, removeOneTask)
{
    // Case with ready thread pool
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1_postpone);
        Foundations_ThreadPoolBase::testRemoveOneTaskWithAvailableTasks(threadPool, IThreadPool::State::READY);
    }

    // Case with running thread pool and waiting for execution tasks
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1);
        Foundations_ThreadPoolBase::testRemoveOneTaskWithAvailableTasks(threadPool, IThreadPool::State::RUNNING);
    }

    // Case with paused thread pool
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1);
        threadPool->pauseExecution();
        Foundations_ThreadPoolBase::testRemoveOneTaskWithAvailableTasks(threadPool, IThreadPool::State::PAUSED);
        threadPool->clearAllTasks(true);
        threadPool->resumeExecution();
    }
}


TEST_F(Foundations_ThreadPool_Unhappy, removeOneTask)
{
    // Case with unavailable tasks
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1_postpone);
        Foundations_ThreadPoolBase::testRemoveOneTaskWithUnavailableTasks(threadPool);
    }
}


TEST_F(Foundations_ThreadPool_Happy, removeAllTasks)
{
    // Case with ready thread pool
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1_postpone);
        Foundations_ThreadPoolBase::testRemoveAllTasksWithAvailableTasks(threadPool, IThreadPool::State::READY);
    }

    // Case with running thread pool and waiting for execution tasks
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1);
        Foundations_ThreadPoolBase::testRemoveAllTasksWithAvailableTasks(threadPool, IThreadPool::State::RUNNING);
    }

    // Case with paused thread pool
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1);
        threadPool->pauseExecution();
        Foundations_ThreadPoolBase::testRemoveAllTasksWithAvailableTasks(threadPool, IThreadPool::State::PAUSED);
        threadPool->clearAllTasks(true);
        threadPool->resumeExecution();
    }

    // Case with removing from workers
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1_postpone);
        Foundations_ThreadPoolBase::testRemoveAllTasksWithRemovingFromWorkersOption(threadPool);
    }
}


TEST_F(Foundations_ThreadPool_Unhappy, removeAllTasks)
{
    // Case with unavailable tasks
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1_postpone);
        Foundations_ThreadPoolBase::testRemoveAllTasksWithUnavailableTasks(threadPool);
    }
}


TEST_F(Foundations_ThreadPool_Happy, clearAllTasks)
{
    // Case with ready thread pool
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1_postpone);
        Foundations_ThreadPoolBase::testClearAllTasksWithAvailableTasks(threadPool, IThreadPool::State::READY);
    }

    // Case with running thread pool and waiting for execution tasks
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1);
        Foundations_ThreadPoolBase::testClearAllTasksWithAvailableTasks(threadPool, IThreadPool::State::RUNNING);
    }

    // Case with paused thread pool
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1);
        threadPool->pauseExecution();
        Foundations_ThreadPoolBase::testClearAllTasksWithAvailableTasks(threadPool, IThreadPool::State::PAUSED);
        threadPool->clearAllTasks(true);
        threadPool->resumeExecution();
    }

    // Case with clearing from workers
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1_postpone);
        Foundations_ThreadPoolBase::testClearAllTasksWithClearingFromWorkersOption(threadPool);
    }
}


TEST_F(Foundations_ThreadPool_Unhappy, clearAllTasks)
{
    // Case with unavailable tasks
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1_postpone);
        Foundations_ThreadPoolBase::testClearAllTasksWithUnavailableTasks(threadPool);
    }
}


TEST_F(Foundations_ThreadPool_Happy, startExecution)
{
    // Case with ready thread pool
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1_postpone);
        Foundations_ThreadPoolBase::testStartExecution(threadPool, IThreadPool::State::READY);
    }
}


TEST_F(Foundations_ThreadPool_Unhappy, startExecution)
{
    // Case with running thread pool
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1);
        Foundations_ThreadPoolBase::testStartExecution(threadPool, IThreadPool::State::RUNNING);
    }

    // Case with paused thread pool
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1);
        threadPool->pauseExecution();
        Foundations_ThreadPoolBase::testStartExecution(threadPool, IThreadPool::State::PAUSED);
        threadPool->resumeExecution();
    }
}


TEST_F(Foundations_ThreadPool_Happy, pauseExecution)
{
    // Case with running thread pool
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1);
        Foundations_ThreadPoolBase::testPauseExecution(threadPool, IThreadPool::State::RUNNING);
    }
}


TEST_F(Foundations_ThreadPool_Unhappy, pauseExecution)
{
    // Case with ready thread pool
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1_postpone);
        Foundations_ThreadPoolBase::testPauseExecution(threadPool, IThreadPool::State::READY);
    }

    // Case with paused thread pool
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1);
        threadPool->pauseExecution();
        Foundations_ThreadPoolBase::testPauseExecution(threadPool, IThreadPool::State::PAUSED);
    }
}


TEST_F(Foundations_ThreadPool_Happy, resumeExecution)
{
    // Case with paused thread pool
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1);
        threadPool->pauseExecution();
        Foundations_ThreadPoolBase::testResumeExecution(threadPool, IThreadPool::State::PAUSED);
    }
}


TEST_F(Foundations_ThreadPool_Unhappy, resumeExecution)
{
    // Case with ready thread pool
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1_postpone);
        Foundations_ThreadPoolBase::testResumeExecution(threadPool, IThreadPool::State::READY);
    }

    // Case with running thread pool
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1);
        Foundations_ThreadPoolBase::testResumeExecution(threadPool, IThreadPool::State::RUNNING);
    }
}


TEST_F(Foundations_ThreadPool_Happy, waitAllTasksExecutionFinished)
{
    // Case with running thread pool
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1);
        Foundations_ThreadPoolBase::testWaitAllTasksExecutionFinished(threadPool, IThreadPool::State::RUNNING);
    }
}


TEST_F(Foundations_ThreadPool_Unhappy, waitAllTasksExecutionFinished)
{
    // Case with ready thread pool
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1_postpone);
        Foundations_ThreadPoolBase::testWaitAllTasksExecutionFinished(threadPool, IThreadPool::State::READY);
    }

    // Case with paused thread pool
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_1);
        threadPool->pauseExecution();
        Foundations_ThreadPoolBase::testWaitAllTasksExecutionFinished(threadPool, IThreadPool::State::PAUSED);
        threadPool->clearAllTasks(true);
        threadPool->resumeExecution();
    }
}


//! loadBalancing is not an available function but mechanism build on top of workers and tasks in thread pool
TEST_F(Foundations_ThreadPool_Happy, DISABLED_loadBalancing)
{
    // Case with N workers and N tasks
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_2_2_2);
        threadPool->addTasks(getSubmittedTasks(2u, 1000000u));

        OSAL::Thread::delay(inTestDelayInMicroseconds); // Let the workers get tasks for execution

        const IThreadPool::Statistic statistic{ threadPool->getStatistic() };

        EXPECT_EQ(statistic.numberOfWorkersInRunningState, 2u);
        EXPECT_EQ(threadPool->getTasksSize(false), 0u);
    }

    // Case with N workers and N+1 tasks
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_2_2_2);
        threadPool->addTasks(getSubmittedTasks(3u, 1000000u));

        OSAL::Thread::delay(inTestDelayInMicroseconds); // Let the workers get tasks for execution

        const IThreadPool::Statistic statistic{ threadPool->getStatistic() };

        EXPECT_EQ(statistic.numberOfWorkersInRunningState, 2u);
        EXPECT_EQ(threadPool->getTasksSize(false), 0u);
    }

    // Case with N workers and N-1 tasks
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_2_2_2);
        threadPool->addTasks(getSubmittedTasks(1u, 1000000u));

        OSAL::Thread::delay(inTestDelayInMicroseconds); // Let the workers get tasks for execution

        const IThreadPool::Statistic statistic{ threadPool->getStatistic() };

        EXPECT_EQ(statistic.numberOfWorkersInRunningState, 1u);
        EXPECT_EQ(statistic.numberOfWorkersInWaitingState, 1u);
        EXPECT_EQ(threadPool->getTasksSize(false), 0u);
    }

    // Case with N workers and 2N tasks
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_2_2_2);
        threadPool->addTasks(getSubmittedTasks(4u, 1000000u));

        OSAL::Thread::delay(inTestDelayInMicroseconds); // Let the workers get tasks for execution

        const IThreadPool::Statistic statistic{ threadPool->getStatistic() };

        EXPECT_EQ(statistic.numberOfWorkersInRunningState, 2u);
        EXPECT_EQ(threadPool->getTasksSize(false), 0u);
    }

    // Case with dynamically increased workers
    {
        std::shared_ptr<IThreadPool> threadPool = std::make_shared<ThreadPool>(options_1_1_3);
        threadPool->addTasks(getSubmittedTasks(3u, 3000000u));

        OSAL::Thread::delay(inTestDelayInMicroseconds); // Let the workers get tasks for execution

        IThreadPool::Statistic statistic{ threadPool->getStatistic() };

        EXPECT_EQ(statistic.numberOfWorkersInRunningState, 1u);
        EXPECT_EQ(threadPool->getTasksSize(false), 0u);

        threadPool->increaseWorkers();
        OSAL::Thread::delay(inTestDelayInMicroseconds); // Let the workers get tasks for execution

        statistic = threadPool->getStatistic();

        EXPECT_EQ(statistic.numberOfWorkersInRunningState, 2u);
        EXPECT_EQ(threadPool->getTasksSize(false), 0u);

        threadPool->increaseWorkers();
        OSAL::Thread::delay(inTestDelayInMicroseconds); // Let the workers get tasks for execution

        statistic = threadPool->getStatistic();

        EXPECT_EQ(statistic.numberOfWorkersInRunningState, 2u);
        EXPECT_EQ(statistic.numberOfWorkersInWaitingState, 1u);
        EXPECT_EQ(threadPool->getTasksSize(false), 0u);

        threadPool->addTask(getSubmittedTask(500000u));
        OSAL::Thread::delay(inTestDelayInMicroseconds); // Let the workers get tasks for execution

        statistic = threadPool->getStatistic();

        EXPECT_EQ(statistic.numberOfWorkersInRunningState, 3u);
        EXPECT_EQ(threadPool->getTasksSize(false), 0u);
    }
}

