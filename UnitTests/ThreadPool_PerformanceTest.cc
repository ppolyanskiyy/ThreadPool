#include "gtest/gtest.h"

#include <sstream>
#include <random>

#include "OSALTime.h"
#include "PriorityTaskScheduler.h"
#include "ShortestJobFirstTaskScheduler.h"
#include "ThreadPoolTask.h"
#include "ThreadPool.h"


// Returns a random number in the range [min, max].
uint32_t getRandomNumber(const uint32_t min, const uint32_t max)
{
    // Seed the PRNG with a random device to ensure a different seed every time.
    std::random_device rd;
    std::mt19937 gen(rd());

    // Define a distribution that produces integers in the range [min, max].
    std::uniform_int_distribution<uint32_t> dist(min, max);

    // Generate a random number using the distribution and return it.
    return dist(gen);
}


class TestTask : public ThreadPoolTask
{
};


class Foundations_ThreadPool_Performance_Base : public ::testing::Test
{
public: // Own version

    using TasksContainer = std::vector<std::shared_ptr<IThreadPoolTask>>;

    static const uint32_t   TEST_NUMBER_OF_TASKS                { 100u };
    static const uint32_t   TEST_NUMBER_OF_WORKERS              { 4u };
    static const uint32_t   TEST_DELAY_IN_MICROSECONDS          { 250u };
    static const uint32_t   TEST_MIN_DELAY_IN_MICROSECONDS      { 25u };
    static const uint32_t   TEST_MAX_DELAY_IN_MICROSECONDS      { 250000u };
    static const bool       TEST_NEEDS_POSTPONE_EXECUTION       { true };

    const std::function<bool()> testFunctionReturnTrue{ [] { return true; } };

    const std::function<bool(const uint32_t)> testFunctionDelayReturnTrue
    {
        [] (const uint32_t delayInMicroseconds)
        {
             OSAL::Thread::delay(delayInMicroseconds);
             return true;
         }
     };


public: // ConcurrentRun version

    static const uint64_t   TEST_PROGRESSION_MIN_VALUE          { 100000u };
    static const uint64_t   TEST_PROGRESSION_MAX_VALUE          { 1000000u };

    static const uint64_t   TEST_TIME_PER_TEST                  { 40000000u };  // 40 sec - time for one test for Tbb or ConcurrentRun
    static const uint32_t   TEST_CONTAINER_SIZE                 { 5000u };

    uint32_t elementsCount  { 1u };
    uint32_t nodesCount     { 100000u };


    uint64_t progression(const uint64_t value) const
    {
        uint64_t sum{ 0u };
        for (uint64_t j = value; j > 0u; --j)
        {
            sum += j;
        }

        return sum;
    };


    uint64_t deProgression (const uint64_t value) const
    {
        uint64_t sum{ 0u };
        uint64_t j;

        for (j = 1u; sum < value; ++j)
        {
            sum += j;
        }
        --j;

        return j;
    };


    const std::function<std::vector<uint64_t>(const std::vector<uint64_t> &)> testFunctionSerialProgressionAndDeProgression
    {
        [this] (const std::vector<uint64_t> & dataContainer) -> std::vector<uint64_t>
        {
            std::vector<uint64_t> outputDataContainer{ dataContainer };

            // Suppressing cppcheck issues for testing
            for (auto && dataIt : outputDataContainer)
            {
                // cppcheck-suppress useStlAlgorithm
                dataIt = progression(dataIt);
            }

            // Suppressing cppcheck issues for testing
            for (auto && dataIt : outputDataContainer)
            {
                // cppcheck-suppress useStlAlgorithm
                dataIt = deProgression(dataIt);
            }

            return outputDataContainer;
        }
    };


    void initializeElementsSizes()
    {
        auto container = getRandomDataContainer(TEST_CONTAINER_SIZE, TEST_PROGRESSION_MIN_VALUE, TEST_PROGRESSION_MAX_VALUE);

        OSAL::Time time;

        for (size_t i = 0u; i < TEST_CONTAINER_SIZE; ++i)
        {
            const auto value = progression(container[i]);
            container[i] = deProgression(value);
        }

        const uint32_t multiplier{ static_cast<uint32_t>(static_cast<uint64_t>(TEST_CONTAINER_SIZE) * TEST_TIME_PER_TEST / time.getElapsedTime()) };
        while (4u * multiplier < 10u * nodesCount)
        {
            nodesCount /= 10u;
        }

        elementsCount = 4u * multiplier / nodesCount;

        std::string infoString =
                "Initialization:"
                "\n NodesCount =        " + std::to_string(nodesCount) +
                "\n ElementsCount =     " + std::to_string(elementsCount) + "\n";

        logging.logInfo(infoString.c_str());
    }


protected: // Random generators

    std::vector<uint64_t> getRandomDataContainer(const uint32_t size, const uint64_t minValue, const uint64_t maxValue) const
    {
        std::vector<uint64_t> container(static_cast<std::vector<uint64_t>::size_type>(size));

        std::generate(container.begin(), container.end(),
            [&minValue, &maxValue]
            {
                return getRandomNumber(minValue, maxValue);
            });

        return container;
    }


    std::vector<uint32_t> getRandomDelayContainer(const uint32_t size, const uint32_t minDelayInMicroseconds, const uint32_t maxDelayInMicroseconds)
    {
        std::vector<uint32_t> container(static_cast<std::vector<uint32_t>::size_type>(size));

        std::generate(container.begin(), container.end(),
            [&minDelayInMicroseconds, &maxDelayInMicroseconds]
            {
                return getRandomNumber(minDelayInMicroseconds, maxDelayInMicroseconds);
            });

        return container;
    }


    std::vector<Priority> getRandomPriorityContainer(const uint32_t size)
    {
        std::vector<Priority> container(static_cast<std::vector<Priority>::size_type>(size));

        std::generate(container.begin(), container.end(), []
            {
                return static_cast<Priority>(getRandomNumber(
                       static_cast<uint32_t>(++Priority::FIRST_PRIORITIES_POSITION),
                       static_cast<uint32_t>(--Priority::LAST_PRIORITIES_POSITION)));
            });

        return container;
    }


    std::vector<BurstTime> getRandomBurstTimeContainer(const uint32_t size)
    {
        std::vector<BurstTime> container(static_cast<std::vector<BurstTime>::size_type>(size));

        std::generate(container.begin(), container.end(), []
            {
                return static_cast<BurstTime>(getRandomNumber(
                       static_cast<uint32_t>(++BurstTime::FIRST_BURST_TIMES_POSITION),
                       static_cast<uint32_t>(--BurstTime::LAST_BURST_TIMES_POSITION)));
            });

        return container;
    }

public:

    struct TestExecutionResult
    {
        std::string                     title;
        std::shared_ptr<IThreadPool>    threadPool;
        uint32_t                        numberOfTasks;
        uint64_t                        threadPoolElapsedTimeInMicroseconds;
        uint64_t                        manualElapsedTimeInMicroseconds;

        std::string toString() const
        {
            std::ostringstream oss;

            oss << "\n---> " << title;

            oss <<     "\n  Was postponed:            " << std::to_string(threadPool->getOptions().needsPostponeExecution())
                <<     "\n  Scheduler type:           " << ThreadPoolOptions::schedulerTypeToString(threadPool->getOptions().getSchedulerType())
                <<     "\n  Number of workers:        " << std::to_string(threadPool->getWorkersSize())
                <<     "\n  Number of tasks:          " << std::to_string(numberOfTasks)
                <<     "\n  Elapsed time thread pool: " << std::to_string(threadPoolElapsedTimeInMicroseconds / 1000000.0) << " sec."
                <<     "\n  Elapsed time manual:      " << std::to_string(manualElapsedTimeInMicroseconds / 1000000.0) << " sec."
                <<     "\n  Speed up:                 " << std::to_string(manualElapsedTimeInMicroseconds / static_cast<double>(threadPoolElapsedTimeInMicroseconds)) << "\n\n";

            return oss.str();
        }
    };


    Logging logging{ "ThreadPoolTest" };

protected: // Help methods

    void EXPECT_ALL_EXECUTED(const TasksContainer & tasks) const
    {
        for (auto && taskIt : tasks)
        {
            EXPECT_EQ(taskIt->getState(), IThreadPoolTask::State::EXECUTED);
        }
    }


protected: // Get regular tasks

    TasksContainer getSameSingleReturnTasks(const uint32_t numberOfTasks)
    {
        return ThreadPoolTask::submitRepeated(numberOfTasks, testFunctionReturnTrue);
    }


    TasksContainer getSameDelayedTasks(const uint32_t numberOfTasks, const uint32_t delayInMicroseconds)
    {
        return ThreadPoolTask::submitRepeated(numberOfTasks, testFunctionDelayReturnTrue, delayInMicroseconds);
    }


    TasksContainer getDifferentDelayedTasks(const std::vector<uint32_t> & delayContainer)
    {
        const size_t size{ delayContainer.size() };

        TasksContainer tasks(size, nullptr);

        for (size_t i = 0u; i < size; ++i)
        {
            auto task = std::make_shared<TestTask>();
            task->submitOne(testFunctionDelayReturnTrue, delayContainer[i]);

            tasks[i] = std::move(task);
        }

        return tasks;
    }


    TasksContainer getSerialProgressionAndDeProgressionTasks(const std::vector<uint64_t> & dataContainer) const
    {
        TasksContainer tasks(dataContainer.size());

        for (auto && taskIt : tasks)
        {
            auto task = std::make_shared<TestTask>();
            task->submitOne(testFunctionSerialProgressionAndDeProgression, dataContainer);

            taskIt = std::move(task);
        }

        return tasks;
    }


protected: // Get priority tasks

    TasksContainer getSingleReturnPriorityTasks(const std::vector<Priority> & priorityContainer)
    {
        const size_t size{ priorityContainer.size() };

        TasksContainer tasks(size, nullptr);

        for (size_t i = 0u; i < size; ++i)
        {
            auto task = std::make_shared<PriorityTask>(priorityContainer[i]);
            task->submitOne(testFunctionReturnTrue);

            tasks[i] = std::move(task);
        }

        return tasks;
    }


    TasksContainer getSameDelayedPriorityTasks(const std::vector<Priority> & priorityContainer, const uint32_t delayInMicroseconds)
    {
        const size_t size{ priorityContainer.size() };

        TasksContainer tasks(size, nullptr);

        for (size_t i = 0u; i < size; ++i)
        {
            auto task = std::make_shared<PriorityTask>(priorityContainer[i]);
            task->submitOne(testFunctionDelayReturnTrue, delayInMicroseconds);

            tasks[i] = std::move(task);
        }

        return tasks;
    }


    TasksContainer getDifferentDelayedPriorityTasks(const std::vector<Priority> & priorityContainer, const std::vector<uint32_t> & delayContainer)
    {
        const size_t size{ priorityContainer.size() };

        TasksContainer tasks(size, nullptr);

        for (size_t i = 0u; i < size; ++i)
        {
            auto task = std::make_shared<PriorityTask>(priorityContainer[i]);
            task->submitOne(testFunctionDelayReturnTrue, delayContainer[i]);

            tasks[i] = std::move(task);
        }

        return tasks;
    }


    TasksContainer getSerialProgressionAndDeProgressionPriorityTasks(const std::vector<Priority> & priorityContainer, const std::vector<uint64_t> & dataContainer)
    {
        const size_t size{ priorityContainer.size() };

        TasksContainer tasks(size, nullptr);

        for (size_t i = 0u; i < size; ++i)
        {
            auto task = std::make_shared<PriorityTask>(priorityContainer[i]);
            task->submitOne(testFunctionSerialProgressionAndDeProgression, dataContainer);

            tasks[i] = std::move(task);
        }

        return tasks;
    }


protected: // Get burst time tasks

    TasksContainer getSingleReturnBurstTimeTasks(const std::vector<BurstTime> & burstTimeContainer)
    {
        const size_t size{ burstTimeContainer.size() };

        TasksContainer tasks(size, nullptr);

        for (size_t i = 0u; i < size; ++i)
        {
            auto task = std::make_shared<BurstTimeTask>(burstTimeContainer[i]);
            task->submitOne(testFunctionReturnTrue);

            tasks[i] = std::move(task);
        }

        return tasks;
    }


    TasksContainer getSameDelayedBurstTimeTasks(const std::vector<BurstTime> &  burstTimeContainer, const uint32_t delayInMicroseconds)
    {
        const size_t size{ burstTimeContainer.size() };

        TasksContainer tasks(size, nullptr);

        for (size_t i = 0u; i < size; ++i)
        {
            auto task = std::make_shared<BurstTimeTask>(burstTimeContainer[i]);
            task->submitOne(testFunctionDelayReturnTrue, delayInMicroseconds);

            tasks[i] = std::move(task);
        }

        return tasks;
    }


    TasksContainer getDifferentDelayedBurstTimeTasks(const std::vector<BurstTime> & burstTimeContainer, const std::vector<uint32_t> & delayContainer)
    {
        const size_t size{ burstTimeContainer.size() };

        TasksContainer tasks(size, nullptr);

        for (size_t i = 0u; i < size; ++i)
        {
            auto task = std::make_shared<BurstTimeTask>(burstTimeContainer[i]);
            task->submitOne(testFunctionDelayReturnTrue, delayContainer[i]);

            tasks[i] = std::move(task);
        }

        return tasks;
    }


    TasksContainer getSerialProgressionAndDeProgressionBurstTimeTasks(const std::vector<BurstTime> & burstTimeContainer, const std::vector<uint64_t> & dataContainer)
    {
        const size_t size{ burstTimeContainer.size() };

        TasksContainer tasks(size, nullptr);

        for (size_t i = 0u; i < size; ++i)
        {
            auto task = std::make_shared<BurstTimeTask>(burstTimeContainer[i]);
            task->submitOne(testFunctionSerialProgressionAndDeProgression, dataContainer);

            tasks[i] = std::move(task);
        }

        return tasks;
    }


protected: // Get elapsed time

    uint64_t getElapsedTimeForManuallyExecutedTasks(const TasksContainer & tasks)
    {
        OSAL::Time time;

        // Manually execute task one by one
        for (auto && taskIt : tasks)
        {
            taskIt->execute();
        }

        return time.getElapsedTime();
    }


    uint64_t getElapsedTimeForThreadPoolExecutedTasksWithAutomaticAdding(const std::shared_ptr<IThreadPool> & threadPool, const TasksContainer & tasks)
    {
        const bool needsPostpone{ threadPool->getOptions().needsPostponeExecution() };

        OSAL::Time time;

        // Automatically add tasks
        threadPool->addTasks(tasks);

        // If thread pool was postponed now we need to start it
        if (needsPostpone)
        {
            threadPool->startExecution();
        }

        threadPool->waitAllTasksExecutionFinished(-1);

        return time.getElapsedTime();
    }


    uint64_t getElapsedTimeForThreadPoolExecutedTasksWithManualAdding(const std::shared_ptr<IThreadPool> & threadPool, const TasksContainer & tasks)
    {
        const bool needsPostpone{ threadPool->getOptions().needsPostponeExecution() };

        OSAL::Time time;

        // Manually add task one by one
        for (auto && taskIt : tasks)
        {
            threadPool->addTask(taskIt);
        }

        // If thread pool was postponed now we need to start it
        if (needsPostpone)
        {
            threadPool->startExecution();
        }

        threadPool->waitAllTasksExecutionFinished(-1);

        return time.getElapsedTime();
    }


protected: // Test execution

    void testExecutionWithAutomaticTasksAdding(const std::shared_ptr<IThreadPool> & threadPool, const TasksContainer & tasksForThreadPool, const TasksContainer & tasksForManual, const std::string & title)
    {
        const auto threadPoolElapsedTime =  getElapsedTimeForThreadPoolExecutedTasksWithAutomaticAdding(threadPool, tasksForThreadPool);
        const auto manualElapsedTime =      getElapsedTimeForManuallyExecutedTasks(tasksForManual);

        TestExecutionResult result{ title, threadPool, static_cast<uint32_t>(tasksForThreadPool.size()), threadPoolElapsedTime, manualElapsedTime };

        logging.logInfo(result.toString().c_str());

        EXPECT_ALL_EXECUTED(tasksForThreadPool);
        EXPECT_ALL_EXECUTED(tasksForManual);
    }


    void testExecutionWithManualTasksAdding(const std::shared_ptr<IThreadPool> & threadPool, const TasksContainer & tasksForThreadPool, const TasksContainer & tasksForManual, const std::string & title)
    {
        const auto threadPoolElapsedTime =  getElapsedTimeForThreadPoolExecutedTasksWithManualAdding(threadPool, tasksForThreadPool);
        const auto manualElapsedTime =      getElapsedTimeForManuallyExecutedTasks(tasksForManual);

        TestExecutionResult result{ title, threadPool, static_cast<uint32_t>(tasksForThreadPool.size()), threadPoolElapsedTime, manualElapsedTime };

        logging.logInfo(result.toString().c_str());

        EXPECT_ALL_EXECUTED(tasksForThreadPool);
        EXPECT_ALL_EXECUTED(tasksForManual);
    }
};



class Foundations_ThreadPool_Performance : public Foundations_ThreadPool_Performance_Base
{
};


//////////////////////////////////////////////////////////////////////////////////////! Default Thread Pool

TEST_F(Foundations_ThreadPool_Performance, singleReturnTasksManualAdding)
{
    // Case with same single return tasks with FCFS scheduler and manual adding
    {
        ThreadPoolOptions options{
            ThreadPoolOptions::SchedulerType::FCFS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NEEDS_POSTPONE_EXECUTION, false };
        std::shared_ptr<IThreadPool> threadPool{ std::make_shared<ThreadPool>(options) };

        auto tasksForThreadPool =           getSameSingleReturnTasks(TEST_NUMBER_OF_TASKS);
        auto tasksForManual =               getSameSingleReturnTasks(TEST_NUMBER_OF_TASKS);

        testExecutionWithManualTasksAdding(threadPool, tasksForThreadPool, tasksForManual, "Single return tasks. Manual adding");
    }

    // Case with same single return tasks with Priority scheduler and manual adding
    {
        ThreadPoolOptions options{
            ThreadPoolOptions::SchedulerType::PRIORITY, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NEEDS_POSTPONE_EXECUTION, false };
        std::shared_ptr<IThreadPool> threadPool{ std::make_shared<ThreadPool>(options) };

        const auto priorityContainer =      getRandomPriorityContainer(TEST_NUMBER_OF_TASKS);

        auto tasksForThreadPool =           getSingleReturnPriorityTasks(priorityContainer);
        auto tasksForManual =               getSingleReturnPriorityTasks(priorityContainer);

        testExecutionWithManualTasksAdding(threadPool, tasksForThreadPool, tasksForManual, "Single return random priority tasks. Manual adding");
    }

    // Case with same single return tasks with Burst Time scheduler and manual adding
    {
        ThreadPoolOptions options{
            ThreadPoolOptions::SchedulerType::SJF, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NEEDS_POSTPONE_EXECUTION, false };
        std::shared_ptr<IThreadPool> threadPool{ std::make_shared<ThreadPool>(options) };

        const auto burstTimeContainer =     getRandomBurstTimeContainer(TEST_NUMBER_OF_TASKS);

        auto tasksForThreadPool =           getSingleReturnBurstTimeTasks(burstTimeContainer);
        auto tasksForManual =               getSingleReturnBurstTimeTasks(burstTimeContainer);

        testExecutionWithManualTasksAdding(threadPool, tasksForThreadPool, tasksForManual, "Single return random burst time tasks. Manual adding");
    }
}


TEST_F(Foundations_ThreadPool_Performance, singleReturnTasksAutomaticAdding)
{
    // Case with same single return tasks with FCFS scheduler and automatic adding
    {
        ThreadPoolOptions options{
            ThreadPoolOptions::SchedulerType::FCFS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NEEDS_POSTPONE_EXECUTION, false };
        std::shared_ptr<IThreadPool> threadPool{ std::make_shared<ThreadPool>(options) };

        auto tasksForThreadPool =           getSameSingleReturnTasks(TEST_NUMBER_OF_TASKS);
        auto tasksForManual =               getSameSingleReturnTasks(TEST_NUMBER_OF_TASKS);

        testExecutionWithAutomaticTasksAdding(threadPool, tasksForThreadPool, tasksForManual, "Single return tasks. Automatic adding");
    }

    // Case with same single return tasks with Priority scheduler and automatic adding
    {
        ThreadPoolOptions options{
            ThreadPoolOptions::SchedulerType::PRIORITY, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NEEDS_POSTPONE_EXECUTION, false };
        std::shared_ptr<IThreadPool> threadPool{ std::make_shared<ThreadPool>(options) };

        const auto priorityContainer =      getRandomPriorityContainer(TEST_NUMBER_OF_TASKS);

        auto tasksForThreadPool =           getSingleReturnPriorityTasks(priorityContainer);
        auto tasksForManual =               getSingleReturnPriorityTasks(priorityContainer);

        testExecutionWithAutomaticTasksAdding(threadPool, tasksForThreadPool, tasksForManual, "Single return random priority tasks. Automatic adding");
    }

    // Case with same single return tasks with Burst Time scheduler and automatic adding
    {
        ThreadPoolOptions options{
            ThreadPoolOptions::SchedulerType::SJF, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NEEDS_POSTPONE_EXECUTION, false };
        std::shared_ptr<IThreadPool> threadPool{ std::make_shared<ThreadPool>(options) };

        const auto burstTimeContainer =     getRandomBurstTimeContainer(TEST_NUMBER_OF_TASKS);

        auto tasksForThreadPool =           getSingleReturnBurstTimeTasks(burstTimeContainer);
        auto tasksForManual =               getSingleReturnBurstTimeTasks(burstTimeContainer);

        testExecutionWithAutomaticTasksAdding(threadPool, tasksForThreadPool, tasksForManual, "Single return random burst time tasks. Automatic adding");
    }
}


TEST_F(Foundations_ThreadPool_Performance, sameDelayedTasksManualAdding)
{
    // Case with same delayed tasks with FCFS scheduler and manual adding
    {
        ThreadPoolOptions options{
            ThreadPoolOptions::SchedulerType::FCFS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NEEDS_POSTPONE_EXECUTION, false };
        std::shared_ptr<IThreadPool> threadPool{ std::make_shared<ThreadPool>(options) };

        auto tasksForThreadPool =           getSameDelayedTasks(TEST_NUMBER_OF_TASKS, TEST_DELAY_IN_MICROSECONDS);
        auto tasksForManual =               getSameDelayedTasks(TEST_NUMBER_OF_TASKS, TEST_DELAY_IN_MICROSECONDS);

        testExecutionWithManualTasksAdding(threadPool, tasksForThreadPool, tasksForManual, "Same delayed tasks. Manual adding");
    }

    // Case with same delayed tasks with Priority scheduler and manual adding
    {
        ThreadPoolOptions options{
            ThreadPoolOptions::SchedulerType::PRIORITY, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NEEDS_POSTPONE_EXECUTION, false };
        std::shared_ptr<IThreadPool> threadPool{ std::make_shared<ThreadPool>(options) };

        const auto priorityContainer =      getRandomPriorityContainer(TEST_NUMBER_OF_TASKS);

        auto tasksForThreadPool =           getSameDelayedPriorityTasks(priorityContainer, TEST_DELAY_IN_MICROSECONDS);
        auto tasksForManual =               getSameDelayedPriorityTasks(priorityContainer, TEST_DELAY_IN_MICROSECONDS);

        testExecutionWithManualTasksAdding(threadPool, tasksForThreadPool, tasksForManual, "Same delayed random priority tasks. Manual adding");
    }

    // Case with same delayed tasks with Burst Time scheduler and manual adding
    {
        ThreadPoolOptions options{
            ThreadPoolOptions::SchedulerType::SJF, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NEEDS_POSTPONE_EXECUTION, false };
        std::shared_ptr<IThreadPool> threadPool{ std::make_shared<ThreadPool>(options) };

        const auto burstTimeContainer =     getRandomBurstTimeContainer(TEST_NUMBER_OF_TASKS);

        auto tasksForThreadPool =           getSameDelayedBurstTimeTasks(burstTimeContainer, TEST_DELAY_IN_MICROSECONDS);
        auto tasksForManual =               getSameDelayedBurstTimeTasks(burstTimeContainer, TEST_DELAY_IN_MICROSECONDS);

        testExecutionWithManualTasksAdding(threadPool, tasksForThreadPool, tasksForManual, "Same delayed random burst time tasks. Manual adding");
    }
}


TEST_F(Foundations_ThreadPool_Performance, sameDelayedTasksAutomaticAdding)
{
    // Case with same delayed tasks with FCFS scheduler and automatic adding
    {
        ThreadPoolOptions options{
            ThreadPoolOptions::SchedulerType::FCFS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NEEDS_POSTPONE_EXECUTION, false };
        std::shared_ptr<IThreadPool> threadPool{ std::make_shared<ThreadPool>(options) };

        auto tasksForThreadPool =           getSameDelayedTasks(TEST_NUMBER_OF_TASKS, TEST_DELAY_IN_MICROSECONDS);
        auto tasksForManual =               getSameDelayedTasks(TEST_NUMBER_OF_TASKS, TEST_DELAY_IN_MICROSECONDS);

        testExecutionWithAutomaticTasksAdding(threadPool, tasksForThreadPool, tasksForManual, "Same delayed tasks. Automatic adding");
    }

    // Case with same delayed tasks with Priority scheduler and automatic adding
    {
        ThreadPoolOptions options{
            ThreadPoolOptions::SchedulerType::PRIORITY, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NEEDS_POSTPONE_EXECUTION, false };
        std::shared_ptr<IThreadPool> threadPool{ std::make_shared<ThreadPool>(options) };

        const auto priorityContainer =      getRandomPriorityContainer(TEST_NUMBER_OF_TASKS);

        auto tasksForThreadPool =           getSameDelayedPriorityTasks(priorityContainer, TEST_DELAY_IN_MICROSECONDS);
        auto tasksForManual =               getSameDelayedPriorityTasks(priorityContainer, TEST_DELAY_IN_MICROSECONDS);

        testExecutionWithAutomaticTasksAdding(threadPool, tasksForThreadPool, tasksForManual, "Same delayed random priority tasks. Automatic adding");
    }

    // Case with same delayed tasks with Burst Time scheduler and automatic adding
    {
        ThreadPoolOptions options{
            ThreadPoolOptions::SchedulerType::SJF, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NEEDS_POSTPONE_EXECUTION, false };
        std::shared_ptr<IThreadPool> threadPool{ std::make_shared<ThreadPool>(options) };

        const auto burstTimeContainer =     getRandomBurstTimeContainer(TEST_NUMBER_OF_TASKS);

        auto tasksForThreadPool =           getSameDelayedBurstTimeTasks(burstTimeContainer, TEST_DELAY_IN_MICROSECONDS);
        auto tasksForManual =               getSameDelayedBurstTimeTasks(burstTimeContainer, TEST_DELAY_IN_MICROSECONDS);

        testExecutionWithAutomaticTasksAdding(threadPool, tasksForThreadPool, tasksForManual, "Same delayed random burst time tasks. Automatic adding");
    }
}


TEST_F(Foundations_ThreadPool_Performance, differentDelayedTasksManualAdding)
{
    // Case with different delayed tasks with FCFS scheduler and manual adding
    {
        ThreadPoolOptions options{
            ThreadPoolOptions::SchedulerType::FCFS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NEEDS_POSTPONE_EXECUTION, false };
        std::shared_ptr<IThreadPool> threadPool{ std::make_shared<ThreadPool>(options) };

        const auto delayContainer =         getRandomDelayContainer(TEST_NUMBER_OF_TASKS, TEST_MIN_DELAY_IN_MICROSECONDS, TEST_MAX_DELAY_IN_MICROSECONDS);

        auto tasksForThreadPool =           getDifferentDelayedTasks(delayContainer);
        auto tasksForManual =               getDifferentDelayedTasks(delayContainer);

        testExecutionWithManualTasksAdding(threadPool, tasksForThreadPool, tasksForManual, "Different delayed tasks. Manual adding");
    }

    // Case with different delayed tasks with Priority scheduler and manual adding
    {
        ThreadPoolOptions options{
            ThreadPoolOptions::SchedulerType::PRIORITY, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NEEDS_POSTPONE_EXECUTION, false };
        std::shared_ptr<IThreadPool> threadPool{ std::make_shared<ThreadPool>(options) };

        const auto priorityContainer =      getRandomPriorityContainer(TEST_NUMBER_OF_TASKS);
        const auto delayContainer =         getRandomDelayContainer(TEST_NUMBER_OF_TASKS, TEST_MIN_DELAY_IN_MICROSECONDS, TEST_MAX_DELAY_IN_MICROSECONDS);

        auto tasksForThreadPool =           getDifferentDelayedPriorityTasks(priorityContainer, delayContainer);
        auto tasksForManual =               getDifferentDelayedPriorityTasks(priorityContainer, delayContainer);

        testExecutionWithManualTasksAdding(threadPool, tasksForThreadPool, tasksForManual, "Different delayed random priority tasks. Manual adding");
    }

    // Case with different delayed tasks with Burst Time scheduler and manual adding
    {
        ThreadPoolOptions options{
            ThreadPoolOptions::SchedulerType::SJF, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NEEDS_POSTPONE_EXECUTION, false };
        std::shared_ptr<IThreadPool> threadPool{ std::make_shared<ThreadPool>(options) };

        const auto burstTimeContainer =     getRandomBurstTimeContainer(TEST_NUMBER_OF_TASKS);
        const auto delayContainer =         getRandomDelayContainer(TEST_NUMBER_OF_TASKS, TEST_MIN_DELAY_IN_MICROSECONDS, TEST_MAX_DELAY_IN_MICROSECONDS);

        auto tasksForThreadPool =           getDifferentDelayedBurstTimeTasks(burstTimeContainer, delayContainer);
        auto tasksForManual =               getDifferentDelayedBurstTimeTasks(burstTimeContainer, delayContainer);

        testExecutionWithManualTasksAdding(threadPool, tasksForThreadPool, tasksForManual, "Different delayed random burst time tasks. Manual adding");
    }
}


TEST_F(Foundations_ThreadPool_Performance, differentDelayedTasksAutomaticAdding)
{
    // Case with different delayed tasks with FCFS scheduler and automatic adding
    {
        ThreadPoolOptions options{
            ThreadPoolOptions::SchedulerType::FCFS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NEEDS_POSTPONE_EXECUTION, false };
        std::shared_ptr<IThreadPool> threadPool{ std::make_shared<ThreadPool>(options) };

        const auto delayContainer =         getRandomDelayContainer(TEST_NUMBER_OF_TASKS, TEST_MIN_DELAY_IN_MICROSECONDS, TEST_MAX_DELAY_IN_MICROSECONDS);

        auto tasksForThreadPool =           getDifferentDelayedTasks(delayContainer);
        auto tasksForManual =               getDifferentDelayedTasks(delayContainer);

        testExecutionWithAutomaticTasksAdding(threadPool, tasksForThreadPool, tasksForManual, "Different delayed tasks. Automatic adding");
    }

    // Case with different delayed tasks with Priority scheduler and automatic adding
    {
        ThreadPoolOptions options{
            ThreadPoolOptions::SchedulerType::PRIORITY, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NEEDS_POSTPONE_EXECUTION, false };
        std::shared_ptr<IThreadPool> threadPool{ std::make_shared<ThreadPool>(options) };

        const auto priorityContainer =      getRandomPriorityContainer(TEST_NUMBER_OF_TASKS);
        const auto delayContainer =         getRandomDelayContainer(TEST_NUMBER_OF_TASKS, TEST_MIN_DELAY_IN_MICROSECONDS, TEST_MAX_DELAY_IN_MICROSECONDS);

        auto tasksForThreadPool =           getDifferentDelayedPriorityTasks(priorityContainer, delayContainer);
        auto tasksForManual =               getDifferentDelayedPriorityTasks(priorityContainer, delayContainer);

        testExecutionWithAutomaticTasksAdding(threadPool, tasksForThreadPool, tasksForManual, "Different delayed random priority tasks. Automatic adding");
    }

    // Case with different delayed tasks with Burst Time scheduler and automatic adding
    {
        ThreadPoolOptions options{
            ThreadPoolOptions::SchedulerType::SJF, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NEEDS_POSTPONE_EXECUTION, false };
        std::shared_ptr<IThreadPool> threadPool{ std::make_shared<ThreadPool>(options) };

        const auto burstTimeContainer =     getRandomBurstTimeContainer(TEST_NUMBER_OF_TASKS);
        const auto delayContainer =         getRandomDelayContainer(TEST_NUMBER_OF_TASKS, TEST_MIN_DELAY_IN_MICROSECONDS, TEST_MAX_DELAY_IN_MICROSECONDS);

        auto tasksForThreadPool =           getDifferentDelayedBurstTimeTasks(burstTimeContainer, delayContainer);
        auto tasksForManual =               getDifferentDelayedBurstTimeTasks(burstTimeContainer, delayContainer);

        testExecutionWithAutomaticTasksAdding(threadPool, tasksForThreadPool, tasksForManual, "Different delayed random burst time tasks. Automatic adding");
    }
}


TEST_F(Foundations_ThreadPool_Performance, serialProgressionDeProgressionManualAdding)
{
    initializeElementsSizes();

    // Case with serial progression and de progression tasks with FCFS scheduler and manual adding
    {
        ThreadPoolOptions options{
            ThreadPoolOptions::SchedulerType::FCFS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NEEDS_POSTPONE_EXECUTION, false };
        std::shared_ptr<IThreadPool> threadPool{ std::make_shared<ThreadPool>(options) };

        const auto dataContainer =          getRandomDataContainer(elementsCount, TEST_PROGRESSION_MIN_VALUE, TEST_PROGRESSION_MAX_VALUE);

        auto tasksForThreadPool =           getSerialProgressionAndDeProgressionTasks(dataContainer);
        auto tasksForManual =               getSerialProgressionAndDeProgressionTasks(dataContainer);

        testExecutionWithManualTasksAdding(threadPool, tasksForThreadPool, tasksForManual, "Serial progression and de progresssion tasks. Manual adding");
    }

    // Case with serial progression and de progression tasks with Priority scheduler and manual adding
    {
        ThreadPoolOptions options{
            ThreadPoolOptions::SchedulerType::PRIORITY, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NEEDS_POSTPONE_EXECUTION, false };
        std::shared_ptr<IThreadPool> threadPool{ std::make_shared<ThreadPool>(options) };

        const auto priorityContainer =      getRandomPriorityContainer(elementsCount);
        const auto dataContainer =          getRandomDataContainer(elementsCount, TEST_PROGRESSION_MIN_VALUE, TEST_PROGRESSION_MAX_VALUE);

        auto tasksForThreadPool =           getSerialProgressionAndDeProgressionPriorityTasks(priorityContainer, dataContainer);
        auto tasksForManual =               getSerialProgressionAndDeProgressionPriorityTasks(priorityContainer, dataContainer);

        testExecutionWithManualTasksAdding(threadPool, tasksForThreadPool, tasksForManual, "Serial progression and de progresssion random priority tasks. Manual adding");
    }

    // Case with serial progression and de progression tasks with Burst Time scheduler and manual adding
    {
        ThreadPoolOptions options{
            ThreadPoolOptions::SchedulerType::SJF, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NEEDS_POSTPONE_EXECUTION, false };
        std::shared_ptr<IThreadPool> threadPool{ std::make_shared<ThreadPool>(options) };

        const auto burstTimeContainer =     getRandomBurstTimeContainer(elementsCount);
        const auto dataContainer =          getRandomDataContainer(elementsCount, TEST_PROGRESSION_MIN_VALUE, TEST_PROGRESSION_MAX_VALUE);

        auto tasksForThreadPool =           getSerialProgressionAndDeProgressionBurstTimeTasks(burstTimeContainer, dataContainer);
        auto tasksForManual =               getSerialProgressionAndDeProgressionBurstTimeTasks(burstTimeContainer, dataContainer);

        testExecutionWithManualTasksAdding(threadPool, tasksForThreadPool, tasksForManual, "Serial progression and de progresssion random burst time tasks. Manual adding");
    }
}


TEST_F(Foundations_ThreadPool_Performance, serialProgressionDeProgressionAutomaticAdding)
{
    initializeElementsSizes();

    // Case with serial progression and de progression tasks with FCFS scheduler and automatic adding
    {
        ThreadPoolOptions options{
            ThreadPoolOptions::SchedulerType::FCFS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NEEDS_POSTPONE_EXECUTION, false };
        std::shared_ptr<IThreadPool> threadPool{ std::make_shared<ThreadPool>(options) };

        const auto dataContainer =          getRandomDataContainer(elementsCount, TEST_PROGRESSION_MIN_VALUE, TEST_PROGRESSION_MAX_VALUE);

        auto tasksForThreadPool =           getSerialProgressionAndDeProgressionTasks(dataContainer);
        auto tasksForManual =               getSerialProgressionAndDeProgressionTasks(dataContainer);

        testExecutionWithAutomaticTasksAdding(threadPool, tasksForThreadPool, tasksForManual, "Serial progression and de progresssion tasks. Automatic adding");
    }

    // Case with serial progression and de progression tasks with Priority scheduler and automatic adding
    {
        ThreadPoolOptions options{
            ThreadPoolOptions::SchedulerType::PRIORITY, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NEEDS_POSTPONE_EXECUTION, false };
        std::shared_ptr<IThreadPool> threadPool{ std::make_shared<ThreadPool>(options) };

        const auto priorityContainer =      getRandomPriorityContainer(elementsCount);
        const auto dataContainer =          getRandomDataContainer(elementsCount, TEST_PROGRESSION_MIN_VALUE, TEST_PROGRESSION_MAX_VALUE);

        auto tasksForThreadPool =           getSerialProgressionAndDeProgressionPriorityTasks(priorityContainer, dataContainer);
        auto tasksForManual =               getSerialProgressionAndDeProgressionPriorityTasks(priorityContainer, dataContainer);

        testExecutionWithAutomaticTasksAdding(threadPool, tasksForThreadPool, tasksForManual, "Serial progression and de progresssion random priority tasks. Automatic adding");
    }

    // Case with serial progression and de progression tasks with Burst Time scheduler and automatic adding
    {
        ThreadPoolOptions options{
            ThreadPoolOptions::SchedulerType::SJF, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NUMBER_OF_WORKERS, TEST_NEEDS_POSTPONE_EXECUTION, false };
        std::shared_ptr<IThreadPool> threadPool{ std::make_shared<ThreadPool>(options) };

        const auto burstTimeContainer =     getRandomBurstTimeContainer(elementsCount);
        const auto dataContainer =          getRandomDataContainer(elementsCount, TEST_PROGRESSION_MIN_VALUE, TEST_PROGRESSION_MAX_VALUE);

        auto tasksForThreadPool =           getSerialProgressionAndDeProgressionBurstTimeTasks(burstTimeContainer, dataContainer);
        auto tasksForManual =               getSerialProgressionAndDeProgressionBurstTimeTasks(burstTimeContainer, dataContainer);

        testExecutionWithAutomaticTasksAdding(threadPool, tasksForThreadPool, tasksForManual, "Serial progression and de progresssion random burst time tasks. Automatic adding");
    }
}

