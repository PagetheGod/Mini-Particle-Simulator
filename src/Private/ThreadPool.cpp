//
// Created by YWvin on 2026/3/29.
//

#include "../Public/ThreadPool.hpp"
// This is the main loop that all worker threads are born into.  They
// wait for a signal on the work queue condition variable, then they
// grab work off the queue.  Threads return when they notice that
// m_killthreads is true.

ThreadPool::ThreadPool(size_t num_threads, bool IsUsingCustomThreadCoutn) : m_thread_vec(), m_Mutex(), m_Conditional(),
m_JobQueue(), m_KillThreads(false) {

    // Initialize our member variables.

    // TODO
    // to pass thread_loop to a thread you need to specify the function as ThreadPool::thread_loop
    // and pass the first argument as `this`

}
void ThreadPool::thread_loop() {
    // TODO
    while (true)
    {

    }
}

ThreadPool:: ~ThreadPool() {
    // TODO
}

// Enqueue a Task for dispatch.
void ThreadPool::dispatch(Task t) {
    // TODO
}