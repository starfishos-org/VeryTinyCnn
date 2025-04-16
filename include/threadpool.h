#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>
#include <functional>
#include <string>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>

namespace tnn {
    extern "C" {
        extern std::vector<int> thread_bind_cpu_list;
        extern bool external_thread_bind_cpu;
        extern std::string thread_bind_cpu_list_filename;
        extern int g_threads_num;
        extern uint32_t g_thread_cnt;
    }
    void proc_bind_thread(int cpu_id) {
        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(cpu_id, &mask);
        if (sched_setaffinity(0, sizeof(mask), &mask) == -1) {
            perror("sched_setaffinity");
        }
        sched_yield();
    }

    class thread_pool {
    public:
        thread_pool(std::size_t threads_n = std::thread::hardware_concurrency()) : stop(false) {
            for(int i = 0; i < threads_n; i++) {
                workers.emplace_back(std::bind(&thread_pool::run, this, i));
            }
        }
        thread_pool(const thread_pool &) = delete;
        thread_pool &operator = (const thread_pool &) = delete;
        thread_pool(thread_pool &&) = delete;
        thread_pool &operator = (thread_pool &&) = delete;
        template<class F, class... Args>
        std::future<typename std::result_of<F(Args...)>::type> enqueue(F&& f, Args&&... args) {
            using packaged_task_t = std::packaged_task<typename std::result_of<F(Args...)>::type ()>;
            std::shared_ptr<packaged_task_t> task(new packaged_task_t(
                    std::bind(std::forward<F>(f), std::forward<Args>(args)...)
            ));
            auto res = task->get_future();
            {
                std::unique_lock<std::mutex> lock(this->queue_mutex);
                tasks.emplace([task](){ (*task)(); });
            }
            condition.notify_one();
            return res;
        }
        std::size_t get_thread_num() const {
            return workers.size();
        }
        void run(int cpu_id) {
            proc_bind_thread(cpu_id);
            while(true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    condition.wait(lock, [this]{ return stop || !tasks.empty(); });
                    if(stop && tasks.empty())
                        return;
                    task = std::move(tasks.front());
                    tasks.pop();
                }
                task();
            }
        }
        void run_once() {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                condition.wait(lock, [this]{ return stop || !tasks.empty(); });
                if(stop && tasks.empty())
                    return;
                task = std::move(tasks.front());
                tasks.pop();
            }
            task();
        }
        ~thread_pool() {
            this->stop = true;
            this->condition.notify_all();
            for(std::thread &worker: this->workers)
                worker.join();
        }
    private:
        std::vector<std::thread> workers;
        std::queue<std::function<void()> > tasks;
        std::mutex queue_mutex;
        std::condition_variable condition;
        std::atomic_bool stop;
    };

    int parse_cpu_bind_file(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::fprintf(stderr, "Failed to open CPU bind file: %s\n", filename.c_str());
            return -1;
        }

            external_thread_bind_cpu = true;

        std::string line;
        if (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string token;
            while (std::getline(iss, token, ',')) {
                // 去除前后空白
                token.erase(0, token.find_first_not_of(" \t"));
                token.erase(token.find_last_not_of(" \t") + 1);

                size_t dash_pos = token.find('-');
                if (dash_pos != std::string::npos) {
                    int start = std::stoi(token.substr(0, dash_pos));
                    int end = std::stoi(token.substr(dash_pos + 1));
                    if (start > end) {
                        std::fprintf(stderr, "Invalid range: %s\n", token.c_str());
                        continue;
                    }
                    for (int i = start; i <= end; ++i) {
                        thread_bind_cpu_list.push_back(i);
                    }
                } else {
                    int cpu = std::stoi(token);
                    thread_bind_cpu_list.push_back(cpu);
                }
            }
        }

        if (thread_bind_cpu_list.empty()) {
            std::fprintf(stderr, "No CPU bind data found\n");
            return -1;
        }
        
        if (thread_bind_cpu_list.size() < static_cast<size_t>(g_thread_cnt)) {
            std::fprintf(stderr, "CPU bind file size is less than thread number\n");
            return -1;
        }

        std::fprintf(stderr, "bind %zu cpu: ", thread_bind_cpu_list.size());
        for (int cpu : thread_bind_cpu_list) {
            std::fprintf(stderr, "%d ", cpu);
        }
        std::fprintf(stderr, "\n");

        return 0;
    }
}

#endif