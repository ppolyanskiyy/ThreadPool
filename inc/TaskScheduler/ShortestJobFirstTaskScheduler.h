#ifndef _SHORTESTJOBFIRSTTASKSCHEDULER_H_
#define _SHORTESTJOBFIRSTTASKSCHEDULER_H_


#include "PriorityOrientedTaskSchedulerBase.h"
#include "BurstTimeTask.h"


class ShortestJobFirstTaskScheduler : public PriorityOrientedTaskSchedulerBase
{
public:

    explicit ShortestJobFirstTaskScheduler(Logging * logging = nullptr);

public:

    size_t getSize() const override;
    Result waitTaskForExecution(const int64_t timeout = -1ll) const override;
    bool isScheduled(const uint64_t taskId) const override;
    
    std::shared_ptr<IThreadPoolTask> getTaskForExecution() override;
    std::shared_ptr<IThreadPoolTask> steal() override;
    Result schedule(const std::shared_ptr<IThreadPoolTask> task) override;
    Result schedule(const std::vector<std::shared_ptr<IThreadPoolTask>> & tasks) override;
    std::shared_ptr<IThreadPoolTask> unscheduleOne(const uint64_t taskId) override;
    std::vector<std::shared_ptr<IThreadPoolTask>> unscheduleAll() override;
    Result clearAll() override;

private:

    BurstTime calculateBurstTime(BurstTime burstTime) const;

private:

    std::unordered_map<BurstTime, std::deque<std::shared_ptr<IThreadPoolTask>>> burstTimeToTasksMap_;
};

#endif // _SHORTESTJOBFIRSTTASKSCHEDULER_H_

