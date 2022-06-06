#pragma once

#include "common.h"
#include "api.h"

#if defined(__WIN32__) || defined(__linux__)
  void knickknack_ext_unload_library(void* data);
  knickknack_object* knickknack_ext_load_library(knickknack_object* name);
  void* knickknack_ext_get_function(knickknack_object* lib, knickknack_object* name);
  void* knickknack_ext_get_string_address(knickknack_object* string);
#endif
