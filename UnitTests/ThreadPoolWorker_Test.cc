#include "gtest/gtest.h"
#include "ShortestJobFirstTaskScheduler.h"
#include "PriorityTaskScheduler.h"
#include "ThreadPoolWorker.h"


class TestTask : public ThreadPoolTask
{
};

class TestThread: public OSAL::Thread
{
public:
    TestThread(std::function<void()> && testFunction): testFunction_{testFunction} {}

private:
    void run() override { testFunction_(); }

private:
    std::function<void()> testFunction_;
};


class ExtendedThreadPoolWorkerTest : public ::testing::Test
{
public:
    const uint32_t inTestDelayInMicroseconds = 500000ul;
    const uint32_t inTaskDelayInMicroseconds = 1000000ul;
    const uint32_t waitForResultTimeoutInMicroseconds = inTaskDelayInMicroseconds + 1000000ul;

public:
    void EXPECT_ONE_EXECUTED_TASK(const  std::shared_ptr<ThreadPoolWorker> & worker, const std::shared_ptr<IThreadPoolTask> & task, const std::future<bool> & future)
    {
        EXPECT_EQ(worker->getState(), ThreadPoolWorker::State::RUNNING);
        EXPECT_EQ(task->getState(), TestTask::State::IN_EXECUTION);
        ASSERT_TRUE(future.valid());
        ASSERT_EQ(future.wait_for(std::chrono::microseconds(waitForResultTimeoutInMicroseconds)), std::future_status::ready);
    }

    void EXPECT_NOT_EXECUTED_TASK(const  std::shared_ptr<ThreadPoolWorker> & worker, const std::shared_ptr<IThreadPoolTask> & task, const std::future<bool> & future)
    {
        EXPECT_EQ(worker->getState(), ThreadPoolWorker::State::READY);
        EXPECT_EQ(task->getState(), TestTask::State::SUBMITTED);
        ASSERT_TRUE(future.valid());
        ASSERT_EQ(future.wait_for(std::chrono::microseconds(waitForResultTimeoutInMicroseconds)), std::future_status::timeout);
    }

    void EXPECT_TASKS_SAME(const std::vector<std::shared_ptr<IThreadPoolTask>> & tasks, const std::shared_ptr<IThreadPoolTask> & sourceTask)
    {
        EXPECT_FALSE(tasks.empty());

        const uint64_t taskId = sourceTask->getId();

        for(auto && taskIt: tasks)
        {
            EXPECT_EQ(taskIt->getId(), taskId);
        }
    }
};


class Foundations_ThreadPoolWorkerBase: public ExtendedThreadPoolWorkerTest
{
public:
    OSAL::Monitor dummyFreeStateMonitor;

    const uint32_t testRepetitions = 2ul;
    const std::function<bool()> testFunctionWithDelay{ [&]{ OSAL::Thread::delay(inTaskDelayInMicroseconds); return true; } };

protected:
    std::shared_ptr<TestTask> getSubmittedTask()
    {
        std::shared_ptr<TestTask> task = std::make_shared<TestTask>();
        task->submitOne(testFunctionWithDelay);

        return task;
    }

    std::vector<std::shared_ptr<IThreadPoolTask>> getSubmittedTasks(const uint32_t repetitions)
    {
        return ThreadPoolTask::submitRepeated(repetitions, testFunctionWithDelay);
    }

    std::shared_ptr<ThreadPoolWorker> getWorkerWithTask(const std::shared_ptr<IThreadPoolTask> & task, const bool needsToCreateThread = true)
    {
        std::shared_ptr<ThreadPoolWorker> worker = std::make_shared<ThreadPoolWorker>(nullptr, dummyFreeStateMonitor, nullptr);
        worker->addTask(task);

        if (needsToCreateThread)
        {
            worker->create();
            OSAL::Thread::delay(inTestDelayInMicroseconds); // Let the worker to get task for execution
        }

        return worker;
    }

    std::shared_ptr<ThreadPoolWorker> getWorkerWithTasks(const uint32_t repetitions, const bool needsToCreateThread = true)
    {
        std::shared_ptr<ThreadPoolWorker> worker = std::make_shared<ThreadPoolWorker>(nullptr, dummyFreeStateMonitor, nullptr);
        worker->addTasks(getSubmittedTasks(repetitions));

        if (needsToCreateThread)
        {
            worker->create();
            OSAL::Thread::delay(inTestDelayInMicroseconds); // Let the worker to get task for execution
        }

        return worker;
    }
};


class Foundations_ThreadPoolThreadPoolWorker_Happy: public Foundations_ThreadPoolWorkerBase
{

};

class Foundations_ThreadPoolThreadPoolWorker_Unhappy: public Foundations_ThreadPoolWorkerBase
{
};


TEST_F(Foundations_ThreadPoolThreadPoolWorker_Happy, addTask)
{
    // Case with running worker
    {
        std::shared_ptr<TestTask> task = std::make_shared<TestTask>();
        auto future = task->submitOne(testFunctionWithDelay);

        std::shared_ptr<ThreadPoolWorker> worker = getWorkerWithTask(task);

        EXPECT_ONE_EXECUTED_TASK(worker, task, future);
    }

    // Case with waiting worker
    {
        std::shared_ptr<TestTask> task = std::make_shared<TestTask>();
        auto future = task->submitOne(testFunctionWithDelay);

        std::shared_ptr<ThreadPoolWorker> worker = std::make_shared<ThreadPoolWorker>(nullptr, dummyFreeStateMonitor, nullptr);
        worker->create();

        OSAL::Thread::delay(inTestDelayInMicroseconds); // Let the worker to get task for execution

        EXPECT_EQ(worker->getState(), ThreadPoolWorker::State::WAITING);

        worker->addTask(task);
        OSAL::Thread::delay(inTestDelayInMicroseconds); // Let the worker to get task for execution
        EXPECT_ONE_EXECUTED_TASK(worker, task, future);
    }
}


TEST_F(Foundations_ThreadPoolThreadPoolWorker_Unhappy, addTask)
{
    // Case with ready worker
    {
        std::shared_ptr<TestTask> task = std::make_shared<TestTask>();
        auto future = task->submitOne(testFunctionWithDelay);

        std::shared_ptr<ThreadPoolWorker> worker = getWorkerWithTask(task, false);

        EXPECT_NOT_EXECUTED_TASK(worker, task, future);
    }
}


TEST_F(Foundations_ThreadPoolThreadPoolWorker_Happy, stealTask)
{
    // Case with ready worker
    {
        std::shared_ptr<TestTask> task = getSubmittedTask();
        std::shared_ptr<ThreadPoolWorker> worker = getWorkerWithTask(task, false);
        std::shared_ptr<IThreadPoolTask> stolenTask = worker->stealTask();

        EXPECT_NE(stolenTask, nullptr);
        EXPECT_EQ(stolenTask->getId(), task->getId());
    }
}


TEST_F(Foundations_ThreadPoolThreadPoolWorker_Unhappy, stealTask)
{
    // Case with running worker
    {
        std::shared_ptr<TestTask> task = getSubmittedTask();
        std::shared_ptr<ThreadPoolWorker> worker = getWorkerWithTask(task);
        std::shared_ptr<IThreadPoolTask> stolenTask = worker->stealTask();

        EXPECT_EQ(stolenTask, nullptr);
    }
}


TEST_F(Foundations_ThreadPoolThreadPoolWorker_Happy, removeOneTask)
{
    // Case with ready worker
    {
        std::shared_ptr<TestTask> task = getSubmittedTask();
        std::shared_ptr<ThreadPoolWorker> worker = getWorkerWithTask(task, false);
        std::shared_ptr<IThreadPoolTask> removedTask = worker->removeOneTask(task->getId());

        EXPECT_NE(removedTask, nullptr);
        EXPECT_EQ(removedTask->getId(), task->getId());
    }
}


TEST_F(Foundations_ThreadPoolThreadPoolWorker_Unhappy, removeOneTask)
{
    // Case with running worker
    {
        std::shared_ptr<TestTask> task = getSubmittedTask();
        std::shared_ptr<ThreadPoolWorker> worker = getWorkerWithTask(task);
        std::shared_ptr<IThreadPoolTask> removedTask = worker->removeOneTask(task->getId());

        EXPECT_EQ(removedTask, nullptr);
    }
}


TEST_F(Foundations_ThreadPoolThreadPoolWorker_Happy, removeAllTasks)
{
     // Case with ready worker and multiple tasks
     {
         std::shared_ptr<TestTask> task = getSubmittedTask();
         std::shared_ptr<ThreadPoolWorker> worker = getWorkerWithTask(task, false);
         std::vector<std::shared_ptr<IThreadPoolTask>> removedTasks = worker->removeAllTasks();

         EXPECT_TASKS_SAME(removedTasks, task);

     }
}


TEST_F(Foundations_ThreadPoolThreadPoolWorker_Unhappy, removeAllTasks)
{
    // Case with running worker and single task
    {
        std::shared_ptr<TestTask> task = getSubmittedTask();
        std::shared_ptr<ThreadPoolWorker> worker = getWorkerWithTask(task);
        std::vector<std::shared_ptr<IThreadPoolTask>> removedTasks = worker->removeAllTasks();

        EXPECT_TRUE(removedTasks.empty());
    }
}


TEST_F(Foundations_ThreadPoolThreadPoolWorker_Happy, clearAllTasks)
{
    // Case with ready worker and multiple tasks
    {
        std::shared_ptr<ThreadPoolWorker> worker = getWorkerWithTasks(testRepetitions, false);

        EXPECT_EQ(worker->clearAllTasks(), Result::OK);
    }

    // Case with running worker and multiple tasks
    {
        std::shared_ptr<ThreadPoolWorker> worker = getWorkerWithTasks(testRepetitions, true);

        EXPECT_EQ(worker->clearAllTasks(), Result::OK);
    }
}


TEST_F(Foundations_ThreadPoolThreadPoolWorker_Unhappy, clearAllTasks)
{
    // Case with running worker and single task
    {
        std::shared_ptr<TestTask> task = getSubmittedTask();
        std::shared_ptr<ThreadPoolWorker> worker = getWorkerWithTask(task);

        EXPECT_EQ(worker->clearAllTasks(), Result::ERROR);
    }
}


TEST_F(Foundations_ThreadPoolThreadPoolWorker_Happy, isTaskAdded)
{
    // Case with ready worker
    {
        std::shared_ptr<TestTask> task = getSubmittedTask();
        std::shared_ptr<ThreadPoolWorker> worker = getWorkerWithTask(task, false);

        EXPECT_TRUE(worker->isTaskAdded(task->getId()));
    }
}


TEST_F(Foundations_ThreadPoolThreadPoolWorker_Unhappy, isTaskAdded)
{
    // Case with running worker
    {
        std::shared_ptr<TestTask> task = getSubmittedTask();
        std::shared_ptr<ThreadPoolWorker> worker = getWorkerWithTask(task);

        EXPECT_FALSE(worker->isTaskAdded(task->getId()));
    }
}

