#ifndef _ITHREADPOOLTASK_H_
#define _ITHREADPOOLTASK_H_


#include "OSAL.h"


class IThreadPoolTask
{
public:

    enum class State : uint8_t
    {
        CREATED,            ///< Initial state when task is created.
        SUBMITTED,          ///< State when task is sumbitted.
        IN_EXECUTION,       ///< State when task start execution.
        EXECUTED,           ///< State when task finish execution.
        CANCELED            ///< State when task is canceled.
    };

    virtual ~IThreadPoolTask () = default;

    virtual State getState() const = 0;
    virtual uint64_t getId() const = 0;
    virtual Result execute() = 0;
    virtual Result cancel() = 0;
};

#endif // _ITHREADPOOLTASK_H_

