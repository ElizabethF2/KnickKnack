#include "KnickKnack.h"
#include <stdlib.h>

static std::unordered_map<std::string, std::string> code_map;

bool loader_exists(std::string name)
{
  return code_map.find(name) != code_map.end();
}

bool loader_feed(std::string name, knickknack_compiler_feeder* feeder)
{
  if (loader_exists(name))
  {
    for (auto c : code_map[name])
    {
      KNICKKNACK_RETURN_IF_FALSE(feeder->feed_char(c));
    }
    return true;
  }
  return false;
}

void leak_test()
{
  if(g_knickknack_globals)
  {
    knickknack_internal_destroy_shallow(g_knickknack_globals);
    g_knickknack_gc_heap.erase(g_knickknack_globals);
  }

  knickknack_std_gc(true);

  for (auto kv : g_knickknack_script_binaries)
  {
    printf("  !!! SCRIPT LEAK: %s\n", kv.first.c_str());
  }
  for (auto kv : g_knickknack_gc_heap)
  {
    printf("  !!! OBJ LEAK: %d %d\n", kv.first, knickknack_std_kind(kv.first));
  }
  for (auto kv : g_knickknack_deferred_symbols)
  {
    auto s1 = std::get<0>(kv.second);
    auto s2 = std::get<2>(kv.second);
    auto s3 = std::get<3>(kv.second);
    printf("  !!! SYM LEAK: %s %s %s\n", s1, s2, s3);
  }
}

int main(int argc, char* argv[]) {
  knickknack_init();
  if (argc > 1)
  {
    if (argc > 2 && strcmp(argv[1], "-c") == 0)
    {
      code_map["cli"] = argv[2];
      g_knickknack_loader_hook_exists = loader_exists;
      g_knickknack_loader_hook_feed = loader_feed;
      if (knickknack_entry_point_exists("cli", "main"))
      {
        return (int)knickknack_call("cli", "main");
      }
      else
      {
        printf("You must create an exported function called 'main'\n");
        return -1;
      }
    }
    else if (knickknack_script_exists(argv[1]))
    {
      if (knickknack_entry_point_exists(argv[1], "main"))
      {
        auto res = (int)knickknack_call(argv[1], "main");
        leak_test();
        return res;
      }
      else
      {
        printf("Your script must have an exported function called 'main'\n");
        return -1;
      } 
    }
    else
    {
      printf("Can't find '%s'\n", argv[1]);
      return -1;
    }
  }
  printf("Please provide a script file as an argument.\n");
  return -1;
}
