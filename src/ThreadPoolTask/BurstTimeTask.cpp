#include "BurstTimeTask.h"


BurstTimeTask::BurstTimeTask()
    : burstTime_{ BurstTime::MEDIUM }
{
}


BurstTimeTask::BurstTimeTask(const BurstTime burstTime)
    : burstTime_{ burstTime }
{
}


BurstTime BurstTimeTask::getBurstTime() const
{
    return burstTime_.load(std::memory_order_relaxed);
}


void BurstTimeTask::setBurstTime(const BurstTime burstTime)
{
    burstTime_.store(burstTime, std::memory_order_relaxed);
}