#include "ThreadPool.hpp"
#include "TestHarness.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

namespace {

struct CounterTaskData {
    std::atomic<int>* counter;
};

void IncrementCounter(void* arg)
{
    auto* data = static_cast<CounterTaskData*>(arg);
    data->counter->fetch_add(1);
}

struct PromiseTaskData {
    std::promise<void>* promise;
};

void FulfillPromise(void* arg)
{
    auto* data = static_cast<PromiseTaskData*>(arg);
    data->promise->set_value();
}

struct SlowTaskData {
    std::atomic<int>* counter;
};

void SlowIncrement(void* arg)
{
    auto* data = static_cast<SlowTaskData*>(arg);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    data->counter->fetch_add(1);
}

}  // namespace

TEST_CASE(ThreadPool_DispatchRunsTasks)
{
    ThreadPool pool(2);
    std::atomic<int> counter = 0;
    std::vector<CounterTaskData> task_data(8, CounterTaskData{&counter});

    for (auto& data : task_data)
    {
        pool.dispatch({&IncrementCounter, &data});
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (counter.load() != static_cast<int>(task_data.size()) &&
           std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    REQUIRE(counter.load() == static_cast<int>(task_data.size()));
}

TEST_CASE(ThreadPool_CanSignalCompletion)
{
    ThreadPool pool(1);
    std::promise<void> promise;
    auto future = promise.get_future();
    PromiseTaskData task_data{&promise};

    pool.dispatch({&FulfillPromise, &task_data});

    REQUIRE(future.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
}

TEST_CASE(ThreadPool_ZeroThreadRequestStillProcessesWork)
{
    ThreadPool pool(0);
    std::promise<void> promise;
    auto future = promise.get_future();
    PromiseTaskData task_data{&promise};

    pool.dispatch({&FulfillPromise, &task_data});

    REQUIRE(future.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
}

TEST_CASE(ThreadPool_DestructorWaitsForQueuedWork)
{
    std::atomic<int> counter = 0;
    std::vector<SlowTaskData> task_data(4, SlowTaskData{&counter});

    {
        ThreadPool pool(2);
        for (auto& data : task_data)
        {
            pool.dispatch({&SlowIncrement, &data});
        }
    }

    REQUIRE(counter.load() == static_cast<int>(task_data.size()));
}
