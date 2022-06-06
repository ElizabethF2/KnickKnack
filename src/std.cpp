#include "std.h"
#include <chrono>

#if KNICKKNACK_ENABLE_SOCKETS
  #include "socket.h"
#endif

#if KNICKKNACK_ENABLE_THREADS
  #include "threading.h"
#endif

#if defined(__WIN32__) || defined(__linux__)
  #include "library.h"
#endif

bool knickknack_std_builtin_exists(knickknack_object* name)
{
  return g_knickknack_builtins.find(*((std::string*)name->instance)) != g_knickknack_builtins.end();
}

knickknack_object* knickknack_std_platform()
{
  knickknack_object* platform = knickknack_std_new_string();

  if (platform)
  {
    ((std::string*)platform->instance)->append(KNICKKNACK_PLATFORM);
  }

  return platform;
}

void knickknack_std_fatal_error(knickknack_object* message)
{
  switch(knickknack_std_kind(message))
  {
    case knickknack_kind::STRING:
    case knickknack_kind::STRING_LITERAL:
      KNICKKNACK_LOG_ERROR("Fatal Error from Script: %s\n", ((std::string*)message->instance)->c_str());
      break;
    case knickknack_kind::UNBOXED_PRIMITIVE:
      KNICKKNACK_LOG_ERROR("Fatal Error from Script: %d\n", (int)message);
      break;
    default:
      KNICKKNACK_LOG_ERROR("Fatal Error from Script, message unprintable: %p\n", message);
      break;
  }

  g_knickknack_fatal_error_hook();
}

bool knickknack_std_script_exists(knickknack_object* name)
{
  return knickknack_script_exists(*(std::string*)name->instance);
}

bool knickknack_std_entry_point_exists(knickknack_object* name, knickknack_object* entry_point)
{
  return knickknack_entry_point_exists(*(std::string*)name->instance, *(std::string*)entry_point->instance);
}

void* knickknack_std_call(knickknack_object* name, knickknack_object* entry_point)
{
  return knickknack_call(*((std::string*)name->instance), *((std::string*)entry_point->instance));
}

void* knickknack_std_call1(knickknack_object* name, knickknack_object* entry_point, void* arg1)
{
  return knickknack_call1(*((std::string*)name->instance), *((std::string*)entry_point->instance), arg1);
}

void* knickknack_std_call2(knickknack_object* name, knickknack_object* entry_point, void* arg1, void* arg2)
{
  return knickknack_call2(*((std::string*)name->instance), *((std::string*)entry_point->instance), arg1, arg2);
}

void* knickknack_std_call3(knickknack_object* name, knickknack_object* entry_point, void* arg1, void* arg2, void* arg3)
{
  return knickknack_call3(*((std::string*)name->instance), *((std::string*)entry_point->instance), arg1, arg2, arg3);
}

int knickknack_std_getchar()
{
  return getchar();
}

void knickknack_std_print(knickknack_object* object)
{
  switch(object->kind)
  {
    case knickknack_kind::STRING:
    case knickknack_kind::STRING_LITERAL:
      printf("%s", ((std::string*)object->instance)->c_str());
      break;
    default:
      break;
  }
}

int knickknack_std_now()
{
  return time(nullptr);
}

knickknack_object* knickknack_internal_new_boxed_primitive(void* value)
{
  auto obj = (knickknack_object*)knickknack_malloc_with_gc(sizeof(knickknack_object));

  if (obj)
  {
    obj->kind = knickknack_kind::PRIMITIVE;
    obj->instance = value;

    #ifdef KNICKKNACK_ENABLE_THREADS
      auto lock = KNICKKNACK_ACQUIRE_RAII_LOCK(g_knickknack_internal_mutex);
    #endif

    g_knickknack_gc_heap[obj] = true;
  }

  return obj;
}

knickknack_object* knickknack_std_new_string()
{
  auto obj = (knickknack_object*)knickknack_malloc_with_gc(sizeof(knickknack_object));

  if (obj)
  {
    obj->kind = knickknack_kind::STRING;
    obj->instance = knickknack_malloc_with_gc(sizeof(std::string));
    if (obj->instance)
    {
      new (obj->instance) std::string();

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

knickknack_object* knickknack_std_new_list()
{
  auto obj = (knickknack_object*)knickknack_malloc_with_gc(sizeof(knickknack_object));

  if (obj)
  {
    obj->kind = knickknack_kind::LIST;
    obj->instance = knickknack_malloc_with_gc(sizeof(std::vector<knickknack_object*>));
    if (obj->instance)
    {
      new (obj->instance) std::vector<knickknack_object*>();

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

knickknack_object* knickknack_std_new_dict()
{
  auto obj = (knickknack_object*)knickknack_malloc_with_gc(sizeof(knickknack_object));
  
  if (obj)
  {
    obj->kind = knickknack_kind::DICT;
    obj->instance = knickknack_malloc_with_gc(sizeof(std::unordered_map<std::string, knickknack_object*>));
    if (obj->instance)
    {
      new (obj->instance) std::unordered_map<std::string, knickknack_object*>();

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

knickknack_object* knickknack_std_copy(knickknack_object* object)
{
  switch(object->kind)
  {
    case knickknack_kind::STRING:
    case knickknack_kind::STRING_LITERAL:
    {
      auto str = knickknack_std_new_string();
      if (str)
      {
        auto old_inst = (std::string*)object->instance;
        auto new_inst = (std::string*)str->instance;
        new_inst->append(*old_inst);
      }
      return str;
    }
      break;
    case knickknack_kind::LIST:
    {
      auto list = knickknack_std_new_list();
      if (list)
      {
        auto old_inst = (std::vector<knickknack_object*>*)object->instance;
        auto new_inst = (std::vector<knickknack_object*>*)list->instance;
        for (auto old_item : *old_inst)
        {
          auto new_item = knickknack_std_copy(old_item);
          if (!new_item)
          {
            knickknack_std_destroy(list);
            return nullptr;
          }
          new_inst->push_back(new_item);
        }
      }
      return list;
    }
      break;
    case knickknack_kind::DICT:
    {
      auto dict = knickknack_std_new_dict();
      if (dict)
      {
        auto old_inst = (std::unordered_map<std::string, knickknack_object*>*)object->instance;
        auto new_inst = (std::unordered_map<std::string, knickknack_object*>*)dict->instance;
        for (auto kv : *old_inst)
        {
          auto new_item = knickknack_std_copy(kv.second);
          if (!new_item)
          {
            knickknack_std_destroy(dict);
            return nullptr;
          }
          (*new_inst)[kv.first] = new_item;
        }
      }
      return dict;
    }
      break;
    case knickknack_kind::PRIMITIVE:
      return knickknack_internal_new_boxed_primitive(object->instance);
      break;
    default:
      break;
  }

  return nullptr;
}

void knickknack_std_append(knickknack_object* object, knickknack_object* value)
{
  switch(object->kind)
  {
    case knickknack_kind::STRING:
    {
      auto inst = (std::string*)object->instance;
      if (value->kind == knickknack_kind::STRING || value->kind == knickknack_kind::STRING_LITERAL)
      {
        auto inst2 = (std::string*)value->instance;
        inst->append(*inst2);
      }
    }
      break;
    case knickknack_kind::LIST:
    {
      auto inst = (std::vector<knickknack_object*>*)object->instance;
      if (value)
      {
        inst->push_back(value);
      }
    }
      break;
    default:
      break;
  }
}

void knickknack_std_append_primitive(knickknack_object* object, void* value)
{
  switch(object->kind)
  {
    case knickknack_kind::STRING:
    {
      auto inst = (std::string*)object->instance;
      char c = ((char*)(&value))[0];
      inst->push_back(c);
    }
      break;
    case knickknack_kind::LIST:
      knickknack_std_append(object, knickknack_internal_new_boxed_primitive(value));
      break;
    default:
      break;
  }
}

void knickknack_std_set(knickknack_object* object, void* key_or_index, knickknack_object* value)
{
  switch(object->kind)
  {
    case knickknack_kind::LIST:
    {
      auto inst = (std::vector<knickknack_object*>*)object->instance;
      (*inst)[(size_t)key_or_index] = value;
    }
      break;
    case knickknack_kind::DICT:
    {
      auto inst = (std::unordered_map<std::string, knickknack_object*>*)object->instance;
      auto key = (std::string*)((knickknack_object*)key_or_index)->instance;
      (*inst)[*key] = value;
    }
      break;
    default:
      break;
  }
}

void knickknack_std_set_primitive(knickknack_object* object, void* key_or_index, void* value)
{
  if (object->kind == knickknack_kind::LIST || object->kind == knickknack_kind::DICT)
  {
    knickknack_std_set(object, key_or_index, knickknack_internal_new_boxed_primitive(value));
  }
}

bool knickknack_std_exists(knickknack_object* object, void* key_or_index)
{
  switch(object->kind)
  {
    case knickknack_kind::STRING:
    case knickknack_kind::STRING_LITERAL:
    case knickknack_kind::LIST:
    {
      auto size = knickknack_std_size(object);
      return size > (size_t)key_or_index;
    }
      break;
    case knickknack_kind::DICT:
    {
      auto inst = (std::unordered_map<std::string, knickknack_object*>*)object->instance;
      auto key = (std::string*)((knickknack_object*)key_or_index)->instance;
      return (inst->find(*key) != inst->end());
    }
      break;
    default:
      break;
  }
  
  return false;
}

void knickknack_std_remove(knickknack_object* object, void* key_or_index)
{
  #ifdef KNICKKNACK_ENABLE_THREADS
    auto lock = KNICKKNACK_ACQUIRE_RAII_LOCK(g_knickknack_internal_mutex);
  #endif

  switch(object->kind)
  {
    case knickknack_kind::STRING:
    {
      auto inst = (std::string*)object->instance;
      inst->erase((int)key_or_index, 1);
    }
      break;
    case knickknack_kind::LIST:
    {
      auto inst = (std::vector<knickknack_object*>*)object->instance;
      auto itr = inst->begin();
      std::advance(itr, (int)key_or_index);
      inst->erase(itr);
    }
      break;
    case knickknack_kind::DICT:
    {
      auto inst = (std::unordered_map<std::string, knickknack_object*>*)object->instance;
      auto key = (std::string*)((knickknack_object*)key_or_index)->instance;
      inst->erase(*key);
    }
      break;
    default:
      break;
  }
}

void* knickknack_internal_unbox_primitives(knickknack_object* object)
{
  if (object->kind == knickknack_kind::PRIMITIVE)
  {
    return object->instance;
  }
  return (void*) object;
}

void* knickknack_std_get(knickknack_object* object, void* key_or_index)
{
  void* value = nullptr;
  
  switch(object->kind)
  {
    case knickknack_kind::STRING:
    case knickknack_kind::STRING_LITERAL:
    {
      auto inst = (std::string*)object->instance;
      int c = 0;
      c += (int)(*inst)[(int)key_or_index];
      return (void*)c;
    }
      break;
    case knickknack_kind::LIST:
    {
      auto inst = (std::vector<knickknack_object*>*)object->instance;
      value = knickknack_internal_unbox_primitives((*inst)[(size_t)key_or_index]);
    }
      break;
    case knickknack_kind::DICT:
    {
      auto inst = (std::unordered_map<std::string, knickknack_object*>*)object->instance;
      auto key = (std::string*)((knickknack_object*)key_or_index)->instance;
      value = knickknack_internal_unbox_primitives((*inst)[*key]);
    }
      break;
    default:
      break;
  }
  
  return value;
}

unsigned int knickknack_std_size(knickknack_object* object)
{
  switch(object->kind)
  {
    case knickknack_kind::STRING:
    case knickknack_kind::STRING_LITERAL:
    {
      auto inst = (std::string*)object->instance;
      return inst->size();
    }
      break;
    case knickknack_kind::LIST:
    {
      auto inst = (std::vector<knickknack_object*>*)object->instance;
      return inst->size();
    }
      break;
    case knickknack_kind::DICT:
    {
      auto inst = (std::unordered_map<std::string, knickknack_object*>*)object->instance;
      return inst->size();
    }
      break;
    default:
      break;
  }
  
  return 0;
}

void knickknack_std_for_each(knickknack_object* object, knickknack_object* name, knickknack_object* entry_point)
{
  switch(object->kind)
  {
    case knickknack_kind::LIST:
    {
      auto inst = (std::vector<knickknack_object*>*)object->instance;
      for (auto item : *inst)
      {
        knickknack_std_call1(name, entry_point, knickknack_internal_unbox_primitives(item));
      }
    }
      break;
    case knickknack_kind::DICT:
    {
      auto inst = (std::unordered_map<std::string, knickknack_object*>*)object->instance;
      for (auto kv : *inst)
      {
        auto key = knickknack_std_new_string();
        if (!key)
        {
          return;
        }
        ((std::string*)key->instance)->append(kv.first);
        knickknack_std_call2(name, entry_point, key, knickknack_internal_unbox_primitives(kv.second));
      }
    }
      break;
    default:
      break;
  }
}

bool knickknack_std_equal(knickknack_object* a, knickknack_object* b)
{
  if (a == b)
  {
    return true;
  }
  
  auto kind_a = a->kind;
  auto kind_b = b->kind;
  
  if (kind_a == knickknack_kind::STRING_LITERAL)
  {
    kind_a = knickknack_kind::STRING;
  }
  
  if (kind_b == knickknack_kind::STRING_LITERAL)
  {
    kind_b = knickknack_kind::STRING;
  }
  
  if (kind_a != kind_b)
  {
    return false;
  }
  
  if (kind_a == knickknack_kind::STRING)
  {
    auto inst_a = (std::string*)a->instance;
    auto inst_b = (std::string*)b->instance;
    return ((*inst_a) == (*inst_b));
  }
  
  auto size_a = knickknack_std_size(a);
  auto size_b = knickknack_std_size(b);

  if (size_a != size_b)
  {
    return false;
  }
  
  if (kind_a == knickknack_kind::LIST)
  {
    auto inst_a = (std::vector<knickknack_object*>*)a->instance;
    auto inst_b = (std::vector<knickknack_object*>*)a->instance;
    for (unsigned int i = 0; i < size_a; ++i)
    {
      if (!knickknack_std_equal((*inst_a)[i], (*inst_b)[i]))
      {
        return false;
      }
    }
    return true;
  }
  else if (kind_a == knickknack_kind::DICT)
  {
    auto inst_a = (std::unordered_map<std::string, knickknack_object*>*)a->instance;
    auto inst_b = (std::unordered_map<std::string, knickknack_object*>*)b->instance;
    for (auto kv : (*inst_a))
    {
      if (inst_b->find(kv.first) == inst_b->end())
      {
        return false;
      }

      if (!knickknack_std_equal(kv.second, (*inst_b)[kv.first]))
      {
        return false;
      }
    }
    return true;
  }
  
  return false;
}

void knickknack_std_destroy(knickknack_object* object)
{
  if (object == g_knickknack_globals)
  {
    return;
  }

  switch(object->kind)
  {
    case knickknack_kind::LIST:
    {
      auto inst = (std::vector<knickknack_object*>*)object->instance;
      for (auto i : *inst)
      {
        knickknack_std_destroy(i);
      }
    }
      break;
    case knickknack_kind::DICT:
    {
      auto inst = (std::unordered_map<std::string, knickknack_object*>*)object->instance;
      for(auto kv : *inst)
      {
        knickknack_std_destroy(kv.second);
      }
    }
      break;
    default:
      break;
  }
  knickknack_internal_destroy_shallow(object);

  #ifdef KNICKKNACK_ENABLE_THREADS
    auto lock = KNICKKNACK_ACQUIRE_RAII_LOCK(g_knickknack_internal_mutex);
  #endif

  g_knickknack_gc_heap.erase(object);
}

void knickknack_internal_destroy_shallow(knickknack_object* object)
{
  switch(object->kind)
  {
    case knickknack_kind::STRING:
    {
      auto inst = (std::string*)object->instance;
      delete &inst;
    }
      break;
    case knickknack_kind::LIST:
    {
      auto inst = (std::vector<knickknack_object*>*)object->instance;
      delete &inst;
    }
      break;
    case knickknack_kind::DICT:
    {
      auto inst = (std::unordered_map<std::string, knickknack_object*>*)object->instance;
      delete &inst;
    }
      break;
    case knickknack_kind::CAPSULE:
    {
      auto inst = (knickknack_capsule*)object->instance;
      inst->destroy(inst->data);
    }
      break;
    default:
      break;
  }
  
  if (object->kind != knickknack_kind::PRIMITIVE)
  {
    free(object->instance);
  }
  free(object);
}

knickknack_kind knickknack_std_kind(knickknack_object* object)
{
  knickknack_kind result = knickknack_kind::UNBOXED_PRIMITIVE;
  
  #ifdef KNICKKNACK_ENABLE_THREADS
    auto lock = KNICKKNACK_ACQUIRE_RAII_LOCK(g_knickknack_internal_mutex);
  #endif

  if (object == nullptr)
  {
    return knickknack_kind::UNBOXED_PRIMITIVE;
  }
  else if (g_knickknack_gc_heap.find(object) != g_knickknack_gc_heap.end())
  {
    result = object->kind;
  }
  else
  {
    for (auto kv : g_knickknack_script_binaries)
    {
      if (kv.second->literals.count(object))
      {
        return knickknack_kind::STRING_LITERAL;
      }
    }
  }

  return result;
}

std::unordered_map<std::string, void*> g_knickknack_builtins =
{
  {"errno", (void*)&errno},
  {"std_error_not_found", (void*)ENOENT},
  {"std_error_invalid_arg", (void*)EINVAL},

  {"std_builtin_exists", (void*)knickknack_std_builtin_exists},
  {"std_script_exists", (void*)knickknack_std_script_exists},
  {"std_entry_point_exists", (void*)knickknack_std_entry_point_exists},
  {"std_platform", (void*)knickknack_std_platform},
  {"std_fatal_error", (void*)knickknack_std_fatal_error},

  {"std_call", (void*)knickknack_std_call},
  {"std_call1", (void*)knickknack_std_call1},
  {"std_call2", (void*)knickknack_std_call2},
  {"std_call3", (void*)knickknack_std_call3},

  {"std_getchar", (void*)knickknack_std_getchar},
  {"std_print", (void*)knickknack_std_print},

  {"std_now", (void*)knickknack_std_now},

  {"std_new_string", (void*)knickknack_std_new_string},
  {"std_new_list", (void*)knickknack_std_new_list},
  {"std_new_dict", (void*)knickknack_std_new_dict},
  {"std_copy", (void*)knickknack_std_copy},

  {"std_set", (void*)knickknack_std_set},
  {"std_set_primitive", (void*)knickknack_std_set_primitive},
  {"std_append", (void*)knickknack_std_append},
  {"std_append_primitive", (void*)knickknack_std_append_primitive},
  {"std_remove", (void*)knickknack_std_remove},

  {"std_equal", (void*)knickknack_std_equal},
  {"std_size", (void*)knickknack_std_size},
  {"std_exists", (void*)knickknack_std_exists},
  {"std_get", (void*)knickknack_std_get},
  {"std_kind", (void*)knickknack_std_kind},
  {"std_for_each", (void*)knickknack_std_for_each},

  {"std_gc", (void*)knickknack_std_gc},
  {"std_destroy", (void*)knickknack_std_destroy},

  #if KNICKKNACK_ENABLE_SOCKETS
  {"ext_new_socket", (void*)knickknack_ext_new_socket},
  {"ext_gethostbyname", (void*)knickknack_ext_gethostbyname},
  {"ext_connect_socket", (void*)knickknack_ext_connect_socket},
  {"ext_send_socket", (void*)knickknack_ext_send_socket},
  {"ext_ioctl", (void*)knickknack_ext_ioctl},
  {"ext_recv", (void*)knickknack_ext_recv},
  #endif

  #if KNICKKNACK_ENABLE_THREADS
  {"ext_new_thread", (void*)knickknack_ext_new_thread},
  {"ext_join_thread", (void*)knickknack_ext_join_thread},
  {"ext_hardware_concurrency", (void*)knickknack_ext_hardware_concurrency},
  {"ext_test_and_set", (void*)knickknack_ext_test_and_set},
  {"ext_wait", (void*)knickknack_ext_wait},
  {"ext_notify", (void*)knickknack_ext_notify},
  #endif

  #if defined(__WIN32__) || defined(__linux__)
  {"ext_load_library", (void*)knickknack_ext_load_library}, 
  {"ext_get_function", (void*)knickknack_ext_get_function}, 
  {"ext_get_string_address", (void*)knickknack_ext_get_string_address}, 
  #endif
};
