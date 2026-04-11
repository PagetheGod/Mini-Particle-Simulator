//
// Created by YWvin on 2026/3/29.
//

#include "../Public/ThreadPool.hpp"
// This is the main loop that all worker threads are born into.  They
// wait for a signal on the work queue condition variable, then they
// grab work off the queue.  Threads return when they notice that
// m_killthreads is true.

ThreadPool::ThreadPool(size_t num_threads) : m_thread_vec(), m_Mutex(), m_Conditional(),
m_JobQueue(), m_KillThreads(false)
{
    // Initialize our member variables.
    // to pass thread_loop to a thread you need to specify the function as ThreadPool::thread_loop
    // and pass the first argument as `this`
    const size_t hw_threads = std::thread::hardware_concurrency() == 0 ? 4 : std::thread::hardware_concurrency();
    // Clamp to [1, hardware_concurrency - 1] to leave 1 thread for the main thread
    const size_t max_threads = std::max(static_cast<size_t>(1), hw_threads - 1);
    num_threads = std::clamp(num_threads, static_cast<size_t>(1), max_threads);
    for (size_t i = 0; i < num_threads; i++)
    {
        m_thread_vec.push_back(std::jthread(&ThreadPool::thread_loop, this));
    }
}
void ThreadPool::thread_loop() {
    while (true)
    {
        // Have all the threads wait for a job to show up, or the pool is closed
        std::unique_lock lock(m_Mutex);
        m_Conditional.wait(lock, [this]()
        {
            return m_KillThreads || !m_JobQueue.empty();
        });
        // Only return if the thread pool is stopping AND there is no job left
        if (m_KillThreads && m_JobQueue.empty())
        {
            return;
        }
        const Task task = m_JobQueue.front();
        m_JobQueue.pop_front();
        lock.unlock();
        task.func(task.arg);
    }
}

ThreadPool:: ~ThreadPool()
{
    // Wake up all the threads
    {
        // Limit the scope of the lock
        // Reduce critical section size
        std::scoped_lock lock(m_Mutex);
        m_KillThreads = true;
    }
    m_Conditional.notify_all();
    for (auto& thread : m_thread_vec)
    {
        thread.join();
    }
}

// Enqueue a Task for dispatch.
void ThreadPool::dispatch(Task t)
{
    // If the thread had stopped, do not allow for adding more tasks
    std::scoped_lock lock(m_Mutex);
    if (!m_KillThreads)
    {
        m_JobQueue.push_back(t);
        m_Conditional.notify_one();
    }
}
