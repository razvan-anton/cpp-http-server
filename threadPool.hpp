#include "threadsafeq.hpp"
#include <vector>

class ThreadPool
{
public:
    ThreadPool();

    ~ThreadPool();

    void addTask(Task);

private:
    ThreadSafeQueue TaskQ;
    uint workerCnt;
    std::vector<std::jthread> workers;

    void workerLoop();
};