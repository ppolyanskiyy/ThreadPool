#ifndef _PRIORITYTASK_H_
#define _PRIORITYTASK_H_


#include "ThreadPoolTask.h"


enum Priority : uint8_t
{
    FIRST_PRIORITIES_POSITION,   ///< First priorities position. Needs for appropriate iteration over enum values.
    HIGH,                        ///< Hight priority
    NORMAL,                      ///< Normal priority, default value.
    LOW,                         ///< Low priority.
    LAST_PRIORITIES_POSITION     ///< Last priorities position. Needs for appropriate iteration over enum values.
};


class PriorityTask : public ThreadPoolTask
{
public:

    PriorityTask();
    explicit PriorityTask(const Priority priority);

    Priority getPriority() const;
    void setPriority(const Priority priority);

private:

    std::atomic<Priority> priority_;
};




inline Priority& operator++(Priority & priority)
{
    if (priority != Priority::LAST_PRIORITIES_POSITION)
    {
        priority = static_cast<Priority>(static_cast<uint8_t>(priority) + 1u);
    }
    return priority;
}


inline Priority operator++(Priority && priority)
{
    Priority newPriority{ priority };
    return ++newPriority;
}


inline Priority& operator--(Priority & priority)
{
    if (priority != Priority::FIRST_PRIORITIES_POSITION)
    {
        priority = static_cast<Priority>(static_cast<uint8_t>(priority) - 1u);
    }
    return priority;
}


inline Priority operator--(Priority && priority)
{
    Priority newPriority{ priority };
    return --newPriority;
}


#endif // _PRIORITYTASK_H_

