#pragma once

#include "platform.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <tuple>

#define KNICKKNACK_RETURN_IF_FALSE(x) do { if (!x) return false; } while(0)
#define KNICKKNACK_LOG_ERROR(f_, ...) do \
                                      { \
                                        fprintf(g_knickknack_error_destination, f_, __VA_ARGS__); \
                                        fflush(g_knickknack_error_destination); \
                                      } \
                                      while(0)

#define KNICKKNACK_BLOCK_SIZE 1024
#define KNICKKNACK_VALUE_SIZE sizeof(void*)
#define KNICKKNACK_BYTES_PER_GC_CYCLE 5242880 // 5MB

enum class knickknack_kind
{
  UNBOXED_PRIMITIVE = 0,
  PRIMITIVE = 1,
  STRING = 2,
  STRING_LITERAL = 3,
  LIST = 4,
  DICT = 5,
  CAPSULE = 6
};

struct knickknack_object
{
  knickknack_kind kind;
  void* instance;
};

struct knickknack_capsule
{
  void (*destroy)(void* data);
  void* data;
};

struct knickknack_binary
{
  void* buffer;
  void* variable_section;
  void* end;

  std::unordered_map<std::string, void*> exports;
  std::unordered_set<knickknack_object*> literals;
  
  std::unordered_set<std::string> linked_binaries;
  std::vector<void*> deferred_symbols;
  unsigned int refcount;
  bool in_use;
};

class knickknack_compiler_feeder
{
  public:
    virtual bool feed_char(char c) { return false; };
};

extern knickknack_object* g_knickknack_globals;
extern std::unordered_map<std::string, void*> g_knickknack_builtins;
extern std::unordered_map<std::string, knickknack_binary*> g_knickknack_script_binaries;
extern std::unordered_map<void*, std::tuple<knickknack_object*, void*, knickknack_object*, knickknack_object*>> g_knickknack_deferred_symbols;
extern std::vector<std::string> g_knickknack_search_paths;

extern FILE* g_knickknack_error_destination;
extern bool (*g_knickknack_loader_hook_exists)(std::string name);
extern bool (*g_knickknack_loader_hook_feed)(std::string name, knickknack_compiler_feeder* feeder);
extern void (*g_knickknack_gc_hook_cleanup)();

void knickknack_internal_fatal_error();
extern void (*g_knickknack_fatal_error_hook)();
