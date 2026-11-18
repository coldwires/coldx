#include "worker_pool.h"

#include <SDL3/SDL.h>

namespace coldx::detail {

WorkerPool::WorkerPool(int count) {
    if (count < 1) count = 1;
    if (count > 64) count = 64;
    mutex_ = SDL_CreateMutex();
    start_ = SDL_CreateCondition();
    done_ = SDL_CreateCondition();
    if (!mutex_ || !start_ || !done_) {
        SDL_Log("coldx.core: worker threads unavailable, drawing on one: %s", SDL_GetError());
        return;
    }
    for (int i = 1; i < count; ++i) {
        // Each thread reads its index from nextIndex_ under the lock.
        threads_[i - 1] = SDL_CreateThread(&WorkerPool::Main, "coldx worker", this);
        if (!threads_[i - 1]) {
            SDL_Log("coldx.core: started %d of %d worker threads: %s", i - 1, count - 1, SDL_GetError());
            break;
        }
        count_ = i + 1;
    }
}

WorkerPool::~WorkerPool() {
    if (mutex_) {
        SDL_LockMutex(mutex_);
        stop_ = true;
        SDL_BroadcastCondition(start_);
        SDL_UnlockMutex(mutex_);
    }
    for (SDL_Thread* t : threads_)
        if (t) SDL_WaitThread(t, nullptr);
    SDL_DestroyCondition(done_);
    SDL_DestroyCondition(start_);
    SDL_DestroyMutex(mutex_);
}

int WorkerPool::Main(void* self) {
    auto* pool = static_cast<WorkerPool*>(self);
    SDL_LockMutex(pool->mutex_);
    const int index = pool->nextIndex_++;
    SDL_UnlockMutex(pool->mutex_);
    pool->Loop(index);
    return 0;
}

void WorkerPool::Loop(int index) {
    unsigned seen = 0;
    SDL_LockMutex(mutex_);
    for (;;) {
        while (!stop_ && generation_ == seen) SDL_WaitCondition(start_, mutex_);
        if (stop_) break;
        seen = generation_;
        const std::function<void(int, int)>* job = job_;
        const int count = count_;
        SDL_UnlockMutex(mutex_);
        (*job)(index, count);
        SDL_LockMutex(mutex_);
        if (++finished_ == count - 1) SDL_SignalCondition(done_);
    }
    SDL_UnlockMutex(mutex_);
}

void WorkerPool::Run(const std::function<void(int, int)>& job) {
    if (count_ == 1) {
        job(0, 1);
        return;
    }
    SDL_LockMutex(mutex_);
    job_ = &job;
    finished_ = 0;
    ++generation_;
    SDL_BroadcastCondition(start_);
    SDL_UnlockMutex(mutex_);
    job(0, count_);
    SDL_LockMutex(mutex_);
    while (finished_ < count_ - 1) SDL_WaitCondition(done_, mutex_);
    SDL_UnlockMutex(mutex_);
}

} // namespace coldx::detail
