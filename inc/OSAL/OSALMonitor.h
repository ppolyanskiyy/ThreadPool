#ifndef _MONITOR_H_
#define _MONITOR_H_


#include "OSALMutex.h"
#include <condition_variable>

namespace OSAL
{
    class Monitor : public Mutex
    {
    public:

        Monitor(Logging* logging = nullptr);

        Result wait(const int64_t timeout = -1);
        void notify();
        void notifyAll();

    private:

        std::condition_variable_any condition_;
    };
} // OSAL namespace

#endif // _MONITOR_H_