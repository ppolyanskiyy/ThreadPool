#include "OSALTimeout.h"


OSAL::Timeout::Timeout(const int64_t timeout)
    : timeout_{ timeout }
{
}


int64_t OSAL::Timeout::getRemainingTime()
{
    if (timeout_ < 0)
    {
        return timeout_;
    }

    const auto elapsedTime = static_cast<int64_t>(getElapsedTime());
    const auto remainingTime = timeout_ - elapsedTime;

    return remainingTime < 0 ? 0 : remainingTime;
}
