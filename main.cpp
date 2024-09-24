#include <queue>
#include <functional>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <future>
#include <thread>

using namespace std;


using Task = pair<int, function<void()>>;

class ThreadPool {
    private: 
      int m_threads;
      vector<thread> threads;
      priority_queue<Task, vector<Task>,grater<>> tasks;
      mutex mtx;
      condition_variable cv;
      bool stop;

    public:
    explicit ThreadPool(int numThreads) : m_threads(numThreads), stop(false) {
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back([this] {
                while (true) {
                    function<void()> task;
                    {
                        unique_lock<mutex> lock(mtx);
                        cv.wait(lock, [this] { return !tasks.empty() || stop; });
                        if (stop && tasks.empty()) {
                            return;
                        }
                        task = move(tasks.top().second);
                        tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        {
            {
                unique_lock<mutex> lock(mtx);
                stop = true;
            }
            cv.notify_all();
            for (auto& t : threads) {
                t.join();
            }
        }


    }

    template<class F, class Callback>
    auto executeTask(int priority, F&& f, Callback&& callback)->future<decltype(f())>{
        using return_type = decltype(f());
        auto task = make_shared<packaged_task<return_type()>>(forward<F>(f));
        future<return_type> res = task->get_future();
        {
            unique_lock<mutex> lock(mtx);
            tasks.emplace(priority, [task, callback]{
                try{
                    (*task)();
                    callback();
                }catch(const std::exception& e){
                    std::cerr <"Task failed with exception :"<<e.what()<<endl;
                    try{
                        callback(e);
                    }catch(const std::exception& cb_e){
                        std::cerr << "Callback failed with exception :"<<cb_e.what()<<endl;
                    }catch(...){
                        std::cerr << "Callback failed with unknown exception"<<endl;
                    }
                }catch(...){
                    std::cerr << "Task failed with unknown exception"<<endl;
                    try{
                        callback();
                    }catch(const std::exception& cb_e){
                        std::cerr << "Callback failed with exception :"<<cb_e.what()<<endl;
                    }catch(...){
                        std::cerr << "Callback failed with unknown exception"<<endl;
                    }
                }
            });
        }
        cv.notify_one();
        return res;
    }

};

void myTask() {
    throw std::runtime_error("Simulated task error");  
}

void myCallback(const std::exception& e) {
    std::cout << "Task failed: " << e.what() << std::endl;
}


void asyncFileRead(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        std::cout << line << std::endl;
    }
    file.close();
}

int main() {
    ThreadPool pool(4);

    auto future = pool.executeTask(1, myTask, myCallback);

    future.wait(); 
    std::cout << "Main function complete." << std::endl;

    auto fileFuture = pool.executeTask(1, [] {
        asyncFileRead("example.txt");
    }, [] {
        std::cout << "File read task complete!" << std::endl;
    });

    fileFuture.get();

    return 0;
}