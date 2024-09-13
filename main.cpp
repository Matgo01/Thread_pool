#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <chrono>

using namespace std;


class ThreadPool {
private:
    int m_threads;
    vector<thread> threads;
    queue<function<void()>> tasks;
    mutex mtx;
    condition_variable cv;
    bool stop;

public:

    explicit ThreadPool(int numThreads) : stop(false) {
        // Start the thread pool
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back([this] {
                while (true) {
                    function<void()> task; 

                    // Lock and wait for a task or stop signal
                    unique_lock<mutex> lock(mtx);
                    cv.wait(lock, [this] { return !tasks.empty() || stop; });

                    // If stopping and no tasks left, exit the loop
                    if (stop && tasks.empty()) {
                        return;
                    }

                    // Move the task from the queue
                    task = move(tasks.front());
                    tasks.pop();

                    // Unlock before running the task
                    lock.unlock();

                    // Execute the task
                    task();
                }
            });
        }
    }

    // Destructor to stop the thread pool
    ~ThreadPool() {
        {
            unique_lock<mutex> lock(mtx);
            stop = true;
        }

        cv.notify_all(); // Notify all threads to stop

        // Join all threads
        for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    // Task execution function
    template<class F, class... Args>
    auto ExecuteTask(F&& f, Args&&... args) -> future<decltype(f(args...))> {
        using return_type = decltype(f(args...));

        // Package the task
        auto task = make_shared<packaged_task<return_type()>>(
            bind(forward<F>(f), forward<Args>(args)...)
        );

        future<return_type> res = task->get_future();

        {
            unique_lock<mutex> lock(mtx);

            // Add the task to the queue
            tasks.push([task]() { (*task)(); });
        }

        cv.notify_one(); // Notify one thread that a task is available
        return res;
    }
};

// Test function for the thread pool
int Func(int a) {
    this_thread::sleep_for(chrono::seconds(5));
    cout << "This is a time-consuming function." << endl;
    return a * a;
}

int main() {
    ThreadPool pool(8); // Initialize pool with 8 threads

    // Execute a task asynchronously
    future<int> res = pool.ExecuteTask(Func, 2);

    // Get the result from the task
    cout << "Result is: " << res.get() << endl;

    cout << "Thread pool program finished." << endl;
    return 0;
}
