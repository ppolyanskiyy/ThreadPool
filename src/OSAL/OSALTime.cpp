#include "OSALTime.h"

OSAL::Time::Time()
    : startTime_{ std::chrono::steady_clock::now() }
{
}


void OSAL::Time::restart()
{
    startTime_ = std::chrono::steady_clock::now();
}


uint64_t OSAL::Time::getElapsedTime()
{
    const auto startTime = static_cast<uint64_t>(
        std::chrono::time_point_cast<std::chrono::microseconds>(startTime_).time_since_epoch().count());
    const auto nowTime = static_cast<uint64_t>(
        std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()).time_since_epoch().count());

    return getElapsedTime(startTime, nowTime);
}


uint64_t OSAL::Time::getAndRestartTime()
{
    const auto elapsedTime = getElapsedTime();
    restart();

    return elapsedTime;
}


uint64_t OSAL::Time::getElapsedTime(uint64_t startTime, uint64_t endTime)
{
    if (endTime > startTime)
    {
        return endTime - startTime;
    }

    return 0;
}
