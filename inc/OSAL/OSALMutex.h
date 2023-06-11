#ifndef _MUTEX_H_
#define _MUTEX_H_


#include "Result.h"
#include <mutex>

class Logging;

namespace OSAL
{
    class Mutex
    {
    public:

        Mutex(Logging * logging = nullptr);
        virtual ~Mutex();

        Result lock(const int64_t timeout = -1);
        void unlock();

    protected:

        std::timed_mutex mutex_;
        std::atomic<bool> isLocked_;
        std::unique_ptr<Logging> logging_;

    };
} // OSAL namespace


#endif // _MUTEX_H_