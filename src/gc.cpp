#include "gc.h"

#ifdef KNICKKNACK_ENABLE_THREADS
  #include "threading.h"
#endif

std::unordered_map<knickknack_object*, bool> g_knickknack_gc_heap {};
void (*g_knickknack_gc_hook_cleanup)() = nullptr;
int knickknack_remaining_bytes_until_gc = KNICKKNACK_BYTES_PER_GC_CYCLE;

void knickknack_gc_mark_in_use(knickknack_object* object)
{
  if (g_knickknack_gc_heap.find(object) != g_knickknack_gc_heap.end())
  {
    if (!g_knickknack_gc_heap[object])
    {
      g_knickknack_gc_heap[object] = true;
      switch(object->kind)
      {
        case knickknack_kind::LIST:
        {
          auto inst = (std::vector<knickknack_object*>*)object->instance;
          for (auto i : *inst)
          {
            knickknack_gc_mark_in_use(i);
          }
        }
          break;
        case knickknack_kind::DICT:
        {
          auto inst = (std::unordered_map<std::string, knickknack_object*>*)object->instance;
          for(auto kv : *inst)
          {
            knickknack_gc_mark_in_use(kv.second);
          }
        }
          break;
        default:
          break;
      }
    }
  }
}

void knickknack_internal_gc_mark_binary_in_use(knickknack_binary* binary)
{
  if (!binary->in_use)
  {
    binary->in_use = true;
    for (auto name : binary->linked_binaries)
    {
      auto it = g_knickknack_script_binaries.find(name);
      if (it != g_knickknack_script_binaries.end())
      {
        knickknack_internal_gc_mark_binary_in_use(it->second);
      }
    }
  }
}

void knickknack_std_gc(bool remove_scripts)
{
  #ifdef KNICKKNACK_ENABLE_THREADS
    auto lock = KNICKKNACK_ACQUIRE_RAII_LOCK(g_knickknack_internal_mutex);
  #endif

  knickknack_remaining_bytes_until_gc = KNICKKNACK_BYTES_PER_GC_CYCLE;
  
  // Mark all objects in the heap as unused
  for (auto kv : g_knickknack_gc_heap)
  {
    g_knickknack_gc_heap[kv.first] = false;
  }

  // Mark all loaded scripts as unused
  for (auto kv : g_knickknack_script_binaries)
  {
    kv.second->in_use = false;
  }

  // Iterate through and mark all global objects as in use
  if(g_knickknack_globals)
  {
    knickknack_gc_mark_in_use(g_knickknack_globals);
  }

  // Call the cleanup hook if it exists
  // The hook should cleanup data structures outside of KnickKnack and
  // should call knickknack_gc_mark_in_use for any objects still being used
  // by the host but not by any KnickKnack code
  if (g_knickknack_gc_hook_cleanup)
  {
    g_knickknack_gc_hook_cleanup();
  }

  // Determine which scripts are in use
  // Scripts are considered in use if they have a refcount > 0
  // or if they're a dependency for a script with a refcount > 0
  for (auto kv : g_knickknack_script_binaries)
  {
    if (kv.second->refcount > 0)
    {
      knickknack_internal_gc_mark_binary_in_use(kv.second);
    }
  }

  // Iterate through loaded scripts
  {
    auto it = g_knickknack_script_binaries.begin();
    while (it != g_knickknack_script_binaries.end())
    {
      // Unload scripts that aren't in use if remove_scripts is true
      if (!it->second->in_use)
      {
        if (remove_scripts)
        {
          knickknack_internal_destroy_binary(it->second);
          it = g_knickknack_script_binaries.erase(it);
        }
      }
      else
      {
        // Iterate through the values stored in the variable_section
        // Mark objects as in use if they're in the heap
        knickknack_object** current = (knickknack_object**)it->second->variable_section;
        knickknack_object** end = (knickknack_object**)it->second->end;
        for(; current != end; ++current)
        {
          knickknack_gc_mark_in_use(*current);
        }
        
        ++it;
      }
    }
  }

  // Destroy all orphaned objects
  {
    auto it = g_knickknack_gc_heap.begin();
    while (it != g_knickknack_gc_heap.end())
    {
      // Check if the object is in use
      if (!it->second)
      {
        knickknack_internal_destroy_shallow(it->first);
        it = g_knickknack_gc_heap.erase(it);
      }
      else
      {
        ++it;
      }
    }
  }
}

#ifdef KNICKKNACK_HAS_VIRTUAL_MEMORY
  // Check if page fault related IO has occurred since the last check
  bool knickknack_internal_should_run_gc()
  {
    #if defined(__WIN32__)
      static DWORD last_count = 0;
      PROCESS_MEMORY_COUNTERS pmc;
      if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
      {
        return true;
      }
      
      #ifdef KNICKKNACK_ENABLE_THREADS
        auto lock = KNICKKNACK_ACQUIRE_RAII_LOCK(g_knickknack_internal_mutex);
      #endif
      
      if (last_count != pmc.PageFaultCount)
      {
        last_count = pmc.PageFaultCount;
        return true;
      }
      return false;
    #else
      static long last_count = 0;
      struct rusage usage;
      if (getrusage(RUSAGE_SELF, &usage))
      {
        return true;
      }

      #ifdef KNICKKNACK_ENABLE_THREADS
        auto lock = KNICKKNACK_ACQUIRE_RAII_LOCK(g_knickknack_internal_mutex);
      #endif

      if (last_count != usage.ru_majflt)
      {
        last_count = usage.ru_majflt;
        return true;
      }
      return false;
    #endif
  }

  // Periodically checks page faults and runs GC
  // when memory is low to reduce disk IO by discouraging
  // compiled scripts from being written to the page file
  // On platforms without paging, we can just wait until the heap is full
  void knickknack_run_gc_if_due(size_t delta)
  {
    if (((int)delta >= knickknack_remaining_bytes_until_gc) && knickknack_internal_should_run_gc())
    {
      knickknack_std_gc(false);
      if (knickknack_internal_should_run_gc())
      {
        knickknack_std_gc(true);
      }
    }
    else
    {
      #ifdef KNICKKNACK_ENABLE_THREADS
        auto lock = KNICKKNACK_ACQUIRE_RAII_LOCK(g_knickknack_internal_mutex);
      #endif

      knickknack_remaining_bytes_until_gc -= delta;
    }
  }
#endif

void* knickknack_malloc_with_gc(size_t size)
{
  #ifdef KNICKKNACK_HAS_VIRTUAL_MEMORY
    knickknack_run_gc_if_due(size);
  #endif

  auto ptr = malloc(size);
  if (ptr)
  {
    return ptr;
  }

  knickknack_std_gc(false);

  ptr = malloc(size);
  if (ptr)
  {
    return ptr;
  }

  knickknack_std_gc(true);

  ptr = malloc(size);
  if (ptr)
  {
    return ptr;
  }

  KNICKKNACK_LOG_ERROR("Out of Memory: Couldn't malloc %d\n", size);
  g_knickknack_fatal_error_hook();
  return nullptr;
}

void* knickknack_realloc_with_gc(void* ptr, size_t new_size)
{
  #ifdef KNICKKNACK_HAS_VIRTUAL_MEMORY
    knickknack_run_gc_if_due(new_size);
  #endif

  auto new_ptr = realloc(ptr, new_size);
  if (new_ptr)
  {
    return new_ptr;
  }

  knickknack_std_gc(false);

  new_ptr = realloc(ptr, new_size);
  if (new_ptr)
  {
    return new_ptr;
  }

  knickknack_std_gc(true);

  new_ptr = realloc(ptr, new_size);
  if (new_ptr)
  {
    return new_ptr;
  }

  KNICKKNACK_LOG_ERROR("Out of Memory: Couldn't realloc %d\n", new_size);
  g_knickknack_fatal_error_hook();
  return nullptr;
}
