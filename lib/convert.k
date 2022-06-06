export string_to_int
export int_to_string

string_to_int:; func_args string
  int = 0
  size = std_size(string)
  index = 0
  
  _loop3:
    c = std_get(string, index)
    if c > 57 goto _error1        # 57 = '9'
    if c < 48 goto _error1        # 48 = '0'
    c = c - 48
    int = int * 10
    int = int + c
    index = index + 1
    if index < size goto _loop3
  
  return int
  
  _error1:
  errno = std_error_invalid_arg
  return 0


int_to_string:; func_args int
  string = std_new_string()
  if string == 0 goto _end1
  length = 0

  # special case for zero
  if_not int == 0 goto _if1
    std_append_primitive(string, 48)   # 48 = '0'
    goto _end1
  _if1:

  if_not int < 0 goto _if2
    int = int * -1
    std_append_primitive(string, 45)   # 45 = '-'
  _if2:

  _loop1:
    digit = int % 10
    if int < 1 goto _loop1_break
    push digit
    length = length + 1
    int = int / 10
    goto _loop1
  _loop1_break:

  _loop2:
    pop digit
    digit = digit + 48   # 48 = '0'
    std_append_primitive(string, digit)
    length = length - 1
    if length != 0 goto _loop2
  
  _end1:
  return string
