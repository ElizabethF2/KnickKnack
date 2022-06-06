#include <stdio.h>

int dummy(int a, int b, int c, int d, int e, int f)
{
  return a + b + c + d + e + f;
}

int dummy2()
{
  return dummy(getchar(),getchar(),getchar(),getchar(),getchar(),getchar());
}

void dummy3()
{
  printf("Hello world! %d\n", 5);
}

int divd(int a, int b)
{
  return a/b;
}

bool check_ds()
{
  #ifdef __arm__
    const bool is_DS = true;
  #else
    const bool is_DS = false;
  #endif
  
  if (is_DS || getchar())
  {
    return true;
  }
  return false;
}