#pragma once

#include <functional>

struct SDL_Condition;
struct SDL_Mutex;
struct SDL_Thread;

namespace coldx::detail {

// A fixed set of threads that run one job at a time, each with its own index. On SDL's
// threads, so a platform that SDL runs on supplies them, and one where SDL has none runs
// everything on the caller.
class WorkerPool {
public:
    // count includes the calling thread: 1 starts no threads.
    explicit WorkerPool(int count);
    ~WorkerPool();
    WorkerPool(const WorkerPool&) = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;

    int Count() const { return count_; }

    // Runs job(index, Count()) once for every index, the caller taking index 0, and returns
    // when all have finished. Not reentrant.
    void Run(const std::function<void(int index, int count)>& job);

private:
    static int Main(void* self);
    void Loop(int index);

    int count_ = 1;
    SDL_Thread* threads_[63] = {};
    SDL_Mutex* mutex_ = nullptr;
    SDL_Condition* start_ = nullptr;
    SDL_Condition* done_ = nullptr;
    const std::function<void(int, int)>* job_ = nullptr;
    unsigned generation_ = 0;
    int finished_ = 0;
    bool stop_ = false;
    int nextIndex_ = 1;
};

} // namespace coldx::detail
