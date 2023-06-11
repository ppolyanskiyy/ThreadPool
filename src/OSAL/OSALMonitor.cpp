#include "OSALMonitor.h"
#include "Logging.h"


OSAL::Monitor::Monitor(Logging* logging)
    : Mutex{ logging }
{
}


Result OSAL::Monitor::wait(const int64_t timeout)
{
    std::unique_lock<std::timed_mutex> lock(mutex_, std::adopt_lock);
    
    if (timeout < 0) 
    {
        condition_.wait(lock);
        return Result::OK;
    }
    else 
    {
        const auto duration = std::chrono::microseconds(timeout);
        return (condition_.wait_for(lock, duration) == std::cv_status::timeout) ? Result::TIMEOUT : Result::OK;
    }
}


void OSAL::Monitor::notify()
{
    std::lock_guard<std::timed_mutex> lock(mutex_, std::adopt_lock);
    condition_.notify_one();
}


void OSAL::Monitor::notifyAll()
{
    std::lock_guard<std::timed_mutex> lock(mutex_, std::adopt_lock);
    condition_.notify_all();
}
