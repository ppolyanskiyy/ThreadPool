#include "OSALManagedThread.h"
#include "Logging.h"


Result OSAL::ManagedThread::pauseExecution()
{
    Result result{ Result::ERROR };

    stateMonitor_.lock();

    if (state_ != State::READY && state_ != State::PAUSED)
    {
        state_ = State::PAUSED;
        result = Result::OK;
    }

    stateMonitor_.unlock();

    return result;
}


Result OSAL::ManagedThread::resumeExecution()
{
    Result result{ Result::ERROR };

    stateMonitor_.lock();

    if (State::PAUSED == state_)
    {
        state_ = State::RUNNING;
        result = Result::OK;
    }

    stateMonitor_.notify();
    stateMonitor_.unlock();

    return result;
}


Result OSAL::ManagedThread::stopExecution()
{
    Result result{ Result::ERROR };

    stateMonitor_.lock();

    if (state_ != State::STOPPED)
    {
        state_ = State::STOPPED;
        threadMustEnd_ = true;

        result = Result::OK;
    }

    stateMonitor_.notify();
    stateMonitor_.unlock();

    return result;
}


OSAL::ManagedThread::ManagedThread(Logging * logging)
    : Thread{ logging }
{
}


void OSAL::ManagedThread::run()
{
    while (!threadMustEnd_)
    {
        stateMonitor_.lock();

        if (State::PAUSED == state_)
        {
            while (Result::OK == stateMonitor_.wait() && State::PAUSED == state_)
            {
            }
            stateMonitor_.unlock();
        }

        if (State::RUNNING == state_ || State::WAITING == state_)
        {
            stateMonitor_.unlock();
            managedRun();
        }

        if (State::STOPPED == state_ || State::FINISHED == state_)
        {
            threadMustEnd_ = true;
            stateMonitor_.unlock();
        }
    }
}
