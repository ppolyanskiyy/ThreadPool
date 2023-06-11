#ifndef _THREADPOOLOPTIONS_H_
#define _THREADPOOLOPTIONS_H_


#include "OSAL.h"


class ThreadPoolOptions
{
public:

    enum class SchedulerType : uint8_t
    {
        FCFS,           ///< First Come First Served, default value.
        PRIORITY,       ///< Priority based.
        SJF,            ///< Shortest Job First.
        UNDEFINED       ///< Undefined scheduler type.
    };


    static std::string schedulerTypeToString(const SchedulerType schedulerType)
    {
        switch (schedulerType)
        {
            case SchedulerType::FCFS:       return "FCFS";
            case SchedulerType::PRIORITY:   return "PRIORITY";
            case SchedulerType::SJF:        return "SJF";
            default:                        return "UNDEFINED";
        }
    };

    static SchedulerType stringToSchedulerType(const std::string & schedulerType)
    {
        std::string upperCaseSchedulerType;
        std::transform(schedulerType.begin(), schedulerType.end(), upperCaseSchedulerType.begin(), [](const std::string::value_type c) { return std::toupper(c); });

        if ("FCFS" == upperCaseSchedulerType)           return SchedulerType::FCFS;
        if ("PRIORITY" == upperCaseSchedulerType)       return SchedulerType::PRIORITY;
        if ("SJF" == upperCaseSchedulerType)            return SchedulerType::SJF;

        return SchedulerType::UNDEFINED;
    };

    ThreadPoolOptions(const SchedulerType schedulerType,
                      const uint32_t initialNumberOfWorkers, const uint32_t minNumberOfWorkers, const uint32_t maxNumberOfWorkers,
                      const bool needsPostponeExecution = false, const bool needsWaitAllTasksExecutionFinished = false);

    ThreadPoolOptions(const uint32_t initialNumberOfWorkers, const uint32_t minNumberOfWorkers, const uint32_t maxNumberOfWorkers,
                      const bool needsPostponeExecution = false, const bool needsWaitAllTasksExecutionFinished = false);

    explicit ThreadPoolOptions(const uint32_t initialNumberOfWorkers, const bool needsPostponeExecution = false, const bool needsWaitAllTasksExecutionFinished = false);

    explicit ThreadPoolOptions(const bool needsPostponeExecution = false, const bool needsWaitAllTasksExecutionFinished = false);

    SchedulerType getSchedulerType() const;
    void setSchedulerType(const SchedulerType schedulerType);

    uint32_t getInitialNumberOfWorkers() const;
    void setInitialNumberOfWorkers(const uint32_t value);

    uint32_t getMinNumberOfWorkers() const;
    void setMinNumberOfWorkers(const uint32_t value);

    uint32_t getMaxNumberOfWorkers() const;
    void setMaxNumberOfWorkers(const uint32_t value);

    bool needsPostponeExecution() const;
    void setPostponeExecution(const bool needsPostponeExecution = true);

    bool needsWaitAllTasksExecutionFinished() const;
    void setWaitAllTasksExecutionFinished(const bool needsWaitAllTasksExecutionFinished = true);

    std::string toString() const;

private:

    SchedulerType schedulerType_;
    uint32_t initialNumberOfWorkers_;
    uint32_t minNumberOfWorkers_;
    uint32_t maxNumberOfWorkers_;
    bool needsPostponeExecution_;
    bool needsWaitAllTasksExecutionFinished_;
};

#endif // _THREADPOOLOPTIONS_H_

