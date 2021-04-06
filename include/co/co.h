#pragma once

#include "def.h"
#include "closure.h"
#include "co/sock.h"

namespace co {

// Add a task, which will run as a coroutine.
// Supported function types:
//   void f();
//   void f(void*);          // func with a param
//   void T::f();            // method in a class
//   std::function<void()>;
void go(Closure* cb);

inline void go(void (*f)()) {
    go(new_closure(f));
}

inline void go(void (*f)(void*), void* p) {
    go(new_closure(f, p));
}

template<typename T>
inline void go(void (T::*f)(), T* p) {
    go(new_closure(f, p));
}

// !! Performance of std::function is poor. Try to avoid it.
inline void go(const std::function<void()>& f) {
    go(new_closure(f));
}

inline void go(std::function<void()>&& f) {
    go(new_closure(std::move(f)));
}

void sleep(unsigned int ms);

// stop coroutine schedulers
void stop();

// max number of schedulers. It is os::cpunum() right now.
// scheduler id is from 0 to max_sched_num-1.
int max_sched_num();

// id of the current scheduler, -1 for non-scheduler
int sched_id();

// id of the current coroutine, -1 for non-coroutine
int coroutine_id();

// co::Event is for communications between coroutines.
// It's similar to SyncEvent for threads.
class Event {
  public:
    Event();
    ~Event();

    Event(Event&& e) : _p(e._p) { e._p = 0; }

    Event(const Event&) = delete;
    void operator=(const Event&) = delete;

    // MUST be called in coroutine
    void wait();

    // return false if timeout
    // MUST be called in coroutine
    bool wait(unsigned int ms);

    // wakeup all waiting coroutines
    // can be called from anywhere
    void signal();

  private:
    void* _p;
};

// co::Mutex is a mutex lock for coroutines.
// It's similar to Mutex for threads.
class Mutex {
  public:
    Mutex();
    ~Mutex();

    Mutex(Mutex&& m) : _p(m._p) { m._p = 0; }

    Mutex(const Mutex&) = delete;
    void operator=(const Mutex&) = delete;

    // MUST be called in coroutine
    void lock();

    // can be called from anywhere
    void unlock();

    // can be called from anywhere
    bool try_lock();

  private:
    void* _p;
};

class MutexGuard {
  public:
    explicit MutexGuard(co::Mutex& lock) : _lock(lock) {
        _lock.lock();
    }

    explicit MutexGuard(co::Mutex* lock) : _lock(*lock) {
        _lock.lock();
    }

    MutexGuard(const MutexGuard&) = delete;
    void operator=(const MutexGuard&) = delete;

    ~MutexGuard() {
        _lock.unlock();
    }

  private:
    co::Mutex& _lock;
};

// co::Pool is a general pool for coroutines.
// It stores void* pointers internally.
// It is coroutine-safe. Each thread has its own pool.
class Pool {
  public:
    Pool();
    ~Pool();

    // @ccb:  a create callback       []() { return (void*) new T; }
    //   when pop from an empty pool, this callback is used to create an element
    //
    // @dcb:  a destroy callback      [](void* p) { delete (T*)p; }
    //   this callback is used to destroy an element when needed
    //
    // @cap:  max capacity of the pool for each thread
    //   this argument is ignored if the destory callback is not set
    Pool(std::function<void*()>&& ccb, std::function<void(void*)>&& dcb, size_t cap=(size_t)-1);

    Pool(Pool&& p) : _p(p._p) { p._p = 0; }

    Pool(const Pool&) = delete;
    void operator=(const Pool&) = delete;

    // pop an element from the pool
    // return NULL if the pool is empty and the create callback is not set
    // MUST be called in coroutine
    void* pop();

    // push an element to the pool
    // nothing is done if p is NULL
    // MUST be called in coroutine
    void push(void* p);

  private:
    void* _p;
};

// pop an element from co::Pool in constructor, and push it back in destructor.
// The element is stored internally as a pointer of type T*.
// As owner of the pointer, its behavior is similar to std::unique_ptr.
template<typename T>
class PoolGuard {
  public:
    explicit PoolGuard(Pool& pool) : _pool(pool) {
        _p = (T*) _pool.pop();
    }

    explicit PoolGuard(Pool* pool) : _pool(*pool) {
        _p = (T*) _pool.pop();
    }

    PoolGuard(const PoolGuard&) = delete;
    void operator=(const PoolGuard&) = delete;

    ~PoolGuard() {
        _pool.push(_p);
    }

    T* get() const {
        return _p;
    }

    void reset(T* p = 0) {
        if (_p != p) { delete _p; _p = p; }
    }

    void operator=(T* p) {
        this->reset(p);
    }

    T* operator->() const {
        return _p;
    }

    bool operator==(T* p) const {
        return _p == p;
    }

    bool operator!=(T* p) const {
        return _p != p;
    }

  private:
    Pool& _pool;
    T* _p;
};


} // namespace co

using co::go;
