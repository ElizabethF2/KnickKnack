#include "library.h"

void knickknack_ext_unload_library(void* data)
{
  #if defined(__WIN32__)
    FreeLibrary((HINSTANCE)data);  
  #endif
}

knickknack_object* knickknack_ext_load_library(knickknack_object* name)
{
  knickknack_object* result = nullptr;

  #if defined(__WIN32__)
  if (auto libname = knickknack_get_readonly_string_from_object(name))
  {
    
      auto lib = LoadLibrary(libname->c_str());
      if (lib)
      {
        result = knickknack_new_capsule(knickknack_ext_unload_library, (void*)lib);
      }
  }
  #endif
  
  return result;
}

void* knickknack_ext_get_function(knickknack_object* lib, knickknack_object* name)
{
  #if defined(__WIN32__)
    if (auto library = (HINSTANCE)knickknack_get_data_from_capsule(knickknack_ext_unload_library, lib))
    {
      if (auto funcname = knickknack_get_readonly_string_from_object(name))
      {
        return (void*)GetProcAddress(library, funcname->c_str());
      }
    }
  #endif

  return nullptr;
}

void* knickknack_ext_get_string_address(knickknack_object* string)
{
  if (auto s = knickknack_get_readonly_string_from_object(string))
  {
    return (void*)s->c_str();
  }
  return nullptr;
}
