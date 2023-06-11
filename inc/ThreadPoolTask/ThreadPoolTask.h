#ifndef _THREADPOOLTASK_H_
#define _THREADPOOLTASK_H_


#include <functional>
#include <future>
#include <atomic>
#include <vector>

#include "IThreadPoolTask.h"
#include "Logging.h"


/**
 * @note When you create a thread pool task you should submit some function to be able to execute it.
 *       Common usage of ThreadPoolTask API is the following:
 *          1. Create some task.
 *          2. Submit any function with any number of parameters it expects.
 *          3. Add created task to ThreadPool.
 *
 *       Bingo! After some time your task will be scheduled and executed.
 *       If you also want to get result of execution, you should save the return type of sumbit() method.
 *       The return type is std::future, which holds appropriate type of result.
 *
 * ----> Code example for one task:
 *          Imagine you want to submit this function:
 *              int sum(int a, int b);
 *
 *              std::shared_ptr<ThreadPoolTask> task = std::make_shared<ThreadPoolTask>();      // 1 step
 *              future<int> futureResult = task->submitOne(sum, 1, 2);                          // 2 step
 *              threadPool.addTask(task);                                                       // 3 step
 *
 *              // imagine task has already executed at this point
 *              int result = futureResult.get_result();                                         // 4 step
 *
 * ----> Note to code example:
 *          On the step #2 we submit the function sum and the return type is std::future<int>, since return type of function sum() is int.
 *          For the simplification you can use auto (but better to be explicit and use std::future<int>):
 *               auto futureResult = task->submit(sum, 1, 2);
 *          So, you submit it and store this std::future<int> in some variable to be able to get the result after execution on the step #4.
 *
 *
 *       So far you have seen how to add and execute only one function and get result of execution.
 *       What if you want same function to be executed multiple times?
 *       In such case you have 2 options:
 *          1. ThreadPoolTask API - use static method ThreadPoolTask::submitRepeated.
 *          2. Manually - create own std::vector of tasks and call ThreadPool::addTasks method which accept std::vector.
 *
 *      Let's take a closer look at both of them.
 *
 *          Option 1. ThreadPoolTask API.
 *          If you are not interested in returned result from sumbitted functions just use ThreadPoolTask::submitRepeated method.
 *          ----> Code example:
 *
 *              int repetitions = 10;
 *
 *              std::vector<std::shared_ptr<IThreadPoolTask>> tasks = ThreadPoolTask::submitRepeated(repetitions, sum, 1, 2);
 *              threadPool.addTasks(tasks);
 *
 *              auto moreTasks = ThreadPoolTask::submitRepeated(repetitions, sum, 3, 4);    // example of using auto
 *              threadPool.addTasks(moreTasks);
 *
 *          Option 2. Manually.
 *          There is only one reasonable case to do it manually:
 *              If you want to add for execution same function N times and you want to get results of each execution (means N results)
 *              then you need to manually submit those functions and store std::future for each (to be able to get the results)
 *          ----> Code example (one of the possible implementations)
 *
 *              std::map<std::shared_ptr<IThreadPoolTask>, std::future<int>> taskToFutureMap;    // 1 step
 *
 *              int repetitions = 10;
 *              for (int i = 0; i < repetitions; ++i)                         // 2 step
 *              {
 *                  auto task = std::make_shared<IThreadPoolTask>();          // 3 step
 *                  auto future = task->submitOne(sum, 1, 2);                 // 4 step
 *                  tasksToFutureMap[task] = std::move(future);               // 5 step
 *              }
 *
 *              std::vector<std::shared_ptr<IThreadPoolTask>> tasks = getKeysAsVector(taskToFutureMap);     // 6 step
 *              threadPool.addTasks(tasks);                                                                 // 7 step
 *
 *          ----> Note to code example:
 *                  Yes, agree, it's a bit messy. But that's the case when you need results of each repeated function execution.
 *                  Keep in mind that it's the most naive implementation.
 *                  Because basically what you need is just store somehow std::future for each task and it's up to you haw to handle this.
 *
 *                  Let me walk you through my example:
 *                      1 step. Create a map with key-task itself and value-corresponding std::future
 *                      2 step. Iterate over desired number of repetitions
 *                      3 step. Create an object of task (have to be new on each iteration)
 *                      4 step. Submit any function to newly created task and save returned std::future
 *                      5 step. Add created task with returned from sumbittion std::future to map
 *                      6 step. Since ThreadPool::addTasks method accepts std::vector of pointers to task you need to extract
 *                              these from our map. Basically you need to create a std::vector which contains only keys from map
 *                      7 step. Pass std:vector of keys (means tasks) to ThreadPool::addTasks.
 *                      8 step. Take a break. You have already read a lot of documentation ^_^
 */
class ThreadPoolTask : public IThreadPoolTask
{
public:

    ThreadPoolTask();
    ThreadPoolTask(const ThreadPoolTask & other) = delete;
    ThreadPoolTask & operator=(const ThreadPoolTask & other) = delete;
    ThreadPoolTask(ThreadPoolTask && other) = delete;
    ThreadPoolTask & operator=(ThreadPoolTask && other) = delete;

    template <typename Function, typename...Args>
    auto submitOne(Function && function, Args &&... args) -> std::future<decltype(function(args...))>;

    template <typename Function, typename...Args>
    static std::vector<std::shared_ptr<IThreadPoolTask>> submitRepeated(const uint32_t repetitions, Function && function, Args &&... args);

public:

    IThreadPoolTask::State getState() const override;
    uint64_t getId() const override;
    Result execute() override;
    Result cancel() override;

protected:

    uint64_t id_;
    std::atomic<IThreadPoolTask::State> state_;
    std::function<void()> wrappedFunction_;
};




template <typename Function, typename...Args>
auto ThreadPoolTask::submitOne(Function && function, Args &&... args) -> std::future<decltype(function(args...))>
{
    using ResultType = decltype(function(args...));

    std::function<ResultType()> bindedFunction{ std::bind(std::forward<Function>(function), std::forward<Args>(args)...) };
    const auto packagedTask = std::make_shared<std::packaged_task<ResultType()>>(bindedFunction);

    wrappedFunction_ = [packagedTask] { (*packagedTask)(); };
    state_.store(IThreadPoolTask::State::SUBMITTED);

    return packagedTask->get_future();
}


template <typename Function, typename...Args>
std::vector<std::shared_ptr<IThreadPoolTask>> ThreadPoolTask::submitRepeated(const uint32_t repetitions, Function && function, Args &&... args) // TODO: Cover with tests
{
    std::vector<std::shared_ptr<IThreadPoolTask>> submittedTasks(repetitions);

    for (auto && taskIt : submittedTasks)
    {
        auto task = std::make_shared<ThreadPoolTask>();
        task->submitOne(std::forward<Function>(function), std::forward<Args>(args)...);

        taskIt = std::move(task);
    }

    return submittedTasks;
}

#endif // _THREADPOOLTASK_H_

