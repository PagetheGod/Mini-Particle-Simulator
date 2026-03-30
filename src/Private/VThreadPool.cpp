#include "VThreadPool.hpp"

VThreadPool::VThreadPool(size_t NumThreads, bool IsUsingCustomThreadCount)
{
	size_t ThreadCount = std::thread::hardware_concurrency();
	if (!IsUsingCustomThreadCount)
	{
		NumThreads = ThreadCount / 2;
	}
	//Cap the thread count at maxium 3/4 of the hardware concurrency
	if (NumThreads > ThreadCount * 0.75f)
	{
		NumThreads = ThreadCount * 0.75f;
	}

	for (size_t i = 0; i < NumThreads; i++)
	{
		m_Workers.emplace_back(
			[this]()
			{
				while (true)
				{
					std::function<void()> Task;
					{
						std::unique_lock<std::mutex> Lock(this->m_QueueMutex);
						//A condition variable is something that will essentially block the thread's execution until it is notified by others using notify_one or notify_all
						//Conditionals can also take in a predicate, forcing the thread to only continue if the predicate returns true when they receive the notification
						//So in this case, we want the thread to just "sleep" until either the thread pool is stopped, or there are tasks to be executed
						this->m_Condition.wait(Lock, [this]
						{
							return this->m_HasStopped || !this->m_Tasks.empty();
						});
						if(this->m_HasStopped && this->m_Tasks.empty())
						{
							return;
						}
						Task = std::move(this->m_Tasks.front());
						this->m_Tasks.pop();
					}
					Task();
				}
			}
		);
	}
}

VThreadPool::~VThreadPool()
{
	{
		std::unique_lock<std::mutex> Lock(m_QueueMutex);
		m_HasStopped = true;
	}
	m_Condition.notify_all();
	for (std::thread& Worker : m_Workers)
	{
		Worker.join();
	}
}
