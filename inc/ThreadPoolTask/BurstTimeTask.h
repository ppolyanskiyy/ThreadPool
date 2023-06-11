#ifndef _BURSTTIMETASK_H_
#define _BURSTTIMETASK_H_


#include "ThreadPoolTask.h"


enum class BurstTime : uint8_t
{
    FIRST_BURST_TIMES_POSITION,     ///< First burst times position. Needs for appropriate iteration over enum values.
    SHORT,                          ///< Short burst time.
    MEDIUM,                         ///< Medium burst time, default value.
    LONG,                           ///< Long burst time.
    LAST_BURST_TIMES_POSITION,      ///< Last burst times position. Needs for appropriate iteration over enum values.
    UNDEFINED                       ///< Undefined burst time. Scheduler will randomly choose value.
};


class BurstTimeTask : public ThreadPoolTask
{
public:

    BurstTimeTask();
    explicit BurstTimeTask(const BurstTime burstTime);

    BurstTime getBurstTime() const;
    void setBurstTime(const BurstTime burstTime);

private:

    std::atomic<BurstTime> burstTime_;
};




inline BurstTime& operator++(BurstTime & burstTime)
{
    if (burstTime != BurstTime::LAST_BURST_TIMES_POSITION)
    {
        burstTime = static_cast<BurstTime>(static_cast<uint8_t>(burstTime) + 1u);
    }
    return burstTime;
}


inline BurstTime operator++(BurstTime && burstTime)
{
    BurstTime newBurstTime{ burstTime };
    return ++newBurstTime;
}


inline BurstTime& operator--(BurstTime & burstTime)
{
    if (burstTime != BurstTime::FIRST_BURST_TIMES_POSITION)
    {
        burstTime = static_cast<BurstTime>(static_cast<uint8_t>(burstTime) - 1u);
    }
    return burstTime;
}


inline BurstTime operator--(BurstTime && burstTime)
{
    BurstTime newBurstTime{ burstTime };
    return --newBurstTime;
}


#endif // _BURSTTIMETASK_H_

