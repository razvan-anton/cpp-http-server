#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <iostream>
#include <functional>

typedef struct Task{
    std::function<void()> execute;
} Task;

class ThreadSafeQueue
{
public:

void push(Task el)
{
    {
        std::scoped_lock lk(m);
        q.push(el);
            // if (q.size() % 20==0) {
            // std::cerr << "[DEBUG] Coada a ajuns la: " << q.size() << "\n";
            // }
    }
    // no LOCK now
    cond.notify_one();
}

Task popRet()
{
    std::unique_lock<std::mutex> lk(m);
    cond.wait(lk, [this]{ return (!q.empty());});
    // use the lamba function to continously check the q.empty, in case of spurious wake ups that 
    // the thread wakes up without notify_one() being explicitly called
    Task val=q.front();
    q.pop();
    return val;
}

private:

    std::condition_variable cond;
    mutable std::mutex m;
    std::queue<Task> q;

};
