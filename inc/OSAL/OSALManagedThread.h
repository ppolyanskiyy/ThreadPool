#ifndef _MANAGEDTHREAD_H_
#define _MANAGEDTHREAD_H_


#include "OSALThread.h"

namespace OSAL
{
    class ManagedThread : public Thread
    {
    public:

        Result pauseExecution();
        Result resumeExecution();
        Result stopExecution();

    protected:

        ManagedThread(Logging * logging = nullptr);
        ~ManagedThread() override = default;

        /**
         * Method that should contain the thread implementation code.
         * In order to have the managed thread work, this function must not implement an
         * infinite loop, only one loop iteration
         */
        virtual void managedRun() = 0;

    private:

        void run() final;

    };
} // OSAL namespace

#endif // _MANAGEDTHREAD_H_