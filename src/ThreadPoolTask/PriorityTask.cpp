#include "PriorityTask.h"


PriorityTask::PriorityTask()
    : priority_{ Priority::NORMAL }
{
}


PriorityTask::PriorityTask(const Priority priority)
    : priority_{ priority }
{
}


Priority PriorityTask::getPriority() const
{
    return priority_.load(std::memory_order_relaxed);
}


void PriorityTask::setPriority(const Priority priority)
{
    priority_.store(priority, std::memory_order_relaxed);
}