#include "threadPool.hpp"
#include "tracy/Tracy.hpp"

ThreadPool::ThreadPool()
{
    workerCnt = std::thread::hardware_concurrency() - 1;
    workers.reserve(workerCnt+1);
    for(uint i=0;i<workerCnt;++i)
    {
        workers.emplace_back(&ThreadPool::workerLoop, this,i);
        // optimised version of: workers.push_back(std::jthread(&ThreadPool::worker_loop, this));
    }
}

ThreadPool::~ThreadPool()
{
    for(uint i=0;i<workerCnt;++i)
    {
        TaskQ.push({nullptr});
        // so that every thread when it encounters this, will die
    }
}

void ThreadPool::addTask(Task task)
{
    TaskQ.push(task);
}

void ThreadPool::workerLoop(size_t id)
{
    std::string name = "Worker-" + std::to_string(id);
    tracy::SetThreadName(name.c_str());
    while(true)
    {
        Task task;
        {
            ZoneScopedN("Wait for Task"); // Measures time spent blocked on the queue
            task = TaskQ.popRet();
        }

        if(task.execute==nullptr)
            break; //in order to exit the while

        {
            ZoneScopedN("Execute Task"); // Measures actual HTTP work
            try {
                task.execute(); // Run the sealed lambda
            } 
                catch (const std::exception& e) {
                    // Intercept the error before libstdc++ panics
                    std::cerr << "\n[CRITICAL THREAD EXCEPTION]: " << e.what() << "\n";
                    std::abort(); // Kill the process instantly to prevent recursive looping
            }
                catch (...) {
                    std::cerr << "\n[CRITICAL]: Unknown non-standard exception caught!\n";
                    std::abort();
            }
        }

    }
}
