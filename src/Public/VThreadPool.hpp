#pragma once

#include <functional>
#include <vector>
#include <thread>
#include <condition_variable>
#include <queue>
#include <mutex>
#include <future>
#include <memory>

//A basic thread pool. Credit goes to: https://github.com/progschj/ThreadPool/blob/master/ThreadPool.h
/*
* I made the following changes:
* 1. Changed the std::result_of to std::invoke_result (in SubmitTask) because the former was deprecated in C++ 17
* 2. Added logics in the constructor to allow user(which is myself) to choose whether they want to specify a custom thread count.
* 3. Added logics in the constructor to cap our max worker thread counts at 3/4 the user's concurrency
* 4. Getter to ask whether the thread pool had stopped
*/

// Iso -
/*
 * This is the thread pool that I had been using, it's much easier to use than the one provided in class
 * It does not require the use of a void*, or a dedicated struct just to pass arguments
 * Although it does make very heavy use of templates, so it can be confusing to read
 * And I also added a bit of restriction to the maximum number of threads this can use
 * I keep this here for testing and references
 */

class VThreadPool
{
public:
	explicit VThreadPool(size_t NumThreads = 2, bool IsUsingCustomThreadCount = false);
	~VThreadPool();

	[[nodiscard]] bool HasStopped() const
	{
		return m_HasStopped;
	}

	/*
	* This part is where the flexibility comes in. Use a template coupled with future so we can query the result of an async task
	* Note: 1. Invoke_result deduces the return type of a function(given its arguments) at compile time
	*/
	template<typename Func, class... Args>
	auto SubmitTask(Func&& InFunc, Args&&... InArgs) -> std::future<std::invoke_result_t<Func, Args...>>;
private:
	std::vector<std::thread> m_Workers;
	std::queue<std::function<void()>> m_Tasks;
	std::mutex m_QueueMutex;
	std::condition_variable m_Condition;
	bool m_HasStopped = false;
};

template<typename Func, class ...Args>
inline auto VThreadPool::SubmitTask(Func&& InFunc, Args && ...InArgs) -> std::future<std::invoke_result_t<Func, Args ...>>
{
	//The majority of the function logics are about using std::future, so we can query it later
	using ReturnType = typename std::invoke_result<Func, Args ...>::type;
	/*
	* 1. std::packaged_task wraps a callable element and allows its result to be retrieved asynchronously, it passes its resualt to a std::future object
	* 2. std::bind creates a new callable object using the given function and arguments. It basically creates a lambda that captures the function and its arguments, this lambda receive no arguments
	* 3. Packaged task is move-only, so we use a shared pointer to pass it to the std::function and to the queue, while also keeping it alive until it is executed
	*/
	auto Task = std::make_shared<std::packaged_task<ReturnType()>>(
		std::bind(std::forward<Func>(InFunc), std::forward<Args>(InArgs)...)
	);

	std::future<ReturnType> Result = Task->get_future();
	{
		std::unique_lock<std::mutex> Lock(m_QueueMutex);
		if (m_HasStopped)
		{
			throw std::runtime_error("Exception: Submitting to a stopped Thread Pool!");
		}
		m_Tasks.emplace([Task]()
		{
			(*Task)();
		});
	}
	m_Condition.notify_one();
	return Result;
}
