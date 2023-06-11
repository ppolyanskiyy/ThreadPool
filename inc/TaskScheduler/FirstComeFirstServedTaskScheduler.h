#ifndef _FIRSTCOMEFIRSTSERVEDSCHEDULER_H_
#define _FIRSTCOMEFIRSTSERVEDSCHEDULER_H_


#include "TaskSchedulerBase.h"


class FirstComeFirstServedTaskScheduler : public TaskSchedulerBase
{
public:

    explicit FirstComeFirstServedTaskScheduler(Logging * logging = nullptr);

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

    std::deque<std::shared_ptr<IThreadPoolTask>> tasks_;
};

#endif // _FIRSTCOMEFIRSTSERVEDSCHEDULER_H_

