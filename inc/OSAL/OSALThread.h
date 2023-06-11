#ifndef _THREAD_H_
#define _THREAD_H_


#include "OSALMonitor.h"
#include <string>
#include <thread>

namespace OSAL
{
    class Thread
    {
    public:

        enum class State : uint8_t
        {
            READY,          ///< Ready state. When thread is created and waiting to start execution.
            RUNNING,        ///< Running state. When thread is running.
            WAITING,        ///< Waiting state. When thread is waiting for execution.
            FINISHED,       ///< Finished state. When thread is requested to wait for finished execution.
            STOPPED,        ///< Stopped state. When worker is stopped.
            PAUSED          ///< Paused state. When worker is paused.
        };

        Thread(Logging * logging = nullptr);
        virtual ~Thread();

        Result create();
        bool waitFinished(const int64_t timeout, const bool breakThreadLoop = true);
        
        uint64_t getId() const;
        State getState() const;

        static void delay(const uint64_t timeout);

    protected:

        virtual void run() = 0;

    protected:

        uint64_t id_;
        std::thread thread_;
        bool threadMustEnd_;
        bool isFinished_;
        mutable OSAL::Monitor finishedMonitor_;
        mutable OSAL::Monitor stateMonitor_;
        State state_;
        std::unique_ptr<Logging> logging_;
    };
}


#endif // _THREAD_H_