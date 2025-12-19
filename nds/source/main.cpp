#include <nds.h>
#include "KnickKnack.h"
#include <fat.h>

#include "codemap.h"

bool exist(std::string s){ return s_code_map.find(s) != s_code_map.end(); }

bool load(std::string s, knickknack_compiler_feeder* feeder)
{
  if (exist(s))
  {
    for (auto c : s_code_map[s])
    {
      feeder->feed_char(c);
    }
    
    return true;
  }
  else
  {
    return false;
  }
};

int main(void) 
{
	g_knickknack_loader_hook_exists = exist;
  g_knickknack_loader_hook_feed = load;

  consoleDemoInit();

  // TODO: keyboard

  if (!fatInitDefault())
  {
    iprintf("fatInit failed, no fs access\n");
  }

  auto res = (int)knickknack_call("main.k", "main");
  printf("Done!  Res = %d\n\n\n", res);

  while(1)
  {
    swiWaitForVBlank();
    scanKeys();
    int keys = keysDown();
    if (keys & KEY_START) break;
  }

  return 0;
}
