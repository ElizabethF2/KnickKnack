#define _WIN32_WINNT 0x0A00
#include <windows.h>

static SRWLOCK foo = {0};

#include <iostream>
using namespace std;

int main() 
{
  

  AcquireSRWLockExclusive(&foo);
  AcquireSRWLockExclusive(&foo);
  AcquireSRWLockExclusive(&foo);
  
  cout << "Hello, World!";
  return 0;
}