#pragma once

#ifdef __WIN32__
  // Compatibility wrapper to enable building under MinGW32
  // This should also give a slight performance boost since we're calling
  // the functions directly rather than through the posix wrapper

  class knickknack_srwlock_raii_wrapper
  {
  public:
      knickknack_srwlock_raii_wrapper(const SRWLOCK& lock) : m_lock(lock)
      {
          AcquireSRWLockExclusive(const_cast<SRWLOCK*>(&m_lock));
      }

      ~knickknack_srwlock_raii_wrapper()
      {
          ReleaseSRWLockExclusive(const_cast<SRWLOCK*>(&m_lock));
      }
  private:
      const SRWLOCK& m_lock;
  };

  #define KNICKKNACK_LOCK_TYPE SRWLOCK
  #define KNICKKNACK_INIT_LOCK(x) SRWLOCK x = {0}
  #define KNICKKNACK_ACQUIRE_RAII_LOCK(x) knickknack_srwlock_raii_wrapper(&x)
#else
  #include <thread>
  #include <mutex>
  #include <condition_variable>
  #include <chrono>

  #define KNICKKNACK_LOCK_TYPE std::mutex
  #define KNICKKNACK_INIT_LOCK(x) std::mutex x
  #define KNICKKNACK_ACQUIRE_RAII_LOCK(x) std::lock_guard<std::mutex>(x)
#endif

extern KNICKKNACK_LOCK_TYPE g_knickknack_internal_mutex;

knickknack_object* knickknack_ext_new_thread(knickknack_object* name, knickknack_object* entry_point);
void knickknack_ext_join_thread(knickknack_object* capsule);
unsigned int knickknack_ext_hardware_concurrency();
void* knickknack_ext_test_and_set(knickknack_object* object, void* key);
void knickknack_ext_wait(unsigned int duration_ms);
void knickknack_ext_notify();
