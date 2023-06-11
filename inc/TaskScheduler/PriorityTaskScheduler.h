#ifndef _PRIORITYTASKSCHEDULER_H_
#define _PRIORITYTASKSCHEDULER_H_


#include "PriorityOrientedTaskSchedulerBase.h"
#include "PriorityTask.h"


class PriorityTaskScheduler : public PriorityOrientedTaskSchedulerBase
{
public:

    explicit PriorityTaskScheduler(Logging * logging = nullptr);

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

    std::unordered_map<Priority, std::deque<std::shared_ptr<IThreadPoolTask>>> priorityToTasksMap_;
};

#endif // _PRIORITYTASKSCHEDULER_H_

