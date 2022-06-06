#if KNICKKNACK_ENABLE_THREADS

#include "common.h"
#include "threading.h"
#include "std.h"

KNICKKNACK_INIT_LOCK(g_knickknack_internal_mutex);

void knickknack_internal_destroy_thread(void* data)
{
  #ifdef __WIN32__
    auto thread = (HANDLE)data;
    DWORD exit_code = 0;
    GetExitCodeThread(thread, &exit_code);
    auto joinable = (exit_code == STILL_ACTIVE);
  #else
    auto thread = (std::thread*)data;
    auto joinable = thread->joinable();
  #endif

  if (joinable)
  {
    KNICKKNACK_LOG_ERROR("Running thread destroyed: %p\n", data);
    g_knickknack_fatal_error_hook();
  }

  #ifdef __WIN32__
    CloseHandle(thread);
  #else
    delete &thread;
  #endif
}

#ifdef __WIN32__
  struct knickknack_internal_thread_worker_args
  {
    std::string name;
    std::string entry_point;
  };

  DWORD WINAPI knickknack_internal_thread_worker(void* data)
  {
    auto args = (knickknack_internal_thread_worker_args*)data;
    knickknack_call(args->name, args->entry_point);
    delete &args->name;
    delete &args->entry_point;
    free(data);
    return 0;
  }
#endif

knickknack_object* knickknack_ext_new_thread(knickknack_object* name, knickknack_object* entry_point)
{
  #ifdef __WIN32__
    auto n = knickknack_get_readonly_string_from_object(name);
    auto e = knickknack_get_readonly_string_from_object(entry_point);
    if (n && e)
    {
      auto args = (knickknack_internal_thread_worker_args*)knickknack_malloc_with_gc(sizeof(knickknack_internal_thread_worker_args));
      new (&args->name) std::string(n->c_str());
      new (&args->entry_point) std::string(e->c_str());
      auto thread = CreateThread(nullptr, 0, knickknack_internal_thread_worker, args, 0, nullptr);
      return knickknack_new_capsule(knickknack_internal_destroy_thread, (void*)thread);
    }
    return nullptr;
  #else
    std::thread t([n = name, e = entry_point](){
      knickknack_std_call(n, e);
    });
    return knickknack_new_capsule(knickknack_internal_destroy_thread, (void*)&t);
  #endif
}

void knickknack_ext_join_thread(knickknack_object* capsule)
{
  #ifdef __WIN32__
    if (auto thread = (HANDLE)knickknack_get_data_from_capsule(knickknack_internal_destroy_thread, capsule))
    {
      WaitForSingleObject(thread, INFINITE);
    }
  #else
    if (auto thread = (std::thread*)knickknack_get_data_from_capsule(knickknack_internal_destroy_thread, capsule))
    {
      thread->join();
    }
  #endif
}

unsigned int knickknack_ext_hardware_concurrency()
{
  #ifdef __WIN32__
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors;
  #else
    std::thread::hardware_concurrency();
  #endif
}

void* knickknack_ext_test_and_set(knickknack_object* object, void* key)
{
  void* old = nullptr;

  auto lock = KNICKKNACK_ACQUIRE_RAII_LOCK(g_knickknack_internal_mutex);
  if (knickknack_std_exists(object, key))
  {
    old = knickknack_std_get(object, key);
  }
  knickknack_std_set_primitive(object, key, (void*)1);
  return old;
}

#ifdef __WIN32__
  HANDLE knickknack_internal_ensure_notifier_event()
  {
    static HANDLE event = NULL;
    
    auto lock = KNICKKNACK_ACQUIRE_RAII_LOCK(g_knickknack_internal_mutex);
    if (!event)
    {
      event = CreateEvent(NULL, FALSE, FALSE, NULL);
    }
    return event;
  }
#else
  std::mutex g_knickknack_internal_notifier_mutex;
  std::condition_variable g_knickknack_internal_notifier_condition;
#endif

void knickknack_ext_wait(unsigned int duration_ms)
{
  #ifdef __WIN32__
    auto event = knickknack_internal_ensure_notifier_event();
    WaitForSingleObject(event, duration_ms);
  #else
    std::unique_lock<std::mutex> lock(g_knickknack_internal_notifier_mutex);
    g_knickknack_internal_notifier_condition.wait_for(lock, std::chrono::milliseconds(duration_ms));
  #endif
}

void knickknack_ext_notify()
{
  #ifdef __WIN32__
    auto event = knickknack_internal_ensure_notifier_event();
    SetEvent(event);
  #else
    std::unique_lock<std::mutex> lock(mtx);
    g_knickknack_internal_notifier_condition.notify_all();
  #endif
}

#endif
