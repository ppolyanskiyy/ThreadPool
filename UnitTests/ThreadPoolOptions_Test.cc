#include "gtest/gtest.h"
#include "ThreadPoolOptions.h"


class Foundations_ThreadPoolThreadPoolOptions_Happy : public ::testing::Test
{
};

class Foundations_ThreadPoolThreadPoolOptions_Unhappy : public ::testing::Test
{
};


TEST_F(Foundations_ThreadPoolThreadPoolOptions_Happy, constructor)
{
    // Case with valid number of workers
    {
        ThreadPoolOptions options{ 20u, 10u, 30u };

        EXPECT_EQ(options.getInitialNumberOfWorkers(), 20u);
        EXPECT_EQ(options.getMinNumberOfWorkers(), 10u);
        EXPECT_EQ(options.getMaxNumberOfWorkers(), 30u);
    }
}


TEST_F(Foundations_ThreadPoolThreadPoolOptions_Unhappy, constructor)
{
    // Case with min number of workers grater than initial
    {
        ThreadPoolOptions options{ 20u, 999u, 30u };

        EXPECT_EQ(options.getInitialNumberOfWorkers(), 20u);
        EXPECT_EQ(options.getMinNumberOfWorkers(), 20u);
        EXPECT_EQ(options.getMaxNumberOfWorkers(), 30u);
    }

    // Case with max number of workers less than initial
    {
        ThreadPoolOptions options{ 20u, 10u, 0u };

        EXPECT_EQ(options.getInitialNumberOfWorkers(), 20u);
        EXPECT_EQ(options.getMinNumberOfWorkers(), 10u);
        EXPECT_EQ(options.getMaxNumberOfWorkers(), 20u);
    }
}


TEST_F(Foundations_ThreadPoolThreadPoolOptions_Happy, setSchedulerType)
{
    // Case with valid scheduler type
    {
        ThreadPoolOptions options{};
        options.setSchedulerType(ThreadPoolOptions::SchedulerType::PRIORITY);

        EXPECT_EQ(options.getSchedulerType(), ThreadPoolOptions::SchedulerType::PRIORITY);
    }
}


TEST_F(Foundations_ThreadPoolThreadPoolOptions_Unhappy, setSchedulerType)
{
    // Case with undefined scheduler type
    {
        ThreadPoolOptions options{};
        options.setSchedulerType(ThreadPoolOptions::SchedulerType::UNDEFINED);

        EXPECT_EQ(options.getSchedulerType(), ThreadPoolOptions::SchedulerType::UNDEFINED);
    }
}


TEST_F(Foundations_ThreadPoolThreadPoolOptions_Happy, setMinNumberOfWorkers)
{
    // Case with valid value, which is less than previous min number of workers
    {
        ThreadPoolOptions options{ 20u, 10u, 30u };
        options.setMinNumberOfWorkers(5u);

        EXPECT_EQ(options.getMinNumberOfWorkers(), 5u);
    }

    // Case with valid value, which is grater than previous min number of workers
    {
        ThreadPoolOptions options{ 20u, 10u, 30u };
        options.setMinNumberOfWorkers(15u);

        EXPECT_EQ(options.getMinNumberOfWorkers(), 15u);
    }
}


TEST_F(Foundations_ThreadPoolThreadPoolOptions_Unhappy, setMinNumberOfWorkers)
{
    // Case with invalid value, which is grater than initial number of workers
    {
        ThreadPoolOptions options{ 20u, 10u, 30u };
        options.setMinNumberOfWorkers(25u);

        EXPECT_EQ(options.getMinNumberOfWorkers(), 20u);
    }
}


TEST_F(Foundations_ThreadPoolThreadPoolOptions_Happy, setMaxNumberOfWorkers)
{
    // Case with valid value, which is less than previous max number of workers
    {
        ThreadPoolOptions options{ 20u, 10u, 30u };
        options.setMaxNumberOfWorkers(25u);

        EXPECT_EQ(options.getMaxNumberOfWorkers(), 25u);
    }

    // Case with valid value, which is grater than previous max number of workers
    {
        ThreadPoolOptions options{ 20u, 10u, 30u };
        options.setMaxNumberOfWorkers(35u);

        EXPECT_EQ(options.getMaxNumberOfWorkers(), 35u);
    }
}


TEST_F(Foundations_ThreadPoolThreadPoolOptions_Unhappy, setMaxNumberOfWorkers)
{
    // Case with invalid value, which is less than initial number of workers
    {
        ThreadPoolOptions options{ 20u, 10u, 30u };
        options.setMaxNumberOfWorkers(15u);

        EXPECT_EQ(options.getMaxNumberOfWorkers(), 20u);
    }
}


TEST_F(Foundations_ThreadPoolThreadPoolOptions_Happy, setInitialNumberOfWorkers)
{
    // Case with valid value, which is grater than previous initial number of workers
    {
        ThreadPoolOptions options{ 20u, 10u, 30u };
        options.setInitialNumberOfWorkers(25u);

        EXPECT_EQ(options.getInitialNumberOfWorkers(), 25u);
    }

    // Case with valid value, which is less than previous initial number of workers
    {
        ThreadPoolOptions options{ 20u, 10u, 30u };
        options.setInitialNumberOfWorkers(15u);

        EXPECT_EQ(options.getInitialNumberOfWorkers(), 15u);
    }
}


TEST_F(Foundations_ThreadPoolThreadPoolOptions_Unhappy, setInitialNumberOfWorkers)
{
    // Case with invalid value, which is less than min number of workers
    {
        ThreadPoolOptions options{ 20u, 10u, 30u };
        options.setInitialNumberOfWorkers(5u);

        EXPECT_EQ(options.getInitialNumberOfWorkers(), 10u);
    }

    // Case with invalid value, which is grater than max number of workers
    {
        ThreadPoolOptions options{ 20u, 10u, 30u };
        options.setInitialNumberOfWorkers(35u);

        EXPECT_EQ(options.getInitialNumberOfWorkers(), 30u);
    }
}


TEST_F(Foundations_ThreadPoolThreadPoolOptions_Happy, setPostponeExecution)
{
    // Case with true value
    {
        ThreadPoolOptions options{};
        options.setPostponeExecution(true);

        EXPECT_EQ(options.needsPostponeExecution(), true);
    }

    // Case with false value
    {
        ThreadPoolOptions options{};
        options.setPostponeExecution(false);

        EXPECT_EQ(options.needsPostponeExecution(), false);
    }
}


TEST_F(Foundations_ThreadPoolThreadPoolOptions_Unhappy, setPostponeExecution)
{
    // Nothing to test for now
}


TEST_F(Foundations_ThreadPoolThreadPoolOptions_Happy, setWaitAllTasksExecutionFinished)
{
    // Case with true value
    {
        ThreadPoolOptions options{};
        options.setWaitAllTasksExecutionFinished(true);

        EXPECT_EQ(options.needsWaitAllTasksExecutionFinished(), true);
    }

    // Case with false value
    {
        ThreadPoolOptions options{};
        options.setWaitAllTasksExecutionFinished(false);

        EXPECT_EQ(options.needsWaitAllTasksExecutionFinished(), false);
    }
}


TEST_F(Foundations_ThreadPoolThreadPoolOptions_Unhappy, setWaitAllTasksExecutionFinished)
{
    // Nothing to test for now
}