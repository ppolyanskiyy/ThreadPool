#ifndef _TASKSCHEDULERBASE_H_
#define _TASKSCHEDULERBASE_H_


#include "ITaskScheduler.h"
#include "Logging.h"


class TaskSchedulerBase : public ITaskScheduler
{
public:

    /**
     * @note Make sure iterator has std::shared_ptr<IThreadPoolTask> value inside.
     */
    template<typename Iterator>
    static Iterator findTaskById(Iterator beginIt, Iterator endIt, const uint64_t taskId);

    /**
     * @note Make sure iterator has std::shared_ptr<IThreadPoolTask> value inside.
     */
    template<typename Iterator>
    static Iterator partitionTasksById(Iterator && beginIt, Iterator && endIt, const uint64_t taskId, const uint32_t repetitions);

public:

    TaskSchedulerBase(const TaskSchedulerBase &) = delete;
    TaskSchedulerBase & operator=(const TaskSchedulerBase &) = delete;

    ~TaskSchedulerBase() override;

public:

    uint64_t getId() const override;
    Statistic getStatistic() const override;
    void notifyTaskForExecution() const override;

protected:

    explicit TaskSchedulerBase(Logging * logging = nullptr);

protected:

    mutable ITaskScheduler::Statistic statistic_;
    mutable OSAL::Monitor tasksMonitor_;
    mutable bool isNewTaskScheduled_;
    std::unique_ptr<Logging> logging_;

private:

    uint64_t id_;
};




template<typename Iterator>
Iterator TaskSchedulerBase::findTaskById(Iterator beginIt, Iterator endIt, const uint64_t taskId)
{
    const auto foundTaskIt = std::find_if(beginIt, endIt,
                                    [&taskId] (const std::shared_ptr<IThreadPoolTask> & task)
                                    {
                                        return task->getId() == taskId;
                                    });
    return foundTaskIt;
}


template<typename Iterator>
Iterator TaskSchedulerBase::partitionTasksById(Iterator && beginIt, Iterator && endIt, const uint64_t taskId, const uint32_t repetitions)
{
    uint32_t removedTasksCount{ 0u };

    const auto partitionedTasksBeginIt = std::stable_partition(beginIt, endIt,
                                                [&taskId, &removedTasksCount, &repetitions] (const std::shared_ptr<IThreadPoolTask> & task)
                                                {
                                                    if (task->getId() == taskId)
                                                    {
                                                        return !(removedTasksCount++ < repetitions);
                                                    }
                                                    return true;
                                                });


    return partitionedTasksBeginIt;
}


#endif // _TASKSCHEDULERBASE_H_

