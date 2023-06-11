#include "OSALThread.h"
#include "OSALTimeout.h"
#include "Logging.h"
#include <thread>


OSAL::Thread::Thread(Logging * logging)
	: threadMustEnd_{ false }
	, isFinished_{ true }
	, state_{ State::READY }
	, logging_{ logging == nullptr ? new Logging{ "OSAL::Thread" } : logging }
{
	static std::atomic<uint64_t> id{ 1u };
	id_ = id.load();
	id.fetch_add(1u);
}


OSAL::Thread::~Thread()
{
	if (thread_.joinable())
	{
		threadMustEnd_ = true;
		thread_.detach();
	}
}


Result OSAL::Thread::create()
{
	Result result{ Result::ERROR };

	stateMonitor_.lock();

	if (state_ == State::READY)
	{
		isFinished_ = false;

		thread_ = std::thread([&]()
					{
						this->run();

						finishedMonitor_.lock();
						isFinished_ = true;
						finishedMonitor_.notifyAll();
						finishedMonitor_.unlock();
					});

		state_ = State::WAITING;
		result = Result::OK;
	}
	else
	{
		logging_->logWarning("Thread with id %" PRIu64 " is alredy created", id_);
	}

	stateMonitor_.unlock();
	
	return result;
}


bool OSAL::Thread::waitFinished(const int64_t timeout, const bool breakThreadLoop)
{
	if (breakThreadLoop)
	{
		threadMustEnd_ = true;
	}

	Result result{ Result::OK };
	
	finishedMonitor_.lock();

	OSAL::Timeout waitTimeout{ timeout };

	while (Result::OK == result && !isFinished_)
	{
		result = finishedMonitor_.wait(waitTimeout.getRemainingTime());
	}

	finishedMonitor_.unlock();

	stateMonitor_.lock();
	state_ = State::FINISHED;
	stateMonitor_.unlock();

	return result == Result::OK;
}


uint64_t OSAL::Thread::getId() const
{
	return id_;
}


OSAL::Thread::State OSAL::Thread::getState() const
{
	stateMonitor_.lock();
	const State state{ state_ };
	stateMonitor_.unlock();

	return state;
}


void OSAL::Thread::delay(const uint64_t timeout)
{
	std::this_thread::sleep_for(std::chrono::microseconds(timeout));
}
