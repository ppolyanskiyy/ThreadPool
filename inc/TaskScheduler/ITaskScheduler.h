#ifndef _ITASKSCHEDULER_H_
#define _ITASKSCHEDULER_H_


#include "IThreadPoolTask.h"


class ITaskScheduler
{
public:

    struct Statistic
    {
        uint32_t totalNumberOfScheduledTasks{ 0u };
        uint32_t totalNumberOfUnscheduledTasks{ 0u };
        uint32_t totalNumberOfStolenTasks{ 0u };
        uint32_t totalNumberOfGotForExecutionTasks{ 0u };

    public:

        inline std::string toString() const
        {
            return "Total number of scheduled tasks : "             + std::to_string(totalNumberOfScheduledTasks)
                 + "\nTotal number of unscheduled tasks : "         + std::to_string(totalNumberOfUnscheduledTasks)
                 + "\nTotal number of stolen tasks : "              + std::to_string(totalNumberOfStolenTasks)
                 + "\nTotal number of got for execution tasks : "   + std::to_string(totalNumberOfGotForExecutionTasks);
        }
    };

    virtual ~ITaskScheduler() = default;

    virtual uint64_t getId() const = 0;
    virtual Statistic getStatistic() const = 0;
    virtual size_t getSize() const = 0;
    virtual Result waitTaskForExecution(const int64_t timeout = -1ll) const = 0;
    virtual void notifyTaskForExecution() const = 0;
    virtual bool isScheduled(const uint64_t taskId) const = 0;
    virtual std::shared_ptr<IThreadPoolTask> getTaskForExecution() = 0;
    virtual std::shared_ptr<IThreadPoolTask> steal() = 0;
    virtual Result schedule(const std::shared_ptr<IThreadPoolTask> task) = 0;
    virtual Result schedule(const std::vector<std::shared_ptr<IThreadPoolTask>> & tasks) = 0;
    virtual std::shared_ptr<IThreadPoolTask> unscheduleOne(const uint64_t taskId) = 0;
    virtual std::vector<std::shared_ptr<IThreadPoolTask>> unscheduleAll() = 0;
    virtual Result clearAll() = 0;
};

#endif // _ITASKSCHEDULER_H_

