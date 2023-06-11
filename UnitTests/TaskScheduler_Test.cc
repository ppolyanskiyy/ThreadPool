#include "gtest/gtest.h"

#include <utility>

#include "ThreadPoolTask.h"
#include "FirstComeFirstServedTaskScheduler.h"
#include "PriorityTaskScheduler.h"
#include "ShortestJobFirstTaskScheduler.h"


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


class ExtendedTaskSchedulerTest : public ::testing::Test
{
protected: // common helpher methods for getTaskForExecution, stole, unscheduleOne

    void EXPECT_CORRECT_TASK(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        ASSERT_NE(task, nullptr);
        EXPECT_EQ(taskScheduler->getSize(), 0u);
        EXPECT_FALSE(taskScheduler->isScheduled(task->getId()));
    }

    void EXPECT_WRONG_TASK(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        EXPECT_EQ(task, nullptr);
        EXPECT_EQ(taskScheduler->getSize(), 0u);
    }


protected: // common helper methods for schedule

    void EXPECT_TASK_SCHEDULED(const Result scheduleResult, ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        EXPECT_EQ(scheduleResult, Result::OK);
        EXPECT_EQ(taskScheduler->getSize(), 1u);
        EXPECT_TRUE(taskScheduler->isScheduled(task->getId()));
    }


protected: // common helper methods for unscheduleAll, clearAll

    void EXPECT_TASKS_EMPTY(ITaskScheduler * const taskScheduler, const std::vector<std::shared_ptr<IThreadPoolTask>> & tasks)
    {
        ASSERT_TRUE(tasks.empty());
        EXPECT_EQ(taskScheduler->getSize(), 0u);
    }

    void EXPECT_TASKS_SIZES_EQ(ITaskScheduler * const taskScheduler, const std::vector<std::shared_ptr<IThreadPoolTask>> & tasks,
                               const uint32_t schedulerTasksSize, const uint32_t tasksSize)
    {
        EXPECT_EQ(taskScheduler->getSize(), static_cast<size_t>(schedulerTasksSize));
        EXPECT_EQ(tasks.size(), static_cast<std::vector<std::shared_ptr<IThreadPoolTask>>::size_type>(tasksSize));
    }

    void EXPECT_TASKS_ID_EQ(const std::vector<std::shared_ptr<IThreadPoolTask>> & tasks, const std::shared_ptr<IThreadPoolTask> & sourceTask)
    {
        for (auto && taskIt: tasks)
        {
            EXPECT_EQ(taskIt->getId(), sourceTask->getId());
        }
    }
};


class Foundations_TaskSchedulerBase : public ExtendedTaskSchedulerTest
{
public:

protected: // getTaskForExecution

    void testGetTaskForExecutionWithCorrectTask(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        taskScheduler->schedule(task);
        std::shared_ptr<IThreadPoolTask> gotTaskForExecution = taskScheduler->getTaskForExecution();

        EXPECT_CORRECT_TASK(taskScheduler, gotTaskForExecution);
        EXPECT_EQ(taskScheduler->getStatistic().totalNumberOfGotForExecutionTasks, 1u);
    }

    void testGetTaskForExecutionWithCorrectTaskDoubleCall(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        testGetTaskForExecutionWithCorrectTask(taskScheduler, task);

        std::shared_ptr<IThreadPoolTask> gotTaskForExecution = taskScheduler->getTaskForExecution();

        EXPECT_WRONG_TASK(taskScheduler, gotTaskForExecution);
        EXPECT_EQ(taskScheduler->getStatistic().totalNumberOfGotForExecutionTasks, 1u);
    }

    void testGetTaskForExecutionWithNotScheduledTask(ITaskScheduler * const taskScheduler)
    {
        std::shared_ptr<IThreadPoolTask> gotTaskForExecution = taskScheduler->getTaskForExecution();

        EXPECT_WRONG_TASK(taskScheduler, gotTaskForExecution);
        EXPECT_EQ(taskScheduler->getStatistic().totalNumberOfGotForExecutionTasks, 0u);
    }

    void testGetTaskForExecutionWithAlreadyExecutedTask(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        std::dynamic_pointer_cast<ThreadPoolTask>(task)->submitOne([]{});
        task->execute();
        testGetTaskForExecutionWithCorrectTask(taskScheduler, task);
    }

    void testGetTaskForExecutionWithAlreadyCanceledTask(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        task->cancel();
        testGetTaskForExecutionWithCorrectTask(taskScheduler, task);
    }


protected: // waitTaskForExecution

    void testWaitTaskForExecutionWithAlreadyScheduledTasks(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        taskScheduler->schedule(task);

        OSAL::Time time;
        const Result result = taskScheduler->waitTaskForExecution(1000000);
        const uint64_t realWaitTimeInMicroseconds = time.getElapsedTime();

        EXPECT_EQ(result, Result::OK);
        EXPECT_NEAR(static_cast<double>(realWaitTimeInMicroseconds), 0.0, 100000.0);
    }

    void testWaitTaskForExecutionWithTaskScheduledDuringWaiting(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        const uint32_t schedulingDelayInMicroseconds = 1000000;
        const int64_t waitTimeoutInMicroseconds = static_cast<int64_t>(schedulingDelayInMicroseconds * 2);
        const double absoluteWaitErrorInMicroseconds = 100000.0;

        TestThread schedulerThread{[&]
        {
            OSAL::Thread::delay(schedulingDelayInMicroseconds);

            taskScheduler->schedule(task);
        }};

        TestThread waiterThread{[&]
        {
            OSAL::Time time1{};
            const Result result1 = taskScheduler->waitTaskForExecution(waitTimeoutInMicroseconds);
            const uint64_t realWaitTimeInMicroseconds1 = time1.getElapsedTime();

            OSAL::Time time2{};
            const Result result2 = taskScheduler->waitTaskForExecution(waitTimeoutInMicroseconds);
            const uint64_t realWaitTimeInMicroseconds2 = time2.getElapsedTime();

            EXPECT_EQ(result1, Result::OK);
            EXPECT_NEAR(static_cast<double>(realWaitTimeInMicroseconds1), static_cast<double>(schedulingDelayInMicroseconds), absoluteWaitErrorInMicroseconds);

            EXPECT_EQ(result2, Result::OK);
            EXPECT_NEAR(static_cast<double>(realWaitTimeInMicroseconds2), 0.0, absoluteWaitErrorInMicroseconds);
        }};

        waiterThread.create();
        schedulerThread.create();

        waiterThread.waitFinished(waitTimeoutInMicroseconds);
    }

    void testWaitTaskForExecutionWithNotScheduledTask(ITaskScheduler * const taskScheduler)
    {
        const Result result = taskScheduler->waitTaskForExecution(1000000);
        EXPECT_EQ(result, Result::TIMEOUT);
    }


protected: // steal

    void testStealWithCorrectTask(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        taskScheduler->schedule(task);
        std::shared_ptr<IThreadPoolTask> stolenTask = taskScheduler->steal();

        EXPECT_CORRECT_TASK(taskScheduler, stolenTask);
        EXPECT_EQ(taskScheduler->getStatistic().totalNumberOfStolenTasks, 1u);
    }

    void testStealWithCorrectTaskDoubleCall(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        testStealWithCorrectTask(taskScheduler, task);

        std::shared_ptr<IThreadPoolTask> stolenTask = taskScheduler->steal();

        EXPECT_WRONG_TASK(taskScheduler, stolenTask);
        EXPECT_EQ(taskScheduler->getStatistic().totalNumberOfStolenTasks, 1u);
    }

    void testStealWithNotScheduledTask(ITaskScheduler * const taskScheduler)
    {
        std::shared_ptr<IThreadPoolTask> stolenTask = taskScheduler->getTaskForExecution();

        EXPECT_WRONG_TASK(taskScheduler, stolenTask);
        EXPECT_EQ(taskScheduler->getStatistic().totalNumberOfStolenTasks, 0u);
    }

    void testStealWithAlreadyExecutedTask(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        std::dynamic_pointer_cast<ThreadPoolTask>(task)->submitOne([]{});
        task->execute();
        testStealWithCorrectTask(taskScheduler, task);
    }

    void testStealWithAlreadyCanceledTask(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        task->cancel();
        testStealWithCorrectTask(taskScheduler, task);
    }


protected: // schedule

    void testScheduleWithCorrectTask(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        const Result result = taskScheduler->schedule(task);

        EXPECT_TASK_SCHEDULED(result, taskScheduler, task);
        EXPECT_EQ(taskScheduler->getStatistic().totalNumberOfScheduledTasks, 1u);
    }

    void testScheduleWithWrongTask(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        const Result result = taskScheduler->schedule(task);

        EXPECT_EQ(result, Result::ERROR);
        EXPECT_EQ(taskScheduler->getSize(), 0u);
        EXPECT_EQ(taskScheduler->getStatistic().totalNumberOfScheduledTasks, 0u);

        if (task != nullptr)
        {
            EXPECT_FALSE(taskScheduler->isScheduled(task->getId()));
        }
    }

    void testScheduleWithAlreadyExecutedTask(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        std::dynamic_pointer_cast<ThreadPoolTask>(task)->submitOne([]{});
        task->execute();
        testScheduleWithCorrectTask(taskScheduler, task);
    }

    void testScheduleWithAlreadyCanceledTask(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        task->cancel();
        testScheduleWithCorrectTask(taskScheduler, task);
    }


protected: // unscheduleOne

    void testUnscheduleOneWithCorrectTask(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        const uint64_t taskId = task->getId();

        taskScheduler->schedule(task);
        std::shared_ptr<IThreadPoolTask> unscheduledTask = taskScheduler->unscheduleOne(taskId);

        EXPECT_CORRECT_TASK(taskScheduler, unscheduledTask);
        EXPECT_EQ(taskScheduler->getStatistic().totalNumberOfUnscheduledTasks, 1u);
    }

    void testUnscheduleOneWithCorrectTaskDoubleCall(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        const uint64_t taskId = task->getId();

        testUnscheduleOneWithCorrectTask(taskScheduler, task);

        std::shared_ptr<IThreadPoolTask> unscheduledTask = taskScheduler->unscheduleOne(taskId);

        EXPECT_WRONG_TASK(taskScheduler, unscheduledTask);
        EXPECT_EQ(taskScheduler->getStatistic().totalNumberOfUnscheduledTasks, 1u);
    }

    void testUnscheduleOneWithNotScheduledTask(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        std::shared_ptr<IThreadPoolTask> unscheduledTask = taskScheduler->unscheduleOne(task->getId());

        EXPECT_WRONG_TASK(taskScheduler, unscheduledTask);
        EXPECT_EQ(taskScheduler->getStatistic().totalNumberOfUnscheduledTasks, 0u);
    }

    void testUnscheduleOneWithWrongTaskId(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        taskScheduler->schedule(task);
        std::shared_ptr<IThreadPoolTask> unscheduledTask = taskScheduler->unscheduleOne(task->getId() + 1u);

        EXPECT_EQ(unscheduledTask, nullptr);
        EXPECT_EQ(taskScheduler->getSize(), 1u);
        EXPECT_TRUE(taskScheduler->isScheduled(task->getId()));
        EXPECT_EQ(taskScheduler->getStatistic().totalNumberOfUnscheduledTasks, 0u);
    }

    void testUnscheduleOneWithAlreadyExecutedTask(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        std::dynamic_pointer_cast<ThreadPoolTask>(task)->submitOne([]{});
        task->execute();
        testUnscheduleOneWithCorrectTask(taskScheduler, task);
    }

    void testUnscheduleOneWithAlreadyCanceledTask(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        task->cancel();
        testUnscheduleOneWithCorrectTask(taskScheduler, task);
    }


protected: // unscheduleAll

    void testUnscheduleAllWithCorrectSameTasks(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        taskScheduler->schedule(task);

        std::vector<std::shared_ptr<IThreadPoolTask>> unscheduledTasks = taskScheduler->unscheduleAll();

        EXPECT_TASKS_SIZES_EQ(taskScheduler, unscheduledTasks, 0u, 1u);
        EXPECT_TASKS_ID_EQ(unscheduledTasks, task);
        EXPECT_EQ(taskScheduler->getStatistic().totalNumberOfUnscheduledTasks, 1u);
    }

    std::vector<std::shared_ptr<IThreadPoolTask>> testUnscheduleAllWithCorrectDifferentTasks(
        ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task1, const std::shared_ptr<IThreadPoolTask> & task2)
    {
        taskScheduler->schedule(task1);
        taskScheduler->schedule(task2);

        std::vector<std::shared_ptr<IThreadPoolTask>> unscheduledTasks = taskScheduler->unscheduleAll();

        EXPECT_TASKS_SIZES_EQ(taskScheduler, unscheduledTasks, 0u, 2u);
        EXPECT_EQ(taskScheduler->getStatistic().totalNumberOfUnscheduledTasks, 2u);

        return unscheduledTasks;
    }

    void testUnscheduleAllWithCorrectTasksDoubleCall(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        testUnscheduleAllWithCorrectSameTasks(taskScheduler, task);

        std::vector<std::shared_ptr<IThreadPoolTask>> unscheduledTasks = taskScheduler->unscheduleAll();

        EXPECT_TASKS_EMPTY(taskScheduler, unscheduledTasks);
        EXPECT_EQ(taskScheduler->getStatistic().totalNumberOfUnscheduledTasks, 1u);
    }

    void testUnscheduleAllWithNotScheduledTasks(ITaskScheduler * const taskScheduler)
    {
        std::vector<std::shared_ptr<IThreadPoolTask>> unscheduledTasks = taskScheduler->unscheduleAll();

        EXPECT_TASKS_EMPTY(taskScheduler, unscheduledTasks);
        EXPECT_EQ(taskScheduler->getStatistic().totalNumberOfUnscheduledTasks, 0u);
    }

    void testUnscheduleAllWithAlreadyExecutedTasks(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        std::dynamic_pointer_cast<ThreadPoolTask>(task)->submitOne([]{});
        task->execute();
        testUnscheduleAllWithCorrectSameTasks(taskScheduler, task);
    }

    void testUnscheduleAllWithAlreadyCanceledTasks(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        task->cancel();
        testUnscheduleAllWithCorrectSameTasks(taskScheduler, task);
    }


protected: // clearAll

    void testClearAllWithCorrectSameTasks(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        taskScheduler->schedule(task);

        const Result result = taskScheduler->clearAll();

        ASSERT_EQ(result, Result::OK);
        EXPECT_EQ(taskScheduler->getSize(), 0u);
        EXPECT_EQ(taskScheduler->getStatistic().totalNumberOfUnscheduledTasks, 1u);
    }

    void testClearAllWithCorrectDifferentTasks(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task1, const std::shared_ptr<IThreadPoolTask> & task2)
    {
        taskScheduler->schedule(task1);
        taskScheduler->schedule(task2);

        const Result result = taskScheduler->clearAll();

        ASSERT_EQ(result, Result::OK);
        EXPECT_EQ(taskScheduler->getSize(), 0u);
        EXPECT_EQ(taskScheduler->getStatistic().totalNumberOfUnscheduledTasks, 2u);
    }

    void testClearAllWithCorrectTasksDoubleCall(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        testClearAllWithCorrectSameTasks(taskScheduler, task);

        const Result result = taskScheduler->clearAll();

        ASSERT_EQ(result, Result::ERROR);
        EXPECT_EQ(taskScheduler->getSize(), 0u);
        EXPECT_EQ(taskScheduler->getStatistic().totalNumberOfUnscheduledTasks, 1u);
    }

    void testClearAllWithNotScheduledTasks(ITaskScheduler * const taskScheduler)
    {
        const Result result = taskScheduler->clearAll();

        ASSERT_EQ(result, Result::ERROR);
        EXPECT_EQ(taskScheduler->getSize(), 0u);
        EXPECT_EQ(taskScheduler->getStatistic().totalNumberOfUnscheduledTasks, 0u);
    }

    void testClearAllWithAlreadyExecutedTasks(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        std::dynamic_pointer_cast<ThreadPoolTask>(task)->submitOne([]{});
        task->execute();
        testClearAllWithCorrectSameTasks(taskScheduler, task);
    }

    void testClearAllWithAlreadyCanceledTasks(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        task->cancel();
        testClearAllWithCorrectSameTasks(taskScheduler, task);
    }


protected: // isScheduled

    void testIsScheduledWithCorrectTask(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        taskScheduler->schedule(task);

        EXPECT_TRUE(taskScheduler->isScheduled(task->getId()));
    }

    void testIsScheduledWithCorrectTaskDoubleCall(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        testIsScheduledWithCorrectTask(taskScheduler, task);

        EXPECT_TRUE(taskScheduler->isScheduled(task->getId()));
    }

    void testIsScheduledWithCorrectDifferentTasks(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task1, const std::shared_ptr<IThreadPoolTask> & task2)
    {
        taskScheduler->schedule(task1);
        taskScheduler->schedule(task2);

        EXPECT_TRUE(taskScheduler->isScheduled(task1->getId()));
        EXPECT_TRUE(taskScheduler->isScheduled(task2->getId()));
    }

    void testIsScheduledWithNotScheduledTask(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        EXPECT_FALSE(taskScheduler->isScheduled(task->getId()));
    }

    void testIsScheduledWithWrongTaskId(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        taskScheduler->schedule(task);

        EXPECT_FALSE(taskScheduler->isScheduled(task->getId() + 1u));
    }

    void testIsScheduledWithAlreadyExecutedTask(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        std::dynamic_pointer_cast<ThreadPoolTask>(task)->submitOne([]{});
        task->execute();
        testIsScheduledWithCorrectTask(taskScheduler, task);
    }

    void testIsScheduledWithAlreadyCanceledTask(ITaskScheduler * const taskScheduler, const std::shared_ptr<IThreadPoolTask> & task)
    {
        task->cancel();
        testIsScheduledWithCorrectTask(taskScheduler, task);
    }
};


class Foundations_ThreadPoolFirstComeFirstServedTaskScheduler_Happy: public Foundations_TaskSchedulerBase
{

};

class Foundations_ThreadPoolFirstComeFirstServedTaskScheduler_Unhappy: public Foundations_TaskSchedulerBase
{

};


class Foundations_ThreadPoolPriorityTaskScheduler_Happy: public Foundations_TaskSchedulerBase
{

public:

    enum class SchedulerPolicy : uint8_t
    {
        FIRST_LOW_SECOND_HIGH_PRIORITY,
        FIRST_HIGH_SECOND_LOW_PRIORITY,
        SAME_PRIORITIES
    };

    std::pair<std::shared_ptr<IThreadPoolTask>, std::shared_ptr<IThreadPoolTask>> scheduleTasks(PriorityTaskScheduler * const taskScheduler, const SchedulerPolicy schedulerPolicy)
    {
        std::shared_ptr<IThreadPoolTask> task1{};
        std::shared_ptr<IThreadPoolTask> task2{};

        switch (schedulerPolicy)
        {
            case SchedulerPolicy::FIRST_LOW_SECOND_HIGH_PRIORITY:
                task1.reset(new PriorityTask{ Priority::LOW });
                task2.reset(new PriorityTask{ Priority::HIGH });
                break;

            case SchedulerPolicy::FIRST_HIGH_SECOND_LOW_PRIORITY:
                task1.reset(new PriorityTask{ Priority::HIGH });
                task2.reset(new PriorityTask{ Priority::LOW });
                break;

            case SchedulerPolicy::SAME_PRIORITIES:
                task1.reset(new PriorityTask{ Priority::NORMAL });
                task2.reset(new PriorityTask{ Priority::NORMAL });
                break;

            default:
                break;
        }

        taskScheduler->schedule(task1);
        taskScheduler->schedule(task2);

        return {task1, task2};
    }


protected: // getTaskForExecution

    void testGetTaskForExecutionWithSchedulingPolicy(PriorityTaskScheduler * const taskScheduler, const SchedulerPolicy schedulerPolicy)
    {
        std::pair<std::shared_ptr<IThreadPoolTask>, std::shared_ptr<IThreadPoolTask>> scheduledTasks = scheduleTasks(taskScheduler, schedulerPolicy);

        std::shared_ptr<IThreadPoolTask> gotTaskForExecution1 = taskScheduler->getTaskForExecution();
        std::shared_ptr<IThreadPoolTask> gotTaskForExecution2 = taskScheduler->getTaskForExecution();

        EXPECT_CORRECT_TASK(taskScheduler, gotTaskForExecution1);
        EXPECT_CORRECT_TASK(taskScheduler, gotTaskForExecution2);

        switch (schedulerPolicy)
        {
            case SchedulerPolicy::FIRST_LOW_SECOND_HIGH_PRIORITY:
            EXPECT_EQ(gotTaskForExecution1->getId(), scheduledTasks.second->getId());
            EXPECT_EQ(gotTaskForExecution2->getId(), scheduledTasks.first->getId());
                break;

            case SchedulerPolicy::FIRST_HIGH_SECOND_LOW_PRIORITY:
            case SchedulerPolicy::SAME_PRIORITIES:
            EXPECT_EQ(gotTaskForExecution1->getId(), scheduledTasks.first->getId());
            EXPECT_EQ(gotTaskForExecution2->getId(), scheduledTasks.second->getId());
                break;

            default:
                return;
        }
    }

protected: // steal

    void testStealWithSchedulingPolicy(PriorityTaskScheduler * const taskScheduler, const SchedulerPolicy schedulerPolicy)
    {
        std::pair<std::shared_ptr<IThreadPoolTask>, std::shared_ptr<IThreadPoolTask>> scheduledTasks = scheduleTasks(taskScheduler, schedulerPolicy);

        std::shared_ptr<IThreadPoolTask> stolenTask1 = taskScheduler->steal();
        std::shared_ptr<IThreadPoolTask> stolenTask2 = taskScheduler->steal();

        EXPECT_CORRECT_TASK(taskScheduler, stolenTask1);
        EXPECT_CORRECT_TASK(taskScheduler, stolenTask2);

        switch (schedulerPolicy)
        {
            case SchedulerPolicy::FIRST_LOW_SECOND_HIGH_PRIORITY:
            EXPECT_EQ(stolenTask1->getId(), scheduledTasks.first->getId());
            EXPECT_EQ(stolenTask2->getId(), scheduledTasks.second->getId());
                break;

            case SchedulerPolicy::FIRST_HIGH_SECOND_LOW_PRIORITY:
            case SchedulerPolicy::SAME_PRIORITIES:
            EXPECT_EQ(stolenTask1->getId(), scheduledTasks.second->getId());
            EXPECT_EQ(stolenTask2->getId(), scheduledTasks.first->getId());
                break;

            default:
                return;
        }
    }
};

class Foundations_ThreadPoolPriorityTaskScheduler_Unhappy: public Foundations_TaskSchedulerBase
{

};


class Foundations_ThreadPoolBurstTimeTaskScheduler_Happy: public Foundations_TaskSchedulerBase
{

public:

    enum class SchedulerPolicy : uint8_t
    {
        FIRST_LONG_SECOND_SHORT_BURST_TIME,
        FIRST_SHORT_SECOND_LONG_BURST_TIME,
        SAME_BURST_TIMES
    };

    std::pair<std::shared_ptr<IThreadPoolTask>, std::shared_ptr<IThreadPoolTask>> scheduleTasks(ShortestJobFirstTaskScheduler * const taskScheduler, const SchedulerPolicy schedulerPolicy)
    {
        std::shared_ptr<IThreadPoolTask> task1{};
        std::shared_ptr<IThreadPoolTask> task2{};

        switch (schedulerPolicy)
        {
            case SchedulerPolicy::FIRST_LONG_SECOND_SHORT_BURST_TIME:
                task1.reset(new BurstTimeTask{ BurstTime::LONG });
                task2.reset(new BurstTimeTask{ BurstTime::SHORT });
                break;

            case SchedulerPolicy::FIRST_SHORT_SECOND_LONG_BURST_TIME:
                task1.reset(new BurstTimeTask{ BurstTime::SHORT });
                task2.reset(new BurstTimeTask{ BurstTime::LONG });
                break;

            case SchedulerPolicy::SAME_BURST_TIMES:
                task1.reset(new BurstTimeTask{ BurstTime::MEDIUM });
                task2.reset(new BurstTimeTask{ BurstTime::MEDIUM });
                break;

            default:
                break;
        }

        taskScheduler->schedule(task1);
        taskScheduler->schedule(task2);

        return {task1, task2};
    }


protected: // getTaskForExecution

    void testGetTaskForExecutionWithSchedulingPolicy(ShortestJobFirstTaskScheduler * const taskScheduler, const SchedulerPolicy schedulerPolicy)
    {
        std::pair<std::shared_ptr<IThreadPoolTask>, std::shared_ptr<IThreadPoolTask>> scheduledTasks = scheduleTasks(taskScheduler, schedulerPolicy);

        std::shared_ptr<IThreadPoolTask> gotTaskForExecution1 = taskScheduler->getTaskForExecution();
        std::shared_ptr<IThreadPoolTask> gotTaskForExecution2 = taskScheduler->getTaskForExecution();

        EXPECT_CORRECT_TASK(taskScheduler, gotTaskForExecution1);
        EXPECT_CORRECT_TASK(taskScheduler, gotTaskForExecution2);

        switch (schedulerPolicy)
        {
            case SchedulerPolicy::FIRST_LONG_SECOND_SHORT_BURST_TIME:
            EXPECT_EQ(gotTaskForExecution1->getId(), scheduledTasks.second->getId());
            EXPECT_EQ(gotTaskForExecution2->getId(), scheduledTasks.first->getId());
                break;

            case SchedulerPolicy::FIRST_SHORT_SECOND_LONG_BURST_TIME:
            case SchedulerPolicy::SAME_BURST_TIMES:
            EXPECT_EQ(gotTaskForExecution1->getId(), scheduledTasks.first->getId());
            EXPECT_EQ(gotTaskForExecution2->getId(), scheduledTasks.second->getId());
                break;

            default:
                return;
        }
    }

protected: // steal

    void testStealWithSchedulingPolicy(ShortestJobFirstTaskScheduler * const taskScheduler, const SchedulerPolicy schedulerPolicy)
    {
        std::pair<std::shared_ptr<IThreadPoolTask>, std::shared_ptr<IThreadPoolTask>> scheduledTasks = scheduleTasks(taskScheduler, schedulerPolicy);

        std::shared_ptr<IThreadPoolTask> stolenTask1 = taskScheduler->steal();
        std::shared_ptr<IThreadPoolTask> stolenTask2 = taskScheduler->steal();

        EXPECT_CORRECT_TASK(taskScheduler, stolenTask1);
        EXPECT_CORRECT_TASK(taskScheduler, stolenTask2);

        switch (schedulerPolicy)
        {
            case SchedulerPolicy::FIRST_LONG_SECOND_SHORT_BURST_TIME:
            EXPECT_EQ(stolenTask1->getId(), scheduledTasks.first->getId());
            EXPECT_EQ(stolenTask2->getId(), scheduledTasks.second->getId());
                break;

            case SchedulerPolicy::FIRST_SHORT_SECOND_LONG_BURST_TIME:
            case SchedulerPolicy::SAME_BURST_TIMES:
            EXPECT_EQ(stolenTask1->getId(), scheduledTasks.second->getId());
            EXPECT_EQ(stolenTask2->getId(), scheduledTasks.first->getId());
                break;

            default:
                return;
        }
    }
};

class Foundations_ThreadPoolBurstTimeTaskScheduler_Unhappy: public Foundations_TaskSchedulerBase
{

};



/////////////////////////////////////////////////////////////////////////////////////// FCFS

TEST_F(Foundations_ThreadPoolFirstComeFirstServedTaskScheduler_Happy, getTaskForExecution)
{
    // Case with correct task
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testGetTaskForExecutionWithCorrectTask(&taskScheduler, task);
    }

    // Case with correct task double call
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testGetTaskForExecutionWithCorrectTaskDoubleCall(&taskScheduler, task);
    }

    // Case with algorithm specifics
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};

        const size_t tasksSize = 3;
        std::vector<std::shared_ptr<IThreadPoolTask>> tasks;
        for (size_t i = 0; i < tasksSize; i++)
        {
            std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();
            tasks.push_back(task);
            taskScheduler.schedule(task);
        }

        for (auto && taskIt: tasks)
        {
            std::shared_ptr<IThreadPoolTask> gotTaskForExecution = taskScheduler.getTaskForExecution();

            EXPECT_NE(gotTaskForExecution, nullptr);
            EXPECT_FALSE(taskScheduler.isScheduled(gotTaskForExecution->getId()));
            EXPECT_EQ(gotTaskForExecution->getId(), taskIt->getId());
        }

        EXPECT_EQ(taskScheduler.getStatistic().totalNumberOfGotForExecutionTasks, tasksSize);
    }
}


TEST_F(Foundations_ThreadPoolFirstComeFirstServedTaskScheduler_Unhappy, getTaskForExecution)
{
    // Case with not scheduled task
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};

        Foundations_TaskSchedulerBase::testGetTaskForExecutionWithNotScheduledTask(&taskScheduler);
    }

    // Case with already executed task
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testGetTaskForExecutionWithAlreadyExecutedTask(&taskScheduler, task);
    }

    // Case with already canceled task
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testGetTaskForExecutionWithAlreadyCanceledTask(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolFirstComeFirstServedTaskScheduler_Happy, waitTaskForExecution)
{
    // Case with already scheduled tasks
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testWaitTaskForExecutionWithAlreadyScheduledTasks(&taskScheduler, task);
    }

    // Case with task scheduled during waiting
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testWaitTaskForExecutionWithTaskScheduledDuringWaiting(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolFirstComeFirstServedTaskScheduler_Unhappy, waitTaskForExecution)
{
    // Case with not scheduled task
    FirstComeFirstServedTaskScheduler taskScheduler{nullptr};

    Foundations_TaskSchedulerBase::testWaitTaskForExecutionWithNotScheduledTask(&taskScheduler);
}


TEST_F(Foundations_ThreadPoolFirstComeFirstServedTaskScheduler_Happy, steal)
{
    // Case with correct task
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testStealWithCorrectTask(&taskScheduler, task);
    }

    // Case with correct task double call
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testStealWithCorrectTaskDoubleCall(&taskScheduler, task);
    }

    // Case with algorithm specifics
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};

        const size_t tasksSize = 3;
        std::vector<std::shared_ptr<IThreadPoolTask>> tasks;
        for (size_t i = 0; i < tasksSize; i++)
        {
            std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();
            tasks.push_back(task);
            taskScheduler.schedule(task);
        }

        for(auto taskIt = tasks.crbegin(); taskIt != tasks.crend(); ++taskIt)
        {
            std::shared_ptr<IThreadPoolTask> stolenTask = taskScheduler.steal();

            EXPECT_NE(stolenTask, nullptr);
            EXPECT_FALSE(taskScheduler.isScheduled(stolenTask->getId()));
            EXPECT_EQ(stolenTask->getId(), (*taskIt)->getId());
        }

        EXPECT_EQ(taskScheduler.getStatistic().totalNumberOfStolenTasks, tasksSize);
    }
}


TEST_F(Foundations_ThreadPoolFirstComeFirstServedTaskScheduler_Unhappy, steal)
{
    // Case with not scheduled task
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};

        Foundations_TaskSchedulerBase::testStealWithNotScheduledTask(&taskScheduler);
    }

    // Case with already executed task
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testStealWithAlreadyExecutedTask(&taskScheduler, task);
    }

    // Case with already canceled task
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testStealWithAlreadyCanceledTask(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolFirstComeFirstServedTaskScheduler_Happy, schedule)
{
    // Case with correct task
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testScheduleWithCorrectTask(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolFirstComeFirstServedTaskScheduler_Unhappy, schedule)
{
    // Case with nullptr task
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};

        Foundations_TaskSchedulerBase::testScheduleWithWrongTask(&taskScheduler, nullptr);
    }

    // Case with already executed task
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testScheduleWithAlreadyExecutedTask(&taskScheduler, task);
    }

    // Case with already canceled task
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testScheduleWithAlreadyCanceledTask(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolFirstComeFirstServedTaskScheduler_Happy, unscheduleOne)
{
    // Case with correct task
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testUnscheduleOneWithCorrectTask(&taskScheduler, task);
    }

    // Case with correct task double call
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testUnscheduleOneWithCorrectTaskDoubleCall(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolFirstComeFirstServedTaskScheduler_Unhappy, unscheduleOne)
{
    // Case with not scheduled task
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testUnscheduleOneWithNotScheduledTask(&taskScheduler, task);
    }

    // Case with wrong task id
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testUnscheduleOneWithWrongTaskId(&taskScheduler, task);
    }

    // Case with already executed task
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testUnscheduleOneWithAlreadyExecutedTask(&taskScheduler, task);
    }

    // Case with already canceled task
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testUnscheduleOneWithAlreadyCanceledTask(&taskScheduler, task);
    }

}


TEST_F(Foundations_ThreadPoolFirstComeFirstServedTaskScheduler_Happy, unscheduleAll)
{
    // Case with correct same tasks
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testUnscheduleAllWithCorrectSameTasks(&taskScheduler, task);
    }

    // Case with correct different tasks
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task1 = std::make_shared<TestTask>();
        std::shared_ptr<IThreadPoolTask> task2 = std::make_shared<TestTask>();

        std::vector<std::shared_ptr<IThreadPoolTask>> unscheduledTasks =
            Foundations_TaskSchedulerBase::testUnscheduleAllWithCorrectDifferentTasks(&taskScheduler, task1, task2);

        // Algorithm specific test
        EXPECT_EQ(unscheduledTasks[0]->getId(), task1->getId());
        EXPECT_EQ(unscheduledTasks[1]->getId(), task2->getId());
    }

    // Case with double call
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testUnscheduleAllWithCorrectTasksDoubleCall(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolFirstComeFirstServedTaskScheduler_Unhappy, unscheduleAll)
{
     // Case with not scheduled tasks
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};

        Foundations_TaskSchedulerBase::testUnscheduleAllWithNotScheduledTasks(&taskScheduler);
    }

    // Case with already executed tasks
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testUnscheduleAllWithAlreadyExecutedTasks(&taskScheduler, task);
    }

    // Case with already canceled tasks
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testUnscheduleAllWithAlreadyCanceledTasks(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolFirstComeFirstServedTaskScheduler_Happy, clearAll)
{
   // Case with correct same tasks
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testClearAllWithCorrectSameTasks(&taskScheduler, task);
    }

    // Case with correct different tasks
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task1 = std::make_shared<TestTask>();
        std::shared_ptr<IThreadPoolTask> task2 = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testClearAllWithCorrectDifferentTasks(&taskScheduler, task1, task2);
    }

    // Case with double call
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testClearAllWithCorrectTasksDoubleCall(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolFirstComeFirstServedTaskScheduler_Unhappy, clearAll)
{
     // Case with not scheduled tasks
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};

        Foundations_TaskSchedulerBase::testClearAllWithNotScheduledTasks(&taskScheduler);
    }

    // Case with already executed tasks
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testClearAllWithAlreadyExecutedTasks(&taskScheduler, task);
    }

    // Case with already canceled tasks
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testClearAllWithAlreadyCanceledTasks(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolFirstComeFirstServedTaskScheduler_Happy, isScheduled)
{
    // Case with correct task
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testIsScheduledWithCorrectTask(&taskScheduler, task);
    }

    // Case with double call
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testIsScheduledWithCorrectTaskDoubleCall(&taskScheduler, task);
    }

    // Case with correct different tasks
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task1 = std::make_shared<TestTask>();
        std::shared_ptr<IThreadPoolTask> task2 = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testIsScheduledWithCorrectDifferentTasks(&taskScheduler, task1, task2);
    }
}


TEST_F(Foundations_ThreadPoolFirstComeFirstServedTaskScheduler_Unhappy, isScheduled)
{
    // Case with not scheduled task
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testIsScheduledWithNotScheduledTask(&taskScheduler, task);
    }

    // Case with wrong task id
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testIsScheduledWithWrongTaskId(&taskScheduler, task);
    }

    // Case with already executed task
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testIsScheduledWithAlreadyExecutedTask(&taskScheduler, task);
    }

    // Case with already canceled task
    {
        FirstComeFirstServedTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<TestTask>();

        Foundations_TaskSchedulerBase::testIsScheduledWithAlreadyCanceledTask(&taskScheduler, task);
    }
}




/////////////////////////////////////////////////////////////////////////////////////// PriorityTask

TEST_F(Foundations_ThreadPoolPriorityTaskScheduler_Happy, getTaskForExecution)
{
    // Case with correct task
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testGetTaskForExecutionWithCorrectTask(&taskScheduler, task);
    }

    // Case with correct task double call
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testGetTaskForExecutionWithCorrectTaskDoubleCall(&taskScheduler, task);
    }

    // Case with scheduling first lower then higher priority task
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        Foundations_ThreadPoolPriorityTaskScheduler_Happy::testGetTaskForExecutionWithSchedulingPolicy(
            &taskScheduler, Foundations_ThreadPoolPriorityTaskScheduler_Happy::SchedulerPolicy::FIRST_LOW_SECOND_HIGH_PRIORITY);
    }

    // Case with scheduling first higher then lower priority task
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        Foundations_ThreadPoolPriorityTaskScheduler_Happy::testGetTaskForExecutionWithSchedulingPolicy(
            &taskScheduler, Foundations_ThreadPoolPriorityTaskScheduler_Happy::SchedulerPolicy::FIRST_HIGH_SECOND_LOW_PRIORITY);
    }

    // Case with scheduling same priority tasks
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        Foundations_ThreadPoolPriorityTaskScheduler_Happy::testGetTaskForExecutionWithSchedulingPolicy(
            &taskScheduler, Foundations_ThreadPoolPriorityTaskScheduler_Happy::SchedulerPolicy::SAME_PRIORITIES);
    }
}


TEST_F(Foundations_ThreadPoolPriorityTaskScheduler_Unhappy, getTaskForExecution)
{
    // Case with not scheduled task
    {
        PriorityTaskScheduler taskScheduler{nullptr};

        Foundations_TaskSchedulerBase::testGetTaskForExecutionWithNotScheduledTask(&taskScheduler);
    }

    // Case with already executed task
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testGetTaskForExecutionWithAlreadyExecutedTask(&taskScheduler, task);
    }

    // Case with already canceled task
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testGetTaskForExecutionWithAlreadyCanceledTask(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolPriorityTaskScheduler_Happy, waitTaskForExecution)
{
    // Case with already scheduled tasks
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testWaitTaskForExecutionWithAlreadyScheduledTasks(&taskScheduler, task);
    }

    // Case with task scheduled during waiting
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testWaitTaskForExecutionWithTaskScheduledDuringWaiting(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolPriorityTaskScheduler_Unhappy, waitTaskForExecution)
{
    // Case with not scheduled task
    PriorityTaskScheduler taskScheduler{nullptr};

    Foundations_TaskSchedulerBase::testWaitTaskForExecutionWithNotScheduledTask(&taskScheduler);
}


TEST_F(Foundations_ThreadPoolPriorityTaskScheduler_Happy, steal)
{
    // Case with correct task
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testStealWithCorrectTask(&taskScheduler, task);
    }

    // Case with correct task double call
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testStealWithCorrectTaskDoubleCall(&taskScheduler, task);
    }

    // Case with scheduling first lower then higher priority task
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        Foundations_ThreadPoolPriorityTaskScheduler_Happy::testStealWithSchedulingPolicy(
            &taskScheduler, Foundations_ThreadPoolPriorityTaskScheduler_Happy::SchedulerPolicy::FIRST_LOW_SECOND_HIGH_PRIORITY);
    }

    // Case with scheduling first higher then lower priority task
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        Foundations_ThreadPoolPriorityTaskScheduler_Happy::testStealWithSchedulingPolicy(
            &taskScheduler, Foundations_ThreadPoolPriorityTaskScheduler_Happy::SchedulerPolicy::FIRST_HIGH_SECOND_LOW_PRIORITY);
    }

    // Case with scheduling same priority tasks
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        Foundations_ThreadPoolPriorityTaskScheduler_Happy::testStealWithSchedulingPolicy(
            &taskScheduler, Foundations_ThreadPoolPriorityTaskScheduler_Happy::SchedulerPolicy::SAME_PRIORITIES);
    }
}


TEST_F(Foundations_ThreadPoolPriorityTaskScheduler_Unhappy, steal)
{
    // Case with not scheduled task
    {
        PriorityTaskScheduler taskScheduler{nullptr};

        Foundations_TaskSchedulerBase::testStealWithNotScheduledTask(&taskScheduler);
    }

    // Case with already executed task
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testStealWithAlreadyExecutedTask(&taskScheduler, task);
    }

    // Case with already canceled task
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testStealWithAlreadyCanceledTask(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolPriorityTaskScheduler_Happy, schedule)
{
    // Case with correct task
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testScheduleWithCorrectTask(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolPriorityTaskScheduler_Unhappy, schedule)
{
    // Case with nullptr task
    {
        PriorityTaskScheduler taskScheduler{nullptr};

        Foundations_TaskSchedulerBase::testScheduleWithWrongTask(&taskScheduler, nullptr);
    }

    // Case with already executed task
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testScheduleWithAlreadyExecutedTask(&taskScheduler, task);
    }

    // Case with already canceled task
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testScheduleWithAlreadyCanceledTask(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolPriorityTaskScheduler_Happy, unscheduleOne)
{
    // Case with correct task
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testUnscheduleOneWithCorrectTask(&taskScheduler, task);
    }

    // Case with correct task double call
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testUnscheduleOneWithCorrectTaskDoubleCall(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolPriorityTaskScheduler_Unhappy, unscheduleOne)
{
    // Case with not scheduled task
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testUnscheduleOneWithNotScheduledTask(&taskScheduler, task);
    }

    // Case with wrong task id
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testUnscheduleOneWithWrongTaskId(&taskScheduler, task);
    }

    // Case with already executed task
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testUnscheduleOneWithAlreadyExecutedTask(&taskScheduler, task);
    }

    // Case with already canceled task
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testUnscheduleOneWithAlreadyCanceledTask(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolPriorityTaskScheduler_Happy, unscheduleAll)
{
    // Case with correct same tasks
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testUnscheduleAllWithCorrectSameTasks(&taskScheduler, task);
    }

    // Case with correct different tasks and same priority
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task1 = std::make_shared<PriorityTask>();
        std::shared_ptr<IThreadPoolTask> task2 = std::make_shared<PriorityTask>();

        std::vector<std::shared_ptr<IThreadPoolTask>> unscheduledTasks =
            Foundations_TaskSchedulerBase::testUnscheduleAllWithCorrectDifferentTasks(&taskScheduler, task1, task2);

        // Algorithm specific test
        EXPECT_EQ(unscheduledTasks[0]->getId(), task1->getId());
        EXPECT_EQ(unscheduledTasks[1]->getId(), task2->getId());
    }

    // Case with correct different tasks and different priority
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task1 = std::make_shared<PriorityTask>(Priority::LOW);
        std::shared_ptr<IThreadPoolTask> task2 = std::make_shared<PriorityTask>(Priority::HIGH);

        std::vector<std::shared_ptr<IThreadPoolTask>> unscheduledTasks =
            Foundations_TaskSchedulerBase::testUnscheduleAllWithCorrectDifferentTasks(&taskScheduler, task1, task2);

        // Algorithm specific test
        auto foundTaskIt1 = TaskSchedulerBase::findTaskById(unscheduledTasks.cbegin(), unscheduledTasks.cend(), task1->getId());
        auto foundTaskIt2 = TaskSchedulerBase::findTaskById(unscheduledTasks.cbegin(), unscheduledTasks.cend(), task2->getId());

        ASSERT_NE(foundTaskIt1, unscheduledTasks.cend());
        ASSERT_NE(foundTaskIt2, unscheduledTasks.cend());
    }

    // Case with double call
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testUnscheduleAllWithCorrectTasksDoubleCall(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolPriorityTaskScheduler_Unhappy, unscheduleAll)
{
     // Case with not scheduled tasks
    {
        PriorityTaskScheduler taskScheduler{nullptr};

        Foundations_TaskSchedulerBase::testUnscheduleAllWithNotScheduledTasks(&taskScheduler);
    }

    // Case with already executed tasks
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testUnscheduleAllWithAlreadyExecutedTasks(&taskScheduler, task);
    }

    // Case with already canceled tasks
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testUnscheduleAllWithAlreadyCanceledTasks(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolPriorityTaskScheduler_Happy, clearAll)
{
   // Case with correct same tasks
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testClearAllWithCorrectSameTasks(&taskScheduler, task);
    }

    // Case with correct different tasks and same priority
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task1 = std::make_shared<PriorityTask>();
        std::shared_ptr<IThreadPoolTask> task2 = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testClearAllWithCorrectDifferentTasks(&taskScheduler, task1, task2);
    }

    // Case with correct different tasks and different priority
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task1 = std::make_shared<PriorityTask>(Priority::LOW);
        std::shared_ptr<IThreadPoolTask> task2 = std::make_shared<PriorityTask>(Priority::HIGH);

        Foundations_TaskSchedulerBase::testClearAllWithCorrectDifferentTasks(&taskScheduler, task1, task2);
    }

    // Case with double call
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testClearAllWithCorrectTasksDoubleCall(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolPriorityTaskScheduler_Unhappy, clearAll)
{
     // Case with not scheduled tasks
    {
        PriorityTaskScheduler taskScheduler{nullptr};

        Foundations_TaskSchedulerBase::testClearAllWithNotScheduledTasks(&taskScheduler);
    }

    // Case with already executed tasks
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testClearAllWithAlreadyExecutedTasks(&taskScheduler, task);
    }

    // Case with already canceled tasks
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testClearAllWithAlreadyCanceledTasks(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolPriorityTaskScheduler_Happy, isScheduled)
{
    // Case with correct task
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testIsScheduledWithCorrectTask(&taskScheduler, task);
    }

    // Case with double call
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testIsScheduledWithCorrectTaskDoubleCall(&taskScheduler, task);
    }

    // Case with correct different tasks and same priority
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task1 = std::make_shared<PriorityTask>();
        std::shared_ptr<IThreadPoolTask> task2 = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testIsScheduledWithCorrectDifferentTasks(&taskScheduler, task1, task2);
    }

    // Case with correct different tasks and different priority
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task1 = std::make_shared<PriorityTask>(Priority::LOW);
        std::shared_ptr<IThreadPoolTask> task2 = std::make_shared<PriorityTask>(Priority::HIGH);

        Foundations_TaskSchedulerBase::testIsScheduledWithCorrectDifferentTasks(&taskScheduler, task1, task2);
    }
}


TEST_F(Foundations_ThreadPoolPriorityTaskScheduler_Unhappy, isScheduled)
{
    // Case with not scheduled task
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testIsScheduledWithNotScheduledTask(&taskScheduler, task);
    }

    // Case with wrong task id
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testIsScheduledWithWrongTaskId(&taskScheduler, task);
    }

    // Case with already executed task
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testIsScheduledWithAlreadyExecutedTask(&taskScheduler, task);
    }

    // Case with already canceled task
    {
        PriorityTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<PriorityTask>();

        Foundations_TaskSchedulerBase::testIsScheduledWithAlreadyCanceledTask(&taskScheduler, task);
    }
}




/////////////////////////////////////////////////////////////////////////////////////// BurstTimeTask

TEST_F(Foundations_ThreadPoolBurstTimeTaskScheduler_Happy, getTaskForExecution)
{
    // Case with correct task
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testGetTaskForExecutionWithCorrectTask(&taskScheduler, task);
    }

    // Case with correct task double call
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testGetTaskForExecutionWithCorrectTaskDoubleCall(&taskScheduler, task);
    }

    // Case with scheduling first longer then shorter burst time task
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        Foundations_ThreadPoolBurstTimeTaskScheduler_Happy::testGetTaskForExecutionWithSchedulingPolicy(
            &taskScheduler, Foundations_ThreadPoolBurstTimeTaskScheduler_Happy::SchedulerPolicy::FIRST_LONG_SECOND_SHORT_BURST_TIME);
    }

    // Case with scheduling first shorter then longer burst time task
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        Foundations_ThreadPoolBurstTimeTaskScheduler_Happy::testGetTaskForExecutionWithSchedulingPolicy(
            &taskScheduler, Foundations_ThreadPoolBurstTimeTaskScheduler_Happy::SchedulerPolicy::FIRST_SHORT_SECOND_LONG_BURST_TIME);
    }

    // Case with scheduling same burst time tasks
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        Foundations_ThreadPoolBurstTimeTaskScheduler_Happy::testGetTaskForExecutionWithSchedulingPolicy(
            &taskScheduler, Foundations_ThreadPoolBurstTimeTaskScheduler_Happy::SchedulerPolicy::SAME_BURST_TIMES);
    }
}


TEST_F(Foundations_ThreadPoolBurstTimeTaskScheduler_Unhappy, getTaskForExecution)
{
    // Case with not scheduled task
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};

        Foundations_TaskSchedulerBase::testGetTaskForExecutionWithNotScheduledTask(&taskScheduler);
    }

    // Case with already executed task
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testGetTaskForExecutionWithAlreadyExecutedTask(&taskScheduler, task);
    }

    // Case with already canceled task
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testGetTaskForExecutionWithAlreadyCanceledTask(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolBurstTimeTaskScheduler_Happy, waitTaskForExecution)
{
    // Case with already scheduled tasks
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testWaitTaskForExecutionWithAlreadyScheduledTasks(&taskScheduler, task);
    }

    // Case with task scheduled during waiting
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testWaitTaskForExecutionWithTaskScheduledDuringWaiting(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolBurstTimeTaskScheduler_Unhappy, waitTaskForExecution)
{
    // Case with not scheduled task
    ShortestJobFirstTaskScheduler taskScheduler{nullptr};

    Foundations_TaskSchedulerBase::testWaitTaskForExecutionWithNotScheduledTask(&taskScheduler);
}


TEST_F(Foundations_ThreadPoolBurstTimeTaskScheduler_Happy, steal)
{
    // Case with correct task
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testStealWithCorrectTask(&taskScheduler, task);
    }

    // Case with correct task double call
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testStealWithCorrectTaskDoubleCall(&taskScheduler, task);
    }

    // Case with scheduling first longer then shorter burst time task
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        Foundations_ThreadPoolBurstTimeTaskScheduler_Happy::testStealWithSchedulingPolicy(
            &taskScheduler, Foundations_ThreadPoolBurstTimeTaskScheduler_Happy::SchedulerPolicy::FIRST_LONG_SECOND_SHORT_BURST_TIME);
    }

    // Case with scheduling first shorter then longer burst time task
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        Foundations_ThreadPoolBurstTimeTaskScheduler_Happy::testStealWithSchedulingPolicy(
            &taskScheduler, Foundations_ThreadPoolBurstTimeTaskScheduler_Happy::SchedulerPolicy::FIRST_SHORT_SECOND_LONG_BURST_TIME);
    }

    // Case with scheduling same burst time tasks
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        Foundations_ThreadPoolBurstTimeTaskScheduler_Happy::testStealWithSchedulingPolicy(
            &taskScheduler, Foundations_ThreadPoolBurstTimeTaskScheduler_Happy::SchedulerPolicy::SAME_BURST_TIMES);
    }
}


TEST_F(Foundations_ThreadPoolBurstTimeTaskScheduler_Unhappy, steal)
{
    // Case with not scheduled task
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};

        Foundations_TaskSchedulerBase::testStealWithNotScheduledTask(&taskScheduler);
    }

    // Case with already executed task
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testStealWithAlreadyExecutedTask(&taskScheduler, task);
    }

    // Case with already canceled task
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testStealWithAlreadyCanceledTask(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolBurstTimeTaskScheduler_Happy, schedule)
{
    // Case with correct task
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testScheduleWithCorrectTask(&taskScheduler, task);
    }

    // Case with undefined burst time task
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>(BurstTime::UNDEFINED);

        Foundations_TaskSchedulerBase::testScheduleWithCorrectTask(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolBurstTimeTaskScheduler_Unhappy, schedule)
{
    // Case with nullptr task
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};

        Foundations_TaskSchedulerBase::testScheduleWithWrongTask(&taskScheduler, nullptr);
    }

    // Case with already executed task
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testScheduleWithAlreadyExecutedTask(&taskScheduler, task);
    }

    // Case with already canceled task
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testScheduleWithAlreadyCanceledTask(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolBurstTimeTaskScheduler_Happy, unscheduleOne)
{
    // Case with correct task
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testUnscheduleOneWithCorrectTask(&taskScheduler, task);
    }

    // Case with correct task double call
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testUnscheduleOneWithCorrectTaskDoubleCall(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolBurstTimeTaskScheduler_Unhappy, unscheduleOne)
{
    // Case with not scheduled task
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testUnscheduleOneWithNotScheduledTask(&taskScheduler, task);
    }

    // Case with wrong task id
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testUnscheduleOneWithWrongTaskId(&taskScheduler, task);
    }

    // Case with already executed task
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testUnscheduleOneWithAlreadyExecutedTask(&taskScheduler, task);
    }

    // Case with already canceled task
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testUnscheduleOneWithAlreadyCanceledTask(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolBurstTimeTaskScheduler_Happy, unscheduleAll)
{
    // Case with correct same tasks
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testUnscheduleAllWithCorrectSameTasks(&taskScheduler, task);
    }

    // Case with correct different tasks and same burst time
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task1 = std::make_shared<BurstTimeTask>();
        std::shared_ptr<IThreadPoolTask> task2 = std::make_shared<BurstTimeTask>();

        std::vector<std::shared_ptr<IThreadPoolTask>> unscheduledTasks =
            Foundations_TaskSchedulerBase::testUnscheduleAllWithCorrectDifferentTasks(&taskScheduler, task1, task2);

        // Algorithm specific test
        EXPECT_EQ(unscheduledTasks[0]->getId(), task1->getId());
        EXPECT_EQ(unscheduledTasks[1]->getId(), task2->getId());
    }

    // Case with correct different tasks and different burst time
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task1 = std::make_shared<BurstTimeTask>(BurstTime::LONG);
        std::shared_ptr<IThreadPoolTask> task2 = std::make_shared<BurstTimeTask>(BurstTime::SHORT);

        std::vector<std::shared_ptr<IThreadPoolTask>> unscheduledTasks =
            Foundations_TaskSchedulerBase::testUnscheduleAllWithCorrectDifferentTasks(&taskScheduler, task1, task2);

        // Algorithm specific test
        auto foundTaskIt1 = TaskSchedulerBase::findTaskById(unscheduledTasks.cbegin(), unscheduledTasks.cend(), task1->getId());
        auto foundTaskIt2 = TaskSchedulerBase::findTaskById(unscheduledTasks.cbegin(), unscheduledTasks.cend(), task2->getId());

        ASSERT_NE(foundTaskIt1, unscheduledTasks.cend());
        ASSERT_NE(foundTaskIt2, unscheduledTasks.cend());
    }

    // Case with double call
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testUnscheduleAllWithCorrectTasksDoubleCall(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolBurstTimeTaskScheduler_Unhappy, unscheduleAll)
{
     // Case with not scheduled tasks
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};

        Foundations_TaskSchedulerBase::testUnscheduleAllWithNotScheduledTasks(&taskScheduler);
    }

    // Case with already executed tasks
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testUnscheduleAllWithAlreadyExecutedTasks(&taskScheduler, task);
    }

    // Case with already canceled tasks
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testUnscheduleAllWithAlreadyCanceledTasks(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolBurstTimeTaskScheduler_Happy, clearAll)
{
   // Case with correct same tasks
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testClearAllWithCorrectSameTasks(&taskScheduler, task);
    }

    // Case with correct different tasks and same burst time
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task1 = std::make_shared<BurstTimeTask>();
        std::shared_ptr<IThreadPoolTask> task2 = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testClearAllWithCorrectDifferentTasks(&taskScheduler, task1, task2);
    }

    // Case with correct different tasks and different burst time
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task1 = std::make_shared<BurstTimeTask>(BurstTime::LONG);
        std::shared_ptr<IThreadPoolTask> task2 = std::make_shared<BurstTimeTask>(BurstTime::SHORT);

        Foundations_TaskSchedulerBase::testClearAllWithCorrectDifferentTasks(&taskScheduler, task1, task2);
    }

    // Case with double call
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testClearAllWithCorrectTasksDoubleCall(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolBurstTimeTaskScheduler_Unhappy, clearAll)
{
     // Case with not scheduled tasks
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};

        Foundations_TaskSchedulerBase::testClearAllWithNotScheduledTasks(&taskScheduler);
    }

    // Case with already executed tasks
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testClearAllWithAlreadyExecutedTasks(&taskScheduler, task);
    }

    // Case with already canceled tasks
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testClearAllWithAlreadyCanceledTasks(&taskScheduler, task);
    }
}


TEST_F(Foundations_ThreadPoolBurstTimeTaskScheduler_Happy, isScheduled)
{
    // Case with correct task
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testIsScheduledWithCorrectTask(&taskScheduler, task);
    }

    // Case with double call
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testIsScheduledWithCorrectTaskDoubleCall(&taskScheduler, task);
    }

    // Case with correct different tasks and same burst time
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task1 = std::make_shared<BurstTimeTask>();
        std::shared_ptr<IThreadPoolTask> task2 = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testIsScheduledWithCorrectDifferentTasks(&taskScheduler, task1, task2);
    }

    // Case with correct different tasks and different burst time
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task1 = std::make_shared<BurstTimeTask>(BurstTime::LONG);
        std::shared_ptr<IThreadPoolTask> task2 = std::make_shared<BurstTimeTask>(BurstTime::SHORT);

        Foundations_TaskSchedulerBase::testIsScheduledWithCorrectDifferentTasks(&taskScheduler, task1, task2);
    }
}


TEST_F(Foundations_ThreadPoolBurstTimeTaskScheduler_Unhappy, isScheduled)
{
    // Case with not scheduled task
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testIsScheduledWithNotScheduledTask(&taskScheduler, task);
    }

    // Case with wrong task id
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testIsScheduledWithWrongTaskId(&taskScheduler, task);
    }

    // Case with already executed task
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testIsScheduledWithAlreadyExecutedTask(&taskScheduler, task);
    }

    // Case with already canceled task
    {
        ShortestJobFirstTaskScheduler taskScheduler{nullptr};
        std::shared_ptr<IThreadPoolTask> task = std::make_shared<BurstTimeTask>();

        Foundations_TaskSchedulerBase::testIsScheduledWithAlreadyCanceledTask(&taskScheduler, task);
    }
}