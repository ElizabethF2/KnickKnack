#pragma once

// Detect, validate and store CPU Architecture

#ifndef __arm__
  #ifdef _M_ARM
    #define __arm__
  #endif
#endif

#ifndef __X86__
  #if defined(__i386__) || defined(_M_I86) || defined(_M_IX86)
    #define __X86__
  #endif
#endif

#if   defined(__X86__)
  #define KNICKKNACK_CPU_ARCHITECTURE "x86"
#elif defined(__arm__)
  #define KNICKKNACK_CPU_ARCHITECTURE "arm"
#else
  #error "Unsupported CPU architecture"
#endif



// Validate and store calling convention

#if   defined(KNICKKNACK_CDECL)
  #define KNICKKNACK_CALLING_CONVENTION "cdecl"
  #ifdef __arm__
    #error "CDECL can't be used with ARM"
  #endif
#elif defined(KNICKKNACK_EABI)
  #define KNICKKNACK_CALLING_CONVENTION "eabi"
  #ifdef __X86__
    #error "ARM EABI can't be used with x86"
  #endif
#else
  #error "No calling convention or unsupported calling convention set"
#endif



// Detect and store OS and/or system
// Sets variables related to system capabilities
// Also load any system dependent libraries

#if defined(__WIN32__)
  #define KNICKKNACK_SYSTEM "windows"
  #define KNICKKNACK_HAS_VIRTUAL_MEMORY
  #define KNICKKNACK_SUPPORTS_THREADING
  #define _WIN32_WINNT 0x0A00
  #include <windows.h>
  #include <psapi.h>
#elif defined(__linux__)
  #define KNICKKNACK_SYSTEM "linux"
  #define KNICKKNACK_HAS_VIRTUAL_MEMORY
  #define KNICKKNACK_SUPPORTS_THREADING
  #include <sys/resource.h>
#elif defined(NDS) || defined(ARM9)
  #define KNICKKNACK_SYSTEM "nds"
  #define KNICKKNACK_ARM_WITH_4MB_OR_LESS_RAM
  #include <nds.h>
#elif defined(PSP)
  // XXX PSP support is unimplemented
  #define KNICKKNACK_SYSTEM "psp"
#else
  #define KNICKKNACK_SYSTEM "unknown"
#endif


// Update capability macros
#ifndef KNICKKNACK_SUPPORTS_THREADING
  #undef KNICKKNACK_ENABLE_THREADS
#endif



// Generate the platform string
#define KNICKKNACK_PLATFORM (KNICKKNACK_CPU_ARCHITECTURE " " KNICKKNACK_CALLING_CONVENTION " " KNICKKNACK_SYSTEM)
