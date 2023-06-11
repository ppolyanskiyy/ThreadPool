#include "ThreadPoolTask.h"


ThreadPoolTask::ThreadPoolTask()
    : state_{ IThreadPoolTask::State::CREATED }
{
    static std::atomic<uint64_t> id{ 1u };
    id_ = id.load();
    id.fetch_add(1u);
}

///////////////////////////////////////////////////////////////////////////////////////////////
///
/// Public IThreadPoolTask methods
///
///////////////////////////////////////////////////////////////////////////////////////////////

IThreadPoolTask::State ThreadPoolTask::getState() const
{
    return state_.load();
}


uint64_t ThreadPoolTask::getId() const
{
    return id_;
}


Result ThreadPoolTask::execute()
{
    Result result{ Result::CANCELED };

    if (!wrappedFunction_)
    {
        result = Result::ERROR;
    }
    else if (state_.load() == IThreadPoolTask::State::SUBMITTED)
    {
        state_.store(IThreadPoolTask::State::IN_EXECUTION);
        wrappedFunction_();
        state_.store(IThreadPoolTask::State::EXECUTED);

        result = Result::OK;
    }

    return result;
}


Result ThreadPoolTask::cancel()
{
    Result result{ Result::CANCELED };

    if (state_.load() != IThreadPoolTask::State::CANCELED)
    {
        state_.store(IThreadPoolTask::State::CANCELED);
        result = Result::OK;
    }

    return result;
}
