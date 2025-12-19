#include "api.h"
#include <unistd.h>

#ifdef KNICKKNACK_ENABLE_THREADS
  #include "threading.h"
  #include <sstream>
#endif

#ifdef KNICKKNACK_ENABLE_SOCKETS
  #include "socket.h"
#endif

void (*g_knickknack_fatal_error_hook)() = knickknack_internal_fatal_error;
std::unordered_map<std::string, knickknack_binary*> g_knickknack_script_binaries {};

void knickknack_init()
{
  #if defined(KNICKKNACK_ENABLE_SOCKETS) && defined(__WIN32__)
    knickknack_init_sockets();
  #endif
}

void knickknack_internal_fatal_error()
{ 
#ifdef __WIN32__
  exit(-1);
#else
  while(1)
  {
    swiWaitForVBlank();
  }
#endif
}

// If threads are enabled, ensure each thread gets its own binary
std::string knickknack_thread_local_name(std::string name)
{
  #ifdef KNICKKNACK_ENABLE_THREADS
    std::stringstream ss;
    #ifdef __WIN32__
      ss << GetCurrentThreadId();
    #else
      ss << std::this_thread::get_id();
    #endif
    return name + "!" + ss.str();
  #else
    return name;
  #endif
}

knickknack_binary* knickknack_internal_acquire_script(std::string name)
{
  knickknack_binary* binary = nullptr;

  #ifdef KNICKKNACK_ENABLE_THREADS
    auto original_name = name;
  #endif

  {
    #ifdef KNICKKNACK_ENABLE_THREADS
      auto lock = KNICKKNACK_ACQUIRE_RAII_LOCK(g_knickknack_internal_mutex);
      name = knickknack_thread_local_name(name);
    #endif

    if (g_knickknack_script_binaries.find(name) != g_knickknack_script_binaries.end())
    {
      binary = g_knickknack_script_binaries[name];
    }
  }

  if (!binary)
  {
    #ifdef KNICKKNACK_ENABLE_THREADS
      binary = knickknack_internal_try_load(original_name);
    #else
      binary = knickknack_internal_try_load(name);
    #endif

    if (binary)
    {
      {
        #ifdef KNICKKNACK_ENABLE_THREADS
          auto lock = KNICKKNACK_ACQUIRE_RAII_LOCK(g_knickknack_internal_mutex);
        #endif

        ++binary->refcount;
        g_knickknack_script_binaries[name] = binary;
      }

      if (binary->exports.find("std_init") != binary->exports.end())
      {
        auto function = (void* (*)())binary->exports["std_init"];

        #if defined(__arm__)
          // Set the LSB so function will execute in THUMB mode
          function = (void* (*)())((unsigned int)function | 1);
        #endif

        function();
      }
    }
  }
  else
  {
    #ifdef KNICKKNACK_ENABLE_THREADS
      auto lock = KNICKKNACK_ACQUIRE_RAII_LOCK(g_knickknack_internal_mutex);
    #endif
    ++binary->refcount;
  }

  if (!binary)
  {
    KNICKKNACK_LOG_ERROR("%s: acquire failed\n", name.c_str());
    g_knickknack_fatal_error_hook();
  }

  return binary;
}

void knickknack_internal_release_script(knickknack_binary* binary)
{
  #ifdef KNICKKNACK_ENABLE_THREADS
    auto lock = KNICKKNACK_ACQUIRE_RAII_LOCK(g_knickknack_internal_mutex);
  #endif
  
  if (binary)
  {
    --binary->refcount;
  }
}

bool knickknack_script_exists(std::string name)
{
  #ifdef KNICKKNACK_ENABLE_THREADS
    auto lock = KNICKKNACK_ACQUIRE_RAII_LOCK(g_knickknack_internal_mutex);
    
    auto original_name = name;
    name = knickknack_thread_local_name(name);
  #endif

  if (g_knickknack_script_binaries.find(name) != g_knickknack_script_binaries.end())
  {
    return true;
  }
  #ifdef KNICKKNACK_ENABLE_THREADS
    else if (g_knickknack_loader_hook_exists && g_knickknack_loader_hook_exists(original_name))
    {
      return true;
    }
    else
    {
      for (auto path : g_knickknack_search_paths)
      {
        
          if (access((path + "/" + original_name).c_str(), R_OK) == 0)
          {
            return true;
          }
      }
    }
  #else
    else if (g_knickknack_loader_hook_exists && g_knickknack_loader_hook_exists(name))
    {
      return true;
    }
    else
    {
      for (auto path : g_knickknack_search_paths)
      {
        
          if (access((path + "/" + name).c_str(), R_OK) == 0)
          {
            return true;
          }
      }
    }
  #endif

  return false;
}

bool knickknack_entry_point_exists(std::string name, std::string entry_point)
{
  if (knickknack_script_exists(name))
  {
    auto script = knickknack_internal_acquire_script(name);
    auto res = (script->exports.find(entry_point) != script->exports.end());
    knickknack_internal_release_script(script);
    return res;
  }
  return false;
}

void* knickknack_call(std::string name, std::string entry_point)
{
  auto script = knickknack_internal_acquire_script(name);
  auto function = (void* (*)())script->exports[entry_point];

  #if defined(__arm__)
    // Set the LSB so function will execute in THUMB mode
    function = (void* (*)())((unsigned int)function | 1);
  #endif

  auto res = function();
  knickknack_internal_release_script(script);
  return res;
}

void* knickknack_call1(std::string name, std::string entry_point, void* arg1)
{
  auto script = knickknack_internal_acquire_script(name);
  auto function = (void* (*)(void*))script->exports[entry_point];

  #if defined(__arm__)
    // Set the LSB so function will execute in THUMB mode
    function = (void* (*)(void*))((unsigned int)function | 1);
  #endif

  auto res = function(arg1);
  knickknack_internal_release_script(script);
  return res;
}

void* knickknack_call2(std::string name, std::string entry_point, void* arg1, void* arg2)
{
  auto script = knickknack_internal_acquire_script(name);
  auto function = (void* (*)(void*, void*))script->exports[entry_point];

  #if defined(__arm__)
    // Set the LSB so function will execute in THUMB mode
    function = (void* (*)(void*, void*))((unsigned int)function | 1);
  #endif

  auto res = function(arg1, arg2);
  knickknack_internal_release_script(script);
  return res;
}

void* knickknack_call3(std::string name, std::string entry_point, void* arg1, void* arg2, void* arg3)
{
  auto script = knickknack_internal_acquire_script(name);
  auto function = (void* (*)(void*, void*, void*))script->exports[entry_point];

  #if defined(__arm__)
    // Set the LSB so function will execute in THUMB mode
    function = (void* (*)(void*, void*, void*))((unsigned int)function | 1);
  #endif

  auto res = function(arg1, arg2, arg3);
  knickknack_internal_release_script(script);
  return res;
}

knickknack_object* knickknack_new_capsule(void (*destructor)(void*), void* data)
{
  auto obj = (knickknack_object*)knickknack_malloc_with_gc(sizeof(knickknack_object));
  
  if (obj)
  {
    obj->kind = knickknack_kind::CAPSULE;
    obj->instance = knickknack_malloc_with_gc(sizeof(knickknack_capsule));
    if (obj->instance)
    {
      ((knickknack_capsule*)obj->instance)->destroy = destructor;
      ((knickknack_capsule*)obj->instance)->data = data;

      #ifdef KNICKKNACK_ENABLE_THREADS
        auto lock = KNICKKNACK_ACQUIRE_RAII_LOCK(g_knickknack_internal_mutex);
      #endif

      g_knickknack_gc_heap[obj] = true;
    }
    else
    {
      free(obj);
      obj = nullptr;
    }
  }

  return obj;
}

void* knickknack_get_data_from_capsule(void (*destructor)(void*), knickknack_object* object)
{
  if (object)
  {
    if (object->kind == knickknack_kind::CAPSULE && object->instance)
    {
      auto capsule = (knickknack_capsule*)object->instance;
      if (capsule->destroy == destructor)
      {
        return capsule->data;
      }
    }
  }
  
  return nullptr;
}

std::string* knickknack_get_string_from_object(knickknack_object* object)
{
  if (object)
  {
    if (object->kind == knickknack_kind::STRING)
    {
      return (std::string*)object->instance;
    }
  }

  return nullptr;
}

const std::string* knickknack_get_readonly_string_from_object(knickknack_object* object)
{
  if (object)
  {
    if (object->kind == knickknack_kind::STRING || object->kind == knickknack_kind::STRING_LITERAL)
    {
      return (std::string*)object->instance;
    }
  }

  return nullptr;
}
