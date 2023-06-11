#include "OSALMutex.h"
#include "Logging.h"


OSAL::Mutex::Mutex(Logging * logging)
	: isLocked_{ false }
	, logging_{ logging == nullptr ? new Logging{ "OSAL::Mutex" } : logging }
{
}

OSAL::Mutex::~Mutex() = default;


Result OSAL::Mutex::lock(const int64_t timeout)
{
	Result result;

	if (timeout < 0)
	{
		mutex_.lock();
		result = Result::OK;
	}
	else
	{
		bool isLocked{ false };
		OSAL::Timeout lockTimeout{ timeout };

		while (!isLocked && lockTimeout.getRemainingTime() != 0)
		{
			isLocked = mutex_.try_lock_for(std::chrono::microseconds(lockTimeout.getRemainingTime()));
		}

		result = isLocked ? Result::OK : Result::TIMEOUT;
	}

	return result;
}


void OSAL::Mutex::unlock()
{
	mutex_.unlock();
}
