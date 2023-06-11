#include "gtest/gtest.h"
#include "PriorityTask.h"
#include "BurstTimeTask.h"


class Foundations_ThreadPoolTask : public ::testing::Test
{
protected: // execute

    void testExecution(ThreadPoolTask * const task)
    {
        task->submitOne([]{});

        const Result result = task->execute();

        EXPECT_EQ(result, Result::OK);
        EXPECT_EQ(task->getState(), IThreadPoolTask::State::EXECUTED);
    }

    void testCanceledExecution(ThreadPoolTask * const task)
    {
        task->submitOne([]{});
        task->cancel();

        const Result result = task->execute();

        EXPECT_EQ(result, Result::CANCELED);
        EXPECT_EQ(task->getState(), IThreadPoolTask::State::CANCELED);
    }

    void testNotSubmittedExecution(IThreadPoolTask * const task)
    {
        const Result result = task->execute();

        EXPECT_EQ(result, Result::ERROR);
        EXPECT_EQ(task->getState(), IThreadPoolTask::State::CREATED);
    }

    void testExecutionResult(ThreadPoolTask * const task)
    {
        const uint32_t returnResult = 1;
        std::future<uint32_t> future = task->submitOne([&]{return returnResult;});
        task->execute();

        EXPECT_EQ(future.get(), returnResult);
    }


protected: // cancel

    void testCancelation(IThreadPoolTask * const task)
    {
        const Result result = task->cancel();

        EXPECT_EQ(result, Result::OK);
        EXPECT_EQ(task->getState(), IThreadPoolTask::State::CANCELED);
    }

    void testDoubleCancelation(IThreadPoolTask * const task)
    {
        testCancelation(task);

        const Result result = task->cancel();

        EXPECT_EQ(result, Result::CANCELED);
        EXPECT_EQ(task->getState(), IThreadPoolTask::State::CANCELED);
    }


protected: // submitOne

    void testSubmission(ThreadPoolTask * const task)
    {
        const std::future<void> future = task->submitOne([]{});

        EXPECT_TRUE(future.valid());
        EXPECT_EQ(task->getState(), IThreadPoolTask::State::SUBMITTED);
    }

    void testResubmissionWithExecution(ThreadPoolTask * const task)
    {
        const auto testsubmitOne = [&](uint32_t returnResult)
        {
            std::future<uint32_t> future = task->submitOne([&]{return returnResult;});
            task->execute();

            EXPECT_TRUE(future.valid());
            EXPECT_EQ(future.get(), returnResult);
        };

        for (uint32_t i = 0; i < 5; ++i)
        {
            testsubmitOne(i);
        }
    }

    void testResubmissionInARow(ThreadPoolTask * const task)
    {
        task->submitOne([]{});
        const uint32_t returnResult = 1;
        std::future<uint32_t> future2 = task->submitOne([&]{return returnResult;});
        task->execute();

        ASSERT_TRUE(future2.valid());
        ASSERT_EQ(future2.wait_for(std::chrono::seconds(0)), std::future_status::ready);
        EXPECT_EQ(future2.get(), returnResult);
    }

    void testNotExecutedSubmission(ThreadPoolTask * const task)
    {
        const std::future<void> future = task->submitOne([]{});

        ASSERT_TRUE(future.valid());
        EXPECT_EQ(future.wait_for(std::chrono::seconds(1)), std::future_status::timeout);
    }
};


class Foundations_ThreadPoolPriorityTask_Happy : public Foundations_ThreadPoolTask
{
};


class Foundations_ThreadPoolPriorityTask_Unhappy : public Foundations_ThreadPoolTask
{
};


class Foundations_ThreadPoolBurstTimeTask_Happy : public Foundations_ThreadPoolTask
{
};


class Foundations_ThreadPoolBurstTimeTask_Unhappy : public Foundations_ThreadPoolTask
{
};


// //////////////////////////////////////////////////////////////////// PriorityTask

TEST_F(Foundations_ThreadPoolPriorityTask_Happy, execute)
{
    // Case with double execution
    {
        PriorityTask task;

        testExecution(&task);
        testExecution(&task);
    }

    // Case with canceled execution
    {
        PriorityTask task;

        testCanceledExecution(&task);
    }

    // Case with execution result
    {
        PriorityTask task;

        testExecutionResult(&task);
    }
}


TEST_F(Foundations_ThreadPoolPriorityTask_Unhappy, execute)
{
    // Case with not sumbitted execution
    PriorityTask task;

    testNotSubmittedExecution(&task);
}


TEST_F(Foundations_ThreadPoolPriorityTask_Happy, cancel)
{
    // Case with single cancelation
    PriorityTask task;

    testCancelation(&task);
}


TEST_F(Foundations_ThreadPoolPriorityTask_Unhappy, cancel)
{
    // Case with double cancelation
    PriorityTask task;

    testDoubleCancelation(&task);
}


TEST_F(Foundations_ThreadPoolPriorityTask_Happy, submitOne)
{
    // Case with default submission
    {
        PriorityTask task;

        testSubmission(&task);
    }

    //  Case with resubmitted execution
    {
        PriorityTask task;

        testResubmissionWithExecution(&task);
    }
}


TEST_F(Foundations_ThreadPoolPriorityTask_Unhappy, submitOne)
{
    // Case with unexecuted submission
    {
        PriorityTask task;

        testNotExecutedSubmission(&task);
    }

    //  Case with resubmitted execution in a row
    {
        PriorityTask task;

        testResubmissionInARow(&task);
    }
}


TEST_F(Foundations_ThreadPoolPriorityTask_Happy, getId)
{
    PriorityTask task1;
    PriorityTask task2;

    EXPECT_NE(task1.getId(), task2.getId());
}

//////////////////////////////////////////////////////////////////// BurstTimeTask

TEST_F(Foundations_ThreadPoolBurstTimeTask_Happy, execute)
{
    // Case with double execution
    {
        BurstTimeTask task;

        testExecution(&task);
        testExecution(&task);
    }

    // Case with canceled execution
    {
        BurstTimeTask task;

        testCanceledExecution(&task);
    }

    // Case with execution result
    {
        BurstTimeTask task;

        testExecutionResult(&task);
    }
}


TEST_F(Foundations_ThreadPoolBurstTimeTask_Unhappy, execute)
{
    // Case with not submitted execution
    BurstTimeTask task;

    testNotSubmittedExecution(&task);
}


TEST_F(Foundations_ThreadPoolBurstTimeTask_Happy, cancel)
{
    // Case with single cancelation
    BurstTimeTask task;

    testCancelation(&task);
}


TEST_F(Foundations_ThreadPoolBurstTimeTask_Unhappy, cancel)
{
    // Case with double cancelation
    BurstTimeTask task;

    testDoubleCancelation(&task);
}


TEST_F(Foundations_ThreadPoolBurstTimeTask_Happy, submitOne)
{
    // Case with default submission
    {
        BurstTimeTask task;

        testSubmission(&task);
    }

    //  Case with resubmitted execution
    {
        BurstTimeTask task;

        testResubmissionWithExecution(&task);
    }
}


TEST_F(Foundations_ThreadPoolBurstTimeTask_Unhappy, submitOne)
{
    // Case with unexecuted submission
    {
        BurstTimeTask task;

        testNotExecutedSubmission(&task);
    }

    //  Case with resubmitted execution in a row
    {
        BurstTimeTask task;

        testResubmissionInARow(&task);
    }
}


TEST_F(Foundations_ThreadPoolBurstTimeTask_Happy, getId)
{
    BurstTimeTask task1;
    BurstTimeTask task2;

    EXPECT_NE(task1.getId(), task2.getId());
}