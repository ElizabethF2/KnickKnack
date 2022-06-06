import "string.k" as string

export x86
export stdcall
export windows
export sockets
export fileio

export std_init
std_init:
  platform = std_platform()
  if_not platform goto _if1
    _is_x86 = string.contains(platform, "x86")
    _is_stdcall = string.contains(platform, "stdcall")
    _is_windows = string.contains(platform, "windows")
    std_destroy(platform)
  _if1:

  _has_sockets = std_builtin_exists("ext_new_socket")
  _has_fileio = std_builtin_exists("ext_open_file")

  return

x86: return _is_x86
stdcall: return _is_stdcall
windows: return _is_windows
sockets: return _has_sockets
fileio: return _has_fileio
