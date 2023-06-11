#ifndef _THREADPOOLOPTIONSBUILDER_H_
#define _THREADPOOLOPTIONSBUILDER_H_


#include "ThreadPoolOptions.h"


class ThreadPoolOptionsBuilder
{
public:

    explicit ThreadPoolOptionsBuilder(const uint32_t initialNumberOfWorkers);

    ThreadPoolOptionsBuilder & setSchedulerType(const ThreadPoolOptions::SchedulerType schedulerType);
    ThreadPoolOptionsBuilder & setMinNumberOfWorkers(const uint32_t minNumberOfWorkers);
    ThreadPoolOptionsBuilder & setMaxNumberOfWorkers(const uint32_t maxNumberOfWorkers);
    ThreadPoolOptionsBuilder & setPostponeExecution(const bool postponeExecution = true);
    ThreadPoolOptionsBuilder & setWaitAllTasksExecutionFinished(const bool waitAllTasksExecutionFinished = true);

    ThreadPoolOptions build() const;

private:

    ThreadPoolOptions options_;
};


#endif // _THREADPOOLOPTIONSBUILDER_H_

