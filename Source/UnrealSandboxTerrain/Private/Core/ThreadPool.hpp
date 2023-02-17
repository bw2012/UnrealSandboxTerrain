
#include <iostream>
#include <atomic>
#include <vector>
#include <list>
#include <mutex>
#include <functional>
#include <thread>

class TConveyour {

private:
    std::mutex m;

    std::list<std::function<void()>> list;

    std::atomic<int> s{0};

public:

    void push(std::function<void()> f) {
        const std::lock_guard<std::mutex> lock(m);
        list.push_back(f);
        s++;
    }

    bool pop(std::function<void()>& f) {
        const std::lock_guard<std::mutex> lock(m);

        if (list.size() == 0) {
            return false;
        }

        f = list.front();
        list.pop_front();
        s--;

        return true;
    }

    int size() {
        return s.load();
    }

};


class TThreadPool {

private:

    std::atomic<int> task_size;

    std::atomic_flag shutdown;

    std::mutex mutex;

    std::condition_variable cv;

    std::vector<std::thread> thread_list;

    std::list<std::function<void()>> task_list;

    void run() {
        while (!shutdown.test()) {
            std::unique_lock lock(mutex);
            cv.wait(lock, [this]()->bool { return task_list.size() > 0 || shutdown.test(); });

            if (task_list.size() > 0 && !shutdown.test()) {
                std::function<void()> f = task_list.front();
                task_list.pop_front();
                task_size--;
                lock.unlock();
                f();

            }
        }
    };

public:

    TThreadPool(int num) {
        task_size = 0;
        shutdown.clear();

        for (int i = 0; i < num; i++) {
            //TODO linux
            std::thread t(&TThreadPool::run, this);
            thread_list.push_back(std::move(t));
        }
    };

    ~TThreadPool() {
        shutdownAndWait();
    };

    void addTask(const std::function<void()> task, bool bHiPrio = false) {
        std::lock_guard<std::mutex> lock(mutex);
        task_size++;

        if (bHiPrio) {
            task_list.push_front(task);
        } else {
            task_list.push_back(task);
        }

        cv.notify_one();
    }

    void shutdownAndWait() {
        shutdown.test_and_set();
        cv.notify_all();

        //TODO linux 

        for (auto& t : thread_list) {
            t.join();
        }

        thread_list.clear();
        task_size = 0;
    }

    int size() {
        return task_size;
    }

};
