#ifndef _TIMEOUT_H_
#define _TIMEOUT_H_


#include "OSALTime.h"
#include <stdint.h>


namespace OSAL
{
    class Timeout : private Time
    {
    public:

        /**
         * @param timeout Time in microseconds, -1 for infinite
         */
        explicit Timeout(const int64_t timeout);

        /**
         * @return Remaining time for timeout expiration in microseconds.
         *         Once timeout is expired, it will return 0
         *         It only returns negative value if timeout is infinite (-1)
         */
        int64_t getRemainingTime();

    private:

        int64_t timeout_;
    };

} // OSAL


#endif // _TIMEOUT_H_