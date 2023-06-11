#ifndef _TIME_H_
#define _TIME_H_


#include <stdint.h>
#include <chrono>
#include "Result.h"


namespace OSAL
{
    class Time
    {
    public:

        Time();

        void restart();
        uint64_t getElapsedTime();
        uint64_t getAndRestartTime();

        static uint64_t getElapsedTime(const uint64_t startTime, const uint64_t endTime);

    private:

        std::chrono::steady_clock::time_point startTime_;
    };

} // OSAL

#endif // _TIME_H_